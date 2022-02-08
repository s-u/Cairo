/* -*- mode: C; tab-width: 4; c-basic-offset: 4 -*-
   Created 4/6/2007 by Simon Urbanek
   Copyright (C) 2007        Simon Urbanek
   X11 window handling is based on src/modules/devX11.c from the R sources:
   Copyright (C) 1995, 1996  Robert Gentleman and Ross Ihaka
   Copyright (C) 1997--2005  Robert Gentleman, Ross Ihaka and the R Development Core Team

   License: GPL v2 or GPL v3
*/

#include <stdlib.h>
#include <string.h>
#include "xlib-backend.h"

#if CAIRO_HAS_XLIB_SURFACE

static const char *types_list[] = { "x11", 0 };
static Rcairo_backend_def RcairoBackendDef_ = {
	BET_XLIB,
	types_list,
	"X11",
	CBDF_VISUAL,
	0
};
Rcairo_backend_def *RcairoBackendDef_xlib = &RcairoBackendDef_;

#include <R_ext/eventloop.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Intrinsic.h>      /*->    Xlib.h  Xutil.h Xresource.h .. */

typedef struct {
	Rcairo_backend *be; /* back-link */
	Display *display;
	Visual  *visual;
	Window  rootwin;
	Window  window;
	Cursor  gcursor;
	int     screen;
	int     width;
	int     height;
} Rcairo_xlib_data;

static int inclose = 0;
static XContext devPtrContext;

static void ProcessX11Events(void *);
static void handleDisplayEvent(Display *display, XEvent event);

#define X_BELL_VOLUME 0 /* integer between -100 and 100 for the volume
						   of the bell in locator. */

/*---- save page ----*/
static void xlib_save_page(Rcairo_backend* be, int pageno){
	cairo_show_page(be->cc);

	cairo_set_source_rgba(be->cc,1,1,1,1);
	cairo_reset_clip(be->cc);
	cairo_new_path(be->cc);
	cairo_paint(be->cc);
}

/*---- resize ----*/
static void xlib_resize(Rcairo_backend* be, double width, double height){
	Rcairo_xlib_data *xd = (Rcairo_xlib_data *) be->backendSpecific;
	if (xd) {
		xd->width=width;
		xd->height=height;
	}
	be->width=width;
	be->height=height;
	if (be->cs) cairo_xlib_surface_set_size(be->cs, width, height);
	Rcairo_backend_repaint(be);
	if (xd->display) XSync(xd->display, 0);
}

/*---- mode ---- (0=done, 1=busy, 2=locator) */
static void xlib_mode(Rcairo_backend* be, int which){
	if (be->in_replay) return;
	if (which < 1)	{
		Rcairo_xlib_data *xd = (Rcairo_xlib_data *) be->backendSpecific;
		if (xd->display) XSync(xd->display, 0);
	}
}

/*---- locator ----*/
int  xlib_locator(struct st_Rcairo_backend *be, double *x, double *y) {
	XEvent event;
	Rcairo_xlib_data *cd = (Rcairo_xlib_data *) be->backendSpecific;
	Rcairo_xlib_data *cdEvent = NULL;
	XPointer temp;
	int done = 0;
	Display *display = cd->display;

    ProcessX11Events((void*)NULL);    /* discard pending events */
    XSync(display, 1);
    /* handle X events as normal until get a button click in the desired device */
	while (!done /* && displayOpen FIXME! */) {
		XNextEvent(display, &event);
		/* possibly later R_CheckUserInterrupt(); */
		if (event.type == ButtonPress) {
			XFindContext(display, event.xbutton.window,
						 devPtrContext, &temp);
			cdEvent = (Rcairo_xlib_data *) temp;
			if (cdEvent == cd) {
				if (event.xbutton.button == Button1) {
					int useBeep = asLogical(GetOption(install("locatorBell"),
													  R_BaseEnv));
					*x = event.xbutton.x;
					*y = event.xbutton.y;
					/* Make a beep! Was print "\07", but that
					   messes up some terminals. */
                    if(useBeep) XBell(display, X_BELL_VOLUME);
                    XSync(display, 0);
                    done = 1;
                }
                else
                    done = 2;
            }
        }
        else
            handleDisplayEvent(display, event);
    }
    /* if it was a Button1 succeed, otherwise fail */
    return (done == 1);
}

