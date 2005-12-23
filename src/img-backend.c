#include <stdlib.h>
#include "backend.h"

static cairo_surface_t *image_backend_create_surface(struct Rcairo_backend* be, int width, int height)
{
  int stride = 4 * width;
  unsigned char *buf;
  void *buf = calloc (stride * height, 1);
  
  return cairo_image_surface_create_for_data (buf,
					      CAIRO_FORMAT_ARGB32,
					      width, height, stride);
}

static void image_backend_destroy_surface()
{
}

Rcairo_backend *Rcairo_new_image_backend()
{
  Rcairo_backend *be = (Rcairo_backend*) malloc(sizeof(Rcairo_backend));
  be->user = 0;
  be->create_surface = image_backend_create_surface;
  be->destroy_surface = image_backend_destroy_surface;
  return be;
}

