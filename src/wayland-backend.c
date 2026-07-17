/* -*- mode: C; tab-width: 4; c-basic-offset: 4 -*- */
/*
 * wayland-backend.c -- native Wayland (wl_shm + xdg-shell) on-screen
 * backend for the "Cairo" R package. No X11 / XWayland dependency.
 *
 * Design:
 *   - be->cs is a PRIVATE cairo image surface (ARGB32); cairotalk.c draws
 *     into it via be->cc and never learns about Wayland. This keeps cs/cc
 *     stable for the device lifetime (cairotalk assumes that) and enables
 *     dev.capture() (CairoGD_Cap only reads back image surfaces).
 *   - Commit = blit cs into a free wl_shm buffer, attach, damage, commit.
 *   - Double-buffered shm pool in one memfd; wl_buffer release marks free.
 *   - The wl_display fd is registered with R's input-handler mechanism so
 *     events dispatch while R idles (same model as the in-tree X11 device).
 *   - Single-threaded. Wayland listener callbacks NEVER longjmp/error() or
 *     rebuild state directly; they set pending flags acted on at a safe
 *     point (wlbe_process_pending) after dispatch returns.
 *
 * The xdg-shell client glue (xdg-shell-client-protocol.h /
 * xdg-shell-protocol.c) is generated at build time by wayland-scanner from
 * the vendored src/xdg-shell.xml (see configure.ac + Makevars.in).
 */

#define _GNU_SOURCE   /* memfd_create, MFD_CLOEXEC */

#include "wayland-backend.h"

#ifdef HAVE_WAYLAND

#include "cairogd.h"

#include <cairo.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <poll.h>

#include <R_ext/eventloop.h>

static const char *types_list[] = { "wayland", 0 };
static Rcairo_backend_def RcairoBackendDef_ = {
	BET_USER,
	types_list,
	"Wayland",
	CBDF_VISUAL,
	0
};
Rcairo_backend_def *RcairoBackendDef_wayland = &RcairoBackendDef_;

#define NBUF 2
#define WL_DEFAULT_DPI 96.0

typedef struct wl_shm_buf {
	struct wl_buffer *buffer;
	unsigned char    *pixels;   /* into the mmap'd pool */
	int               busy;     /* held by the compositor until released */
} wl_shm_buf;

typedef struct Rcairo_wayland_data {
	/* globals */
	struct wl_display    *display;
	struct wl_registry   *registry;
	struct wl_compositor *compositor;
	struct wl_shm        *shm;
	struct xdg_wm_base   *wm_base;
	struct zxdg_decoration_manager_v1 *decoration_manager;
	struct wl_seat       *seat;      /* locator */
	struct wl_pointer    *pointer;

	/* window */
	struct wl_surface    *surface;
	struct xdg_surface   *xdg_surface;
	struct xdg_toplevel  *toplevel;
	struct zxdg_toplevel_decoration_v1 *toplevel_decoration;
	int configured;                  /* first configure received */

	/* buffers */
	int width, height, stride;
	int pool_fd;
	size_t pool_size;
	unsigned char *pool_data;
	struct wl_shm_pool *pool;
	wl_shm_buf buf[NBUF];

	/* R integration */
	InputHandler *input_handler;
	Rcairo_backend *be;              /* back-pointer */

	/* locator state */
	int in_locator;
	int locator_done;                /* 0 pending, 1 click, -1 cancel */
	double loc_x, loc_y;
	double ptr_x, ptr_y;             /* latest surface-local pointer pos */

	/* deferred actions -- set by listeners, acted on at a safe point */
	int pending_w, pending_h;        /* >0 => resize requested */
	int pending_close;               /* window close requested */
} Rcairo_wayland_data;

/* ------------------------------------------------------------------ */
/* shm pool / buffers                                                 */
/* ------------------------------------------------------------------ */

static void buffer_release(void *data, struct wl_buffer *b) {
	wl_shm_buf *sb = (wl_shm_buf*) data;
	sb->busy = 0;
}
static const struct wl_buffer_listener buffer_listener = { buffer_release };