/*---- destroy ----*/
static void xlib_backend_destroy(Rcairo_backend* be)
{
	Rcairo_xlib_data *xd = (Rcairo_xlib_data *) be->backendSpecific;

	cairo_surface_destroy(be->cs);
	cairo_destroy(be->cc);
	
	inclose = 1;
	ProcessX11Events((void*) NULL);
	
	if (xd) {
		XFreeCursor(xd->display, xd->gcursor);
		XDestroyWindow(xd->display, xd->window);
		XSync(xd->display, 0);
	}

	free(be);
}

/*-----------------*/

static int firstInit = 1;

static void Rcairo_init_xlib() {
	if (!firstInit) return;
	devPtrContext = XUniqueContext();
}

typedef struct Rcairo_display_list_s {
	Display *display;
	InputHandler *handler;
	struct Rcairo_display_list_s *next;
} Rcairo_display_list;

static Rcairo_display_list display_list = { 0, 0, 0 };

/* FIXME: this is bad - we need to associate it with a display */
static Atom _XA_WM_PROTOCOLS, protocol;

#include <setjmp.h>

/* Xlib provides no way to detect I/O error, it just exists the process, so
   we use a handler to detect such conditions and prevent it from exiting */
typedef int (x11_io_handler_t)(Display*);

static jmp_buf x11_io_error_jmp;
static Display *x11_io_error_display;

static int x11_safety_handler(Display *display) {
	x11_io_error_display = display;
	longjmp(x11_io_error_jmp, 1);
	return 0;
}

static int ProcessX11DisplayEvents(Display *display)
{
	XEvent event;
	int ok = 0;
	/* setup IOErrorHandler while we process events
	   such that errors don't kill R */
	x11_io_handler_t *last_handler = XSetIOErrorHandler(x11_safety_handler);
	if (setjmp(x11_io_error_jmp) == 0) {
		while (display && XPending(display)) {
			XNextEvent(display, &event);
			handleDisplayEvent(display, event);
		}
		ok = 1;
	}
	XSetIOErrorHandler(last_handler);
	return ok;
}

static void handleDisplayEvent(Display *display, XEvent event) {
	XPointer temp;
	Rcairo_xlib_data *xd = NULL;

	/* fprintf(stderr, "handleDisplayEvent, type=%d, inclose=%d\n", (int) event.type, inclose); */
	if (event.xany.type == Expose) {
		/* fprintf(stderr, "Expose (inclose=%d)\n", inclose); */
		while(XCheckTypedEvent(display, Expose, &event))
			;
		XFindContext(display, event.xexpose.window,
					 devPtrContext, &temp);
		xd = (Rcairo_xlib_data *) temp;
		if (event.xexpose.count == 0) /* sync - some X11 implementations seem to need this */
			XSync(xd->display, 0);
	} else if (event.type == ConfigureNotify) {
		/* fprintf(stderr,"ConfigureNotify (inclose=%d)\n", inclose); */
		while(XCheckTypedEvent(display, ConfigureNotify, &event))
			;
		XFindContext(display, event.xconfigure.window,
					 devPtrContext, &temp);
		xd = (Rcairo_xlib_data *) temp;
		if (xd->width != event.xconfigure.width ||
			xd->height != event.xconfigure.height) {
			/* printf(" - configure %d x %d (vs %d x %d)\n", event.xconfigure.width, event.xconfigure.height, xd->width, xd->height); */
			Rcairo_backend_resize(xd->be, event.xconfigure.width, event.xconfigure.height);
			/* Gobble Expose events; we'll redraw anyway */
			while(XCheckTypedEvent(display, Expose, &event))
				;
		}
	} else if (event.type == DestroyNotify) {
		/* fprintf(stderr,"DestroyNotify (inclose=%d)\n", inclose); */
		XFindContext(display, event.xclient.window,
					 devPtrContext, &temp);
		xd = (Rcairo_xlib_data *) temp;
		Rcairo_backend_kill(xd->be);
	} else if ((event.type == ClientMessage) &&
			   (event.xclient.message_type == _XA_WM_PROTOCOLS))
		/* fprintf(stderr,"ClientMessage (inclose=%d)\n", inclose); */
		if (!inclose && event.xclient.data.l[0] == protocol) {
			XFindContext(display, event.xclient.window,
						 devPtrContext, &temp);
			xd = (Rcairo_xlib_data *) temp;
			Rcairo_backend_kill(xd->be);
		}
}

