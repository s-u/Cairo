#ifndef __IMAGE_BACKEND_H__
#define __IMAGE_BACKEND_H__

#include "backend.h"

/* usually we don't expose this, but the experimental
   image backplane API needs this so that we don't have
   to use cairo 1.2 API */
typedef struct st_Rcairo_image_backend {
  void *buf;
  char *filename;
  int  conn;
  int  quality;
  cairo_format_t format;
  SEXP locator_call, locator_dev;
} Rcairo_image_backend;


extern Rcairo_backend_def *RcairoBackendDef_image;

Rcairo_backend *Rcairo_new_image_backend(Rcairo_backend *be, int conn, const char *filename, const char *type,
					 int width, int height, int quality, int alpha_plane, SEXP locator_cb);

#endif
