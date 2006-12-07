#ifndef __CAIRO_BACKEND_H__
#define __CAIRO_BACKEND_H__

#include <cairo.h>

typedef struct st_Rcairo_backend {
  void *user; /* what's this for? JRH */
  cairo_t          *cc; /* cairo context */
  cairo_surface_t  *cs; /* cairo surface */

  /* cairo_surface_t *(*create_surface)(struct st_Rcairo_backend *be, int width, int height); */
  void (*save_page)(struct st_Rcairo_backend *be, int pageno);
  void (*destroy_backend)(struct st_Rcairo_backend *be);
  int (*locator)(struct st_Rcairo_backend *be, double *x, double *y);
  void *backendSpecific; /* private data for backend use */
} Rcairo_backend;

/* display or something ... */
#endif
