#ifndef __CAIRO_BACKEND_H__
#define __CAIRO_BACKEND_H__

#include <cairo.h>

typedef struct st_Rcairo_backend {
  void *user;
  cairo_surface_t *(create_surface)(struct Rcairo_backend* be, int width, int height);
  void (destroy_surface)(struct Rcairo_backend* be);
} Rcairo_backend;

#endif
