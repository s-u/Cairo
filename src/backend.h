#ifndef __CAIRO_BACKEND_H__
#define __CAIRO_BACKEND_H__

#include <cairo.h>

typedef struct st_Rcairo_backend {
  void *user;
  cairo_surface_t *(*create_surface)(struct st_Rcairo_backend *be, int width, int height);
  void (*destroy_surface)(struct st_Rcairo_backend *be);
  int (*locator)(struct st_Rcairo_backend *be, double *x, double *y);
} Rcairo_backend;

/* display or something ... */
#endif
