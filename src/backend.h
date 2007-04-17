#ifndef __CAIRO_BACKEND_H__
#define __CAIRO_BACKEND_H__

/* Cario config from configure */
#include "cconfig.h"

#include <cairo.h>
#include <R.h>
#include <Rinternals.h>
#include <R_ext/GraphicsDevice.h>

#define CDF_HAS_UI    0x0001  /* backend has UI (e.g. window) */

typedef struct st_Rcairo_backend {
  /*----- instance variables -----*/
  void *backendSpecific; /* private data for backend use */
  cairo_t          *cc;  /* cairo context */
  cairo_surface_t  *cs;  /* cairo surface */
  NewDevDesc       *dd;  /* device descriptor */
  double width, height;  /* size in native units (pixels or pts) */
  int               in_replay; /* set to 1 if it is known where the painting stops
				  and thus the backend doesn't need to perform
				  any synchronization until we're done.
				  if mode is set, it is called ater replay
				  with mode=-1
			       */
  int               truncate_rect; /* set to 1 to truncate rectangle coordinates
				      to integers. Useful with bitmap back-ends. */

  /*----- back-end global variables (capabilities etc.) -----*/
  int  flags; /* see CDF_xxx above */
  double dpix, dpiy;

  /*----- back-end global callbacks (=methods) -----*/
  /* cairo_surface_t *(*create_surface)(struct st_Rcairo_backend *be, int width, int height); */
  void (*save_page)(struct st_Rcairo_backend *be, int pageno);
  void (*destroy_backend)(struct st_Rcairo_backend *be);

  /* optional callbacks - must be set to 0 if unsupported */
  int  (*locator)(struct st_Rcairo_backend *be, double *x, double *y);
  void (*activation)(struct st_Rcairo_backend *be, int activate);  /* maps both Activate/Deactivate */
  void (*mode)(struct st_Rcairo_backend *be, int mode); /* in addition it is called after internal replay with mode -1 */
  void (*resize)(struct st_Rcairo_backend *be, double width, double height);
} Rcairo_backend;

/* implemented in cairotalk but can be used by any back-end to talk to the GD system */
void Rcairo_backend_resize(Rcairo_backend *be, double width, double height); /* won't do anything if resize is not implemented in the back-end */
void Rcairo_backend_repaint(Rcairo_backend *be); /* re-plays the display list */
void Rcairo_backend_kill(Rcairo_backend *be); /* kills the devide */
void Rcairo_backend_init_surface(Rcairo_backend *be); /* initialize a new surface */
#endif
