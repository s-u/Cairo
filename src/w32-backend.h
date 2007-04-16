#ifndef __CAIRO_W32_BACKEND_H__
#define __CAIRO_W32_BACKEND_H__

#include "backend.h"

#if CAIRO_HAS_WIN32_SURFACE
#include <cairo-win32.h>
#endif

Rcairo_backend *Rcairo_new_w32_backend(Rcairo_backend *be, char *display, double width, double height, double umpl);

#endif