static int create_anon_file(size_t size) {
	int fd = memfd_create("Rcairo-wayland", MFD_CLOEXEC);
	if (fd < 0) return -1;
	if (ftruncate(fd, (off_t) size) < 0) { close(fd); return -1; }
	return fd;
}

static int wlbe_alloc_buffers(Rcairo_wayland_data *w, int width, int height) {
	int stride = width * 4;               /* ARGB8888 */
	size_t sz = (size_t) stride * height * NBUF;
	int i;

	w->pool_fd = create_anon_file(sz);
	if (w->pool_fd < 0) return -1;
	w->pool_data = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED,
						w->pool_fd, 0);
	if (w->pool_data == MAP_FAILED) { close(w->pool_fd); w->pool_fd = -1; return -1; }

	w->pool = wl_shm_create_pool(w->shm, w->pool_fd, (int32_t) sz);
	for (i = 0; i < NBUF; i++) {
		w->buf[i].pixels = w->pool_data + (size_t) i * stride * height;
		w->buf[i].buffer = wl_shm_pool_create_buffer(
			w->pool, (int32_t)((size_t) i * stride * height),
			width, height, stride, WL_SHM_FORMAT_ARGB8888);
		w->buf[i].busy = 0;
		wl_buffer_add_listener(w->buf[i].buffer, &buffer_listener, &w->buf[i]);
	}
	w->width = width; w->height = height;
	w->stride = stride; w->pool_size = sz;
	return 0;
}

static void wlbe_free_buffers(Rcairo_wayland_data *w) {
	int i;
	for (i = 0; i < NBUF; i++)
		if (w->buf[i].buffer) {
			wl_buffer_destroy(w->buf[i].buffer);
			w->buf[i].buffer = NULL;
		}
	if (w->pool)      { wl_shm_pool_destroy(w->pool); w->pool = NULL; }
	if (w->pool_data) { munmap(w->pool_data, w->pool_size); w->pool_data = NULL; }
	if (w->pool_fd >= 0) { close(w->pool_fd); w->pool_fd = -1; }
}

/* ------------------------------------------------------------------ */
/* commit: blit private surface -> free shm buffer -> attach/commit   */
/* ------------------------------------------------------------------ */

static void wlbe_commit(Rcairo_backend *be) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) be->backendSpecific;
	wl_shm_buf *sb = NULL;
	cairo_surface_t *dst;
	cairo_t *cr;
	int i;

	if (!w || !w->configured || !be->cs) return;

	/* honor dev.hold(): suppress on-screen commits until the hold level
	   returns to 0 at dev.flush(). CairoGD_HoldFlush zeroes holdlevel
	   before it calls sync(), so the flush path itself still commits. */
	if (be->dd && be->dd->deviceSpecific &&
		((CairoGDDesc*) be->dd->deviceSpecific)->holdlevel > 0)
		return;

	for (i = 0; i < NBUF; i++)
		if (!w->buf[i].busy) { sb = &w->buf[i]; break; }
	if (!sb) {
		/* both buffers held by the compositor: wait for a release. Block in
		   poll() holding NO Wayland read intent, so an R interrupt (Ctrl-C
		   out of a rapid redraw loop) can't strand a dangling read and
		   deadlock the next dispatch. */
		while (!sb) {
			struct pollfd pfd;
			if (wl_display_dispatch_pending(w->display) == -1) break;
			for (i = 0; i < NBUF; i++)
				if (!w->buf[i].busy) { sb = &w->buf[i]; break; }
			if (sb) break;
			wl_display_flush(w->display);
			pfd.fd = wl_display_get_fd(w->display);
			pfd.events = POLLIN; pfd.revents = 0;
			if (poll(&pfd, 1, 100) < 0) break;   /* EINTR/error: skip commit */
			if ((pfd.revents & POLLIN) && wl_display_dispatch(w->display) == -1) break;
		}
		if (!sb) return;
	}

	cairo_surface_flush(be->cs);
	dst = cairo_image_surface_create_for_data(
		sb->pixels, CAIRO_FORMAT_ARGB32, w->width, w->height, w->stride);
	cr = cairo_create(dst);
	cairo_set_source_surface(cr, be->cs, 0, 0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(dst);

	sb->busy = 1;
	wl_surface_attach(w->surface, sb->buffer, 0, 0);
	wl_surface_damage_buffer(w->surface, 0, 0, w->width, w->height);
	wl_surface_commit(w->surface);
	wl_display_flush(w->display);
}

