#ifndef __CAIRO_BACKEND_H__
#define __CAIRO_BACKEND_H__

#include <cairo.h>

typedef struct st_Rcairo_backend {
  void *backendSpecific; /* private data for backend use */
  cairo_t          *cc; /* cairo context */
  cairo_surface_t  *cs; /* cairo surface */

  /* cairo_surface_t *(*create_surface)(struct st_Rcairo_backend *be, int width, int height); */
  void (*save_page)(struct st_Rcairo_backend *be, int pageno);
  void (*destroy_backend)(struct st_Rcairo_backend *be);

  /* optional callbacks - must be set to 0 if unsupported */
  int  (*locator)(struct st_Rcairo_backend *be, double *x, double *y);
  void (*activation)(struct st_Rcairo_backend *be, int activate);  /* maps both Activate/Deactivate */
  void (*mode)(struct st_Rcairo_backend *be, int mode);
} Rcairo_backend;

/* display or something ... */
#endif
