#ifndef __CAIRO_SVG_BACKEND_H__
#define __CAIRO_SVG_BACKEND_H__

#include "backend.h"

#if CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
#endif

extern Rcairo_backend_def *RcairoBackendDef_svg;

Rcairo_backend *Rcairo_new_svg_backend(Rcairo_backend *be, int conn, const char *filename, double width, double height);

#endif