/* ------------------------------------------------------------------ */
/* backend hooks called from cairotalk.c                              */
/* ------------------------------------------------------------------ */

/* (0=done, 1=busy, 2=locator, -1=after internal replay) */
static void wlbe_mode(Rcairo_backend *be, int mode) {
	if (be->in_replay) return;       /* replay commits once via mode(-1) */
	if (mode < 1) wlbe_commit(be);
}

static void wlbe_sync(Rcairo_backend *be) {
	wlbe_commit(be);                 /* dev.flush() path (holdflush -> 0) */
}

static void wlbe_save_page(Rcairo_backend *be, int pageno) {
	/* on-screen: NewPage clears via cairotalk's background paint; just
	   make the finished page visible. */
	wlbe_commit(be);
}

static void wlbe_activation(Rcairo_backend *be, int active) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) be->backendSpecific;
	if (w && w->toplevel) {
		int devnum = (be->dd) ? ndevNumber(be->dd) + 1 : 0;
		char title[64];
		snprintf(title, sizeof(title), "R Graphics: Device %d (%s)",
				 devnum, active ? "ACTIVE" : "inactive");
		xdg_toplevel_set_title(w->toplevel, title);
		wl_display_flush(w->display);
	}
}

/* resize hook: driven by Rcairo_backend_resize() from a safe point.
   image surfaces cannot resize in place, so recreate cs/cc, rebuild the
   shm buffers, then replay the display list at the new size. */
static void wlbe_resize(Rcairo_backend *be, double width, double height) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) be->backendSpecific;
	int nw = (int)(width + 0.5), nh = (int)(height + 0.5);
	if (nw < 1) nw = 1;
	if (nh < 1) nh = 1;

	wlbe_free_buffers(w);
	if (wlbe_alloc_buffers(w, nw, nh) < 0) return;

	if (be->cc) { cairo_destroy(be->cc); be->cc = NULL; }
	if (be->cs) { cairo_surface_destroy(be->cs); be->cs = NULL; }
	be->cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, nw, nh);
	be->cc = cairo_create(be->cs);
	Rcairo_backend_init_surface(be);

	Rcairo_backend_repaint(be);      /* replays; final mode(-1) commits */
}

static Rboolean wlbe_locator(Rcairo_backend *be, double *x, double *y) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) be->backendSpecific;
	if (!w) return FALSE;
	if (!w->pointer) {               /* seat capabilities may not be in yet */
		wl_display_roundtrip(w->display);
		if (!w->pointer) return FALSE;   /* no pointer device available */
	}
	w->in_locator = 1; w->locator_done = 0;
	/* Nested wait for a click. We must NOT block while holding a Wayland
	   read intent: an R interrupt (Ctrl-C) longjmps out of any blocking
	   call, and a dangling wl_display_prepare_read would deadlock the next
	   wl_display_read_events. So drain the queue, then block in poll()
	   (holding no read intent) with a timeout, dispatching only when the
	   fd is readable. */
	while (!w->locator_done && !w->pending_close) {
		struct pollfd pfd;
		int pr;
		if (wl_display_dispatch_pending(w->display) == -1) break;
		if (w->locator_done || w->pending_close) break;
		wl_display_flush(w->display);
		pfd.fd = wl_display_get_fd(w->display);
		pfd.events = POLLIN; pfd.revents = 0;
		pr = poll(&pfd, 1, 100);
		if (pr < 0) break;                 /* EINTR (R interrupt) or error */
		if (pr > 0 && (pfd.revents & POLLIN) &&
			wl_display_dispatch(w->display) == -1) break;
	}
	w->in_locator = 0;
	if (w->locator_done == 1) { *x = w->loc_x; *y = w->loc_y; return TRUE; }
	return FALSE;               /* right-click, close, or error = cancel */
}

