/* -*- mode: C; tab-width: 4; c-basic-offset: 4 -*- */
#ifndef __CAIRO_WAYLAND_BACKEND_H__
#define __CAIRO_WAYLAND_BACKEND_H__

#include "backend.h"

extern Rcairo_backend_def *RcairoBackendDef_wayland;

Rcairo_backend *Rcairo_new_wayland_backend(Rcairo_backend *be, const char *display,
										   double width, double height, double umpl);

#endif
