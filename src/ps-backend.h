#ifndef __CAIRO_PS_BACKEND_H__
#define __CAIRO_PS_BACKEND_H__

#include "backend.h"

#if CAIRO_HAS_PS_SURFACE
#include <cairo-ps.h>
#endif

Rcairo_backend *Rcairo_new_ps_backend(int conn, char *filename, int width, int height);

#endif
