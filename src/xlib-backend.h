#ifndef __CAIRO_XLIB_BACKEND_H__
#define __CAIRO_XLIB_BACKEND_H__

#include "backend.h"

#if CAIRO_HAS_XLIB_SURFACE
#include <cairo-xlib.h>
#endif

Rcairo_backend *Rcairo_new_xlib_backend(char *display, int width, int height);

#endif