static void wlbe_destroy(Rcairo_backend *be) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) be->backendSpecific;
	if (w) {
		if (w->input_handler)
			removeInputHandler(&R_InputHandlers, w->input_handler);
		wlbe_free_buffers(w);
		if (w->pointer)     wl_pointer_destroy(w->pointer);
		if (w->seat)        wl_seat_destroy(w->seat);
		if (w->toplevel_decoration) zxdg_toplevel_decoration_v1_destroy(w->toplevel_decoration);
		if (w->decoration_manager)  zxdg_decoration_manager_v1_destroy(w->decoration_manager);
		if (w->toplevel)    xdg_toplevel_destroy(w->toplevel);
		if (w->xdg_surface) xdg_surface_destroy(w->xdg_surface);
		if (w->surface)     wl_surface_destroy(w->surface);
		if (w->wm_base)     xdg_wm_base_destroy(w->wm_base);
		if (w->shm)         wl_shm_destroy(w->shm);
		if (w->compositor)  wl_compositor_destroy(w->compositor);
		if (w->registry)    wl_registry_destroy(w->registry);
		if (w->display)     { wl_display_flush(w->display); wl_display_disconnect(w->display); }
		free(w);
		be->backendSpecific = NULL;
	}
	if (be->cc) { cairo_destroy(be->cc); be->cc = NULL; }
	if (be->cs) { cairo_surface_destroy(be->cs); be->cs = NULL; }
	free(be);
}

/* ------------------------------------------------------------------ */
/* safe point: act on deferred requests after dispatch returns        */
/* ------------------------------------------------------------------ */

static void wlbe_process_pending(Rcairo_backend *be) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) be->backendSpecific;
	if (!w) return;

	if (w->pending_close) {
		w->pending_close = 0;
		Rcairo_backend_kill(be);     /* frees be+w; must not touch after */
		return;
	}
	if (w->pending_w > 0 && w->pending_h > 0 &&
		(w->pending_w != w->width || w->pending_h != w->height)) {
		double nw = w->pending_w, nh = w->pending_h;
		w->pending_w = w->pending_h = 0;
		Rcairo_backend_resize(be, nw, nh);
	} else {
		w->pending_w = w->pending_h = 0;
	}
}

/* ------------------------------------------------------------------ */
/* xdg-shell listeners                                                */
/* ------------------------------------------------------------------ */

static void wm_base_ping(void *data, struct xdg_wm_base *b, uint32_t serial) {
	xdg_wm_base_pong(b, serial);
}
static const struct xdg_wm_base_listener wm_base_listener = { wm_base_ping };

static void xdg_surface_configure(void *data, struct xdg_surface *s,
								  uint32_t serial) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) data;
	xdg_surface_ack_configure(s, serial);
	w->configured = 1;
	/* actual buffer/surface rebuild is deferred to wlbe_process_pending;
	   pending_w/h were recorded by toplevel_configure. */
}
static const struct xdg_surface_listener xdg_surface_listener =
	{ xdg_surface_configure };

static void toplevel_configure(void *data, struct xdg_toplevel *t,
							   int32_t width, int32_t height,
							   struct wl_array *states) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) data;
	if (width > 0 && height > 0) { w->pending_w = width; w->pending_h = height; }
}
static void toplevel_close(void *data, struct xdg_toplevel *t) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) data;
	w->pending_close = 1;            /* acted on at the next safe point */
}
static const struct xdg_toplevel_listener toplevel_listener =
	{ toplevel_configure, toplevel_close };

/* ------------------------------------------------------------------ */
/* seat / pointer (locator support)                                   */
/* ------------------------------------------------------------------ */

#define WLBE_BTN_LEFT  0x110     /* linux/input-event-codes.h */
#define WLBE_BTN_RIGHT 0x111

static void ptr_enter(void *data, struct wl_pointer *p, uint32_t serial,
					  struct wl_surface *surf, wl_fixed_t sx, wl_fixed_t sy) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) data;
	w->ptr_x = wl_fixed_to_double(sx);
	w->ptr_y = wl_fixed_to_double(sy);
}
static void ptr_leave(void *data, struct wl_pointer *p, uint32_t serial,
					  struct wl_surface *surf) {}
