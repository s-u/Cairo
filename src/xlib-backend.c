/* Created 4/6/2007 by Simon Urbanek
   Copyright (C) 2007        Simon Urbanek
   X11 window handling is based on src/modules/devX11.c from the R sources:
   Copyright (C) 1995, 1996  Robert Gentleman and Ross Ihaka
   Copyright (C) 1997--2005  Robert Gentleman, Ross Ihaka and the R Development Core Team

   License: GPL v2
*/

#include <stdlib.h>
#include <string.h>
#include "xlib-backend.h"

#if CAIRO_HAS_XLIB_SURFACE

#include <R.h>
#include <R_ext/eventloop.h>
#include <Rdevices.h>
#include <R_ext/GraphicsEngine.h>

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

static void ProcessX11Events(void *);

/*---- save page ----*/
static void xlib_save_page(Rcairo_backend* be, int pageno){
  Rprintf("Cairo.xlib-backend:xlib_save_page %d\n", pageno);
  cairo_show_page(be->cc);
  cairo_set_source_rgba(be->cc,1,1,1,0);
  cairo_reset_clip(be->cc);
  cairo_paint(be->cc);
}

/*---- resize ----*/
static void xlib_resize(Rcairo_backend* be, int width, int height){
  
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
static XContext devPtrContext;

static void Rcairo_init_xlib() {
  if (!firstInit) return;
  devPtrContext = XUniqueContext();
}

typedef struct Rcairo_display_list_s {
  Display *display;
  struct Rcairo_display_list_s *next;
} Rcairo_display_list;

static Rcairo_display_list display_list = { 0, 0 };

/* this is bad - we need to associate it with a display */
static Atom _XA_WM_PROTOCOLS, protocol;

static void ProcessX11DisplayEvents(Display *display)
{
  caddr_t temp;
  XEvent event;
  Rcairo_xlib_data *xd = NULL;
  NewDevDesc *dd = NULL;
  int do_update = 0;
  int devNum = 0;

  while (display && XPending(display)) {
    XNextEvent(display, &event);

    printf("ProcessX11DisplayEvents.type=%d\n", event.xany.type);

    if (event.xany.type == Expose) {
      printf(" - expose\n");
      while(XCheckTypedEvent(display, Expose, &event))
	;
      XFindContext(display, event.xexpose.window,
		   devPtrContext, &temp);
      xd = (Rcairo_xlib_data *) temp;
      if (event.xexpose.count == 0)
	do_update = 1;
    } else if (event.type == ConfigureNotify) {
      while(XCheckTypedEvent(display, ConfigureNotify, &event))
	;
      XFindContext(display, event.xconfigure.window,
		   devPtrContext, &temp);
      xd = (Rcairo_xlib_data *) temp;
      if (xd->width != event.xconfigure.width ||
	  xd->height != event.xconfigure.height)
	do_update = 1;

      /* FIXME: this should actually go to resize */
      xd->width = event.xconfigure.width;
      xd->height = event.xconfigure.height;
      printf(" - configure %d x %d\n", xd->width, xd->height);
      /*
	dd->size(&(dd->left), &(dd->right), &(dd->bottom), &(dd->top),
	dd);
      */

      if (do_update) { /* Gobble Expose events; we'll redraw anyway */
	while(XCheckTypedEvent(display, Expose, &event))
	  ;

	cairo_surface_destroy(xd->be->cs);
	cairo_destroy(xd->be->cc);
	xd->be->cs = cairo_xlib_surface_create(xd->display, xd->window, xd->visual, xd->width, xd->height);
	if (cairo_surface_status(xd->be->cs) != CAIRO_STATUS_SUCCESS){/* FIXME: what do we do? */  }
	xd->be->cc = cairo_create(xd->be->cs);
	if (cairo_status(xd->be->cc) != CAIRO_STATUS_SUCCESS){/* FIXME: what do we do? */ }
	cairo_set_operator(xd->be->cc,CAIRO_OPERATOR_OVER);

	Rcairo_backend_resize(xd->be, xd->width, xd->height);
      }
    } else if ((event.type == ClientMessage) &&
	       (event.xclient.message_type == _XA_WM_PROTOCOLS))
      if (!inclose && event.xclient.data.l[0] == protocol) {
	XFindContext(display, event.xclient.window,
		     devPtrContext, &temp);
	xd = (Rcairo_xlib_data *) temp;
	Rcairo_backend_kill(xd->be);
      }
    
    if (do_update)
      Rcairo_backend_repaint(xd->be);
  }
}

static void ProcessX11Events(void *foo)
{
  Rcairo_display_list *l = &display_list;
  while (l && l->display) {
    ProcessX11DisplayEvents(l->display);
    l = l->next;
  }
}

static int Rcairo_xlib_new_window(Rcairo_xlib_data *xd, char *display,
				   int width, int height) {
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
      addInputHandler(R_InputHandlers, ConnectionNumber(xd->display),
		      ProcessX11Events, 71);
      Rcairo_init_xlib();
    }
  }

  xd->screen = DefaultScreen(xd->display);
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
  xd->width = hint->width = width;
  xd->height = hint->height= height;
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

  XSaveContext(xd->display, xd->window, devPtrContext, (caddr_t) xd);

  XSelectInput(xd->display, xd->window,
               ExposureMask | ButtonPressMask | StructureNotifyMask);
  XMapWindow(xd->display, xd->window);
  XSync(xd->display, 0);
  while ( XPeekEvent(xd->display, &event),
          !XCheckTypedEvent(xd->display, Expose, &event))
    ;

  return 0;
}

Rcairo_backend *Rcairo_new_xlib_backend(char *display, int width, int height)
{
	Rcairo_backend *be;
	Rcairo_xlib_data *xd;

	if ( ! (be = (Rcairo_backend*) calloc(1,sizeof(Rcairo_backend))))
		return NULL;
	if ( ! (xd = (Rcairo_xlib_data*) calloc(1,sizeof(Rcairo_xlib_data)))) {
	  free(be);
	  return NULL;
	}

	be->backendSpecific = xd;
	xd->be = be;
	be->destroy_backend = xlib_backend_destroy;
	be->save_page = xlib_save_page;
	be->resize = xlib_resize;

	if (Rcairo_xlib_new_window(xd, display, width, height)) {
	  free(be); free(xd);
	  error("Unable to create X11 windows");
	  return NULL;
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

	cairo_set_operator(be->cc,CAIRO_OPERATOR_OVER);

	return be;
}
#else
Rcairo_backend *Rcairo_new_xlib_backend(char *filename, int width, int height)
{
	error("cairo library was compiled without XLIB back-end.");
	return NULL;
}
#endif