static void ProcessX11Events(void *foo)
{
	Rcairo_display_list *l = &display_list, *last = 0;
	/* fprintf(stderr, "ProcessX11Events:\n"); */
	while (l && l->display) {
		/* fprintf(stderr, "   display=%p (inclose=%d)\n", l->display, inclose); */
		if (ProcessX11DisplayEvents(l->display)) { /* error -> need to remove */
			removeInputHandler(&R_InputHandlers, l->handler);
			l->display = 0;
			l->handler = 0;
			if (last)
				last->next = l->next;
			Rf_error("X11 fatal IO error: please save work and shut down R");
		} else last = l;
		l = l->next;
	}
	/* fprintf(stderr, "--done ProcessX11Events--\n"); */
}

Rcairo_backend *Rcairo_new_xlib_backend(Rcairo_backend *be, const char *display, double width, double height, double umpl)
{
	Rcairo_xlib_data *xd;

	if ( ! (xd = (Rcairo_xlib_data*) calloc(1,sizeof(Rcairo_xlib_data)))) {
		free(be);
		return NULL;
	}

	be->backend_type = BET_XLIB;
	be->backendSpecific = xd;
	xd->be = be;
	be->destroy_backend = xlib_backend_destroy;
	be->save_page = xlib_save_page;
	be->resize = xlib_resize;
	be->mode = xlib_mode;
	be->locator = xlib_locator;
	be->truncate_rect = 1;

	{
		int depth;
		int blackpixel = 0;
		int whitepixel = 0;
		static XSetWindowAttributes attributes;    /* Window attributes */
		XSizeHints *hint;
		XEvent event;
		char *title = "Cairo display";
		
		if (!display) {
			display=getenv("DISPLAY");
			if (!display) display=":0.0";
		}
	
		xd->display = XOpenDisplay(display);
		if (!xd->display)
			error("Update to open X11 display %s", display);
	
		{ /* we maintain a list of displays we used so far.
			 let's see if we encountered this display already.
			 if not, setup the event handler etc.
		  */
			Rcairo_display_list *l = &display_list;
			while (l->display != xd->display && l->next) l=l->next;
			if (l->display)
				l = l->next = (Rcairo_display_list *)calloc(1, sizeof(Rcairo_display_list));
			if (l->display != xd->display) { /* new display */
				l->display = xd->display;
				l->handler = addInputHandler(R_InputHandlers, ConnectionNumber(xd->display),
											 ProcessX11Events, 71);
				Rcairo_init_xlib();
			}
		}

		xd->screen = DefaultScreen(xd->display);
		{ /* adjust width and height to be in pixels */
			if (be->dpix<=0) { /* is DPI set to auto? then find it out */
				int dwp = DisplayWidth(xd->display,xd->screen);
				int dwr = DisplayWidthMM(xd->display,xd->screen);
				int dhp = DisplayHeight(xd->display,xd->screen);
				int dhr = DisplayHeightMM(xd->display,xd->screen);
				if (dwp && dwr && dhp && dhr) {
					be->dpix = ((double)dwp)/((double)dwr)*25.4;
					be->dpiy = ((double)dhp)/((double)dhr)*25.4;
				}
			}
			if (umpl>0 && be->dpix<=0) {
				warning("cannot determine DPI from the screen, assuming 90dpi");
				be->dpix = 90; be->dpiy = 90;
			}
			if (be->dpiy==0 && be->dpix>0) be->dpiy=be->dpix;
			if (umpl>0) {
				width = width * umpl * be->dpix;
				height = height * umpl * be->dpiy;
				umpl=-1;
			}
			if (umpl!=-1) {
				width *= (-umpl);
				height *= (-umpl);
			}
		}

		xd->rootwin = DefaultRootWindow(xd->display);
		depth = DefaultDepth(xd->display, xd->screen);    
		xd->visual = DefaultVisual(xd->display, xd->screen);
		if (!xd->visual)
			error("Unable to get visual from X11 display %s", display);
		if (xd->visual->class != TrueColor)
			error("Sorry, Cairo Xlib back-end supports true-color displays only.");
	
		{ int d=depth; for (;d;d--) whitepixel=(whitepixel << 1)|1; }
	
		devPtrContext = XUniqueContext();
	
		memset(&attributes, 0, sizeof(attributes));
		attributes.background_pixel = whitepixel;
		attributes.border_pixel = blackpixel;
		attributes.backing_store = Always;
		attributes.event_mask = ButtonPressMask
			| ExposureMask
			| StructureNotifyMask;
		
		hint = XAllocSizeHints();
		hint->x = 10;
		hint->y = 10;
		
		xd->width = hint->width = be->width = width;
		xd->height = hint->height = be->height = height;
		hint->flags  = PPosition | PSize;
	
		xd->window = XCreateSimpleWindow(xd->display,
										 xd->rootwin,
										 hint->x,hint->y,
										 hint->width, hint->height,
										 1,
										 blackpixel,
										 whitepixel);
		if (!xd->window) {
			XFree(hint);
			error("Unable to create X11 window on display %s", display);
		}
		
		XSetWMNormalHints(xd->display, xd->window, hint);
		XFree(hint);
		XChangeWindowAttributes(xd->display, xd->window,
								CWEventMask | CWBackPixel |
								CWBorderPixel | CWBackingStore,
								&attributes);
		
		XStoreName(xd->display, xd->window, title);
		
		xd->gcursor = XCreateFontCursor(xd->display, XC_crosshair);
		XDefineCursor(xd->display, xd->window, xd->gcursor);
		
		{ /* FIXME: this is really display-dependent */
			_XA_WM_PROTOCOLS = XInternAtom(xd->display, "WM_PROTOCOLS", 0);
			protocol = XInternAtom(xd->display, "WM_DELETE_WINDOW", 0);
			XSetWMProtocols(xd->display, xd->window, &protocol, 1);
		}
		
		XSaveContext(xd->display, xd->window, devPtrContext, (XPointer) xd);
		
		XSelectInput(xd->display, xd->window,
					 ExposureMask | ButtonPressMask | StructureNotifyMask);
		XMapWindow(xd->display, xd->window);
		XSync(xd->display, 0);
		while ( XPeekEvent(xd->display, &event),
				!XCheckTypedEvent(xd->display, Expose, &event))
			;
	}

	be->cs = cairo_xlib_surface_create(xd->display, xd->window, xd->visual,
									   (double)width,(double)height);
	
	if (cairo_surface_status(be->cs) != CAIRO_STATUS_SUCCESS){
		free(be);
		return NULL;
	}
	
	be->cc = cairo_create(be->cs);
	
	if (cairo_status(be->cc) != CAIRO_STATUS_SUCCESS){
		free(be);
		return NULL;
	}

	cairo_set_operator(be->cc,CAIRO_OPERATOR_ATOP);

	return be;
}
#else
Rcairo_backend_def *RcairoBackendDef_xlib = 0;

Rcairo_backend *Rcairo_new_xlib_backend(Rcairo_backend *be, const char *display, double width, double height, double umpl)
{
	error("cairo library was compiled without XLIB back-end.");
	return NULL;
}
#endif