static void ptr_motion(void *data, struct wl_pointer *p, uint32_t time,
					   wl_fixed_t sx, wl_fixed_t sy) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) data;
	w->ptr_x = wl_fixed_to_double(sx);
	w->ptr_y = wl_fixed_to_double(sy);
}
static void ptr_button(void *data, struct wl_pointer *p, uint32_t serial,
					   uint32_t time, uint32_t button, uint32_t state) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) data;
	if (!w->in_locator || state != WL_POINTER_BUTTON_STATE_PRESSED) return;
	if (button == WLBE_BTN_LEFT) {
		w->loc_x = w->ptr_x; w->loc_y = w->ptr_y; w->locator_done = 1;
	} else {                 /* right/middle/other button = cancel */
		w->locator_done = -1;
	}
}
static void ptr_axis(void *data, struct wl_pointer *p, uint32_t time,
					 uint32_t axis, wl_fixed_t value) {}
static const struct wl_pointer_listener pointer_listener = {
	ptr_enter, ptr_leave, ptr_motion, ptr_button, ptr_axis
	/* wl_seat bound at v1: v5+ frame/axis_* events are never sent (NULL ok) */
};

static void seat_caps(void *data, struct wl_seat *seat, uint32_t caps) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) data;
	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !w->pointer) {
		w->pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(w->pointer, &pointer_listener, w);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && w->pointer) {
		wl_pointer_destroy(w->pointer); w->pointer = NULL;
	}
}
static void seat_name(void *data, struct wl_seat *seat, const char *name) {}
static const struct wl_seat_listener seat_listener = { seat_caps, seat_name };

/* ------------------------------------------------------------------ */
/* registry                                                           */
/* ------------------------------------------------------------------ */

static void registry_global(void *data, struct wl_registry *reg,
							uint32_t name, const char *iface, uint32_t ver) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) data;
	if (!strcmp(iface, wl_compositor_interface.name))
		w->compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
	else if (!strcmp(iface, wl_shm_interface.name))
		w->shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
	else if (!strcmp(iface, xdg_wm_base_interface.name)) {
		w->wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(w->wm_base, &wm_base_listener, w);
	} else if (!strcmp(iface, wl_seat_interface.name)) {
		w->seat = wl_registry_bind(reg, name, &wl_seat_interface, 1);
		wl_seat_add_listener(w->seat, &seat_listener, w);
	} else if (!strcmp(iface, zxdg_decoration_manager_v1_interface.name)) {
		w->decoration_manager =
			wl_registry_bind(reg, name, &zxdg_decoration_manager_v1_interface, 1);
	}
}
static void registry_global_remove(void *d, struct wl_registry *r, uint32_t n) {}
static const struct wl_registry_listener registry_listener =
	{ registry_global, registry_global_remove };

/* ------------------------------------------------------------------ */
/* R event loop integration                                           */
/* ------------------------------------------------------------------ */

static void wlbe_input_handler(void *userData) {
	Rcairo_wayland_data *w = (Rcairo_wayland_data*) userData;
	Rcairo_backend *be = w->be;

	/* canonical non-blocking read: drain queued events, then read the fd */
	while (wl_display_prepare_read(w->display) != 0)
		wl_display_dispatch_pending(w->display);
	wl_display_flush(w->display);
	wl_display_read_events(w->display);
	wl_display_dispatch_pending(w->display);

	wlbe_process_pending(be);        /* safe point (may kill the device) */
}

/* ------------------------------------------------------------------ */
/* constructor                                                        */
/* ------------------------------------------------------------------ */

