#ifndef __CAIRO_XLIB_BACKEND_H__
#define __CAIRO_XLIB_BACKEND_H__

#include "backend.h"

#if CAIRO_HAS_XLIB_SURFACE
#include <cairo-xlib.h>
#endif

extern Rcairo_backend_def *RcairoBackendDef_xlib;

Rcairo_backend *Rcairo_new_xlib_backend(Rcairo_backend *be, char *display, double width, double height, double umpl);

#endif
