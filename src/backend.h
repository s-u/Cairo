#ifndef __CAIRO_BACKEND_H__
#define __CAIRO_BACKEND_H__

/* Cario config from configure */
#include "cconfig.h"

#include <cairo.h>
#include <R.h>
#include <Rinternals.h>
#include <R_ext/GraphicsDevice.h>

typedef struct st_Rcairo_backend {
  /*----- instance variables -----*/
  void *backendSpecific; /* private data for backend use */
  cairo_t          *cc;  /* cairo context */
  cairo_surface_t  *cs;  /* cairo surface */
  NewDevDesc       *dd;  /* device descriptor */

  /*----- back-end global callbacks (=methods) -----*/
  /* cairo_surface_t *(*create_surface)(struct st_Rcairo_backend *be, int width, int height); */
  void (*save_page)(struct st_Rcairo_backend *be, int pageno);
  void (*destroy_backend)(struct st_Rcairo_backend *be);

  /* optional callbacks - must be set to 0 if unsupported */
  int  (*locator)(struct st_Rcairo_backend *be, double *x, double *y);
  void (*activation)(struct st_Rcairo_backend *be, int activate);  /* maps both Activate/Deactivate */
  void (*mode)(struct st_Rcairo_backend *be, int mode);
  void (*resize)(struct st_Rcairo_backend *be, int width, int height);
} Rcairo_backend;

/* implemented in cairotalk but can be used by any back-end to talk to the GD system */
void Rcairo_backend_resize(Rcairo_backend *be, int width, int height); /* won't do anything if resize is not implemented in the back-end */
void Rcairo_backend_repaint(Rcairo_backend *be); /* re-plays the display list */
void Rcairo_backend_kill(Rcairo_backend *be); /* kills the devide */
void Rcairo_backend_init_surface(Rcairo_backend *be); /* initialize a new surface */
#endif