Rcairo_backend *Rcairo_new_wayland_backend(Rcairo_backend *be,
										   const char *display_name,
										   double width, double height,
										   double umpl) {
	Rcairo_wayland_data *w;
	int iw, ih;

	if (!(w = (Rcairo_wayland_data*) calloc(1, sizeof(Rcairo_wayland_data)))) {
		free(be);
		return NULL;
	}
	w->pool_fd = -1;
	be->backend_type = BET_USER;
	be->backendSpecific = w;
	w->be = be;

	/* on-screen semantics: opaque, snap rects (X11-like look) */
	be->flags |= CDF_OPAQUE;
	be->truncate_rect = 1;

	/* resolve size to device pixels. Wayland has no screen-DPI concept, so
	   use a fixed default unless the caller already set a DPI. Mirrors the
	   umpl handling in the xlib backend. */
	if (be->dpix <= 0) be->dpix = WL_DEFAULT_DPI;
	if (be->dpiy <= 0) be->dpiy = be->dpix;
	if (umpl > 0) {              /* width/height are in inches */
		width  = width  * umpl * be->dpix;
		height = height * umpl * be->dpiy;
		umpl = -1;
	}
	if (umpl != -1) {            /* negative umpl: multiplier to pixels */
		width  *= (-umpl);
		height *= (-umpl);
	}
	iw = (int)(width + 0.5);  if (iw < 1) iw = 1;
	ih = (int)(height + 0.5); if (ih < 1) ih = 1;
	be->width = iw; be->height = ih;

	w->display = wl_display_connect(
		(display_name && *display_name) ? display_name : NULL);
	if (!w->display) { free(w); free(be); return NULL; }

	w->registry = wl_display_get_registry(w->display);
	wl_registry_add_listener(w->registry, &registry_listener, w);
	wl_display_roundtrip(w->display);
	if (!w->compositor || !w->shm || !w->wm_base) {
		wlbe_destroy(be);
		return NULL;
	}

	w->surface = wl_compositor_create_surface(w->compositor);
	w->xdg_surface = xdg_wm_base_get_xdg_surface(w->wm_base, w->surface);
	xdg_surface_add_listener(w->xdg_surface, &xdg_surface_listener, w);
	w->toplevel = xdg_surface_get_toplevel(w->xdg_surface);
	xdg_toplevel_add_listener(w->toplevel, &toplevel_listener, w);
	xdg_toplevel_set_title(w->toplevel, "R Graphics");
	xdg_toplevel_set_app_id(w->toplevel, "org.r-project.R");
	/* ask the compositor to draw the frame (titlebar + close button) when
	   it supports xdg-decoration; degrade to borderless (client-side, i.e.
	   none) when the manager is absent. */
	if (w->decoration_manager) {
		w->toplevel_decoration =
			zxdg_decoration_manager_v1_get_toplevel_decoration(
				w->decoration_manager, w->toplevel);
		zxdg_toplevel_decoration_v1_set_mode(w->toplevel_decoration,
			ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}
	wl_surface_commit(w->surface);
	wl_display_roundtrip(w->display);   /* wait for initial configure */

	if (wlbe_alloc_buffers(w, iw, ih) < 0) {
		wlbe_destroy(be);
		return NULL;
	}

	/* PRIVATE render target, stable for the device lifetime */
	be->cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, iw, ih);
	if (cairo_surface_status(be->cs) != CAIRO_STATUS_SUCCESS) {
		wlbe_destroy(be);
		return NULL;
	}
	be->cc = cairo_create(be->cs);
	if (cairo_status(be->cc) != CAIRO_STATUS_SUCCESS) {
		wlbe_destroy(be);
		return NULL;
	}

	/* hooks */
	be->save_page       = wlbe_save_page;
	be->destroy_backend = wlbe_destroy;
	be->activation      = wlbe_activation;
	be->mode            = wlbe_mode;
	be->sync            = wlbe_sync;
	be->resize          = wlbe_resize;
	be->locator         = wlbe_locator;

	/* paint an initial opaque background and show the window */
	cairo_set_source_rgb(be->cc, 1, 1, 1);
	cairo_paint(be->cc);
	wlbe_commit(be);

	/* let R's idle loop pump Wayland events */
	w->input_handler = addInputHandler(R_InputHandlers,
									   wl_display_get_fd(w->display),
									   wlbe_input_handler, XActivity);
	if (w->input_handler) w->input_handler->userData = w;

	return be;
}

#else  /* !HAVE_WAYLAND */

Rcairo_backend_def *RcairoBackendDef_wayland = 0;

Rcairo_backend *Rcairo_new_wayland_backend(Rcairo_backend *be, const char *display,
										   double width, double height, double umpl) {
	error("Cairo was compiled without the Wayland back-end.");
	return NULL;
}

#endif /* HAVE_WAYLAND */
