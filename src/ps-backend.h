#ifndef __CAIRO_PS_BACKEND_H__
#define __CAIRO_PS_BACKEND_H__

#include "backend.h"

#if CAIRO_HAS_PS_SURFACE
#include <cairo-ps.h>
#endif

extern Rcairo_backend_def *RcairoBackendDef_ps;

Rcairo_backend *Rcairo_new_ps_backend(Rcairo_backend *be, int conn, const char *filename, double width, double height);

#endif
