#include <stdlib.h>
#include <string.h>
#include "backend.h"

typedef struct st_Rcairo_image_backend {
	void *buf;
	char *type;
} Rcairo_image_backend;

static cairo_surface_t *image_backend_create_surface(Rcairo_backend* be, int width, int height)
{
	Rcairo_image_backend *image = (Rcairo_image_backend *)be->priv;
	if (!strcmp(image->type,"png24")){
		int stride = 4 * width;
		image->buf = calloc (stride * height, 1);

		/* start off with fully transparent image */
		/* memset(image->buf, 0xff, stride * height); */

		return cairo_image_surface_create_for_data (image->buf,
				CAIRO_FORMAT_ARGB32,
				width, height, stride);
	} else if (!strcmp(image->type,"png")){
		return cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
	}
	return NULL;
}


static void image_backend_destroy_surface(Rcairo_backend* be)
{
	if (be->priv){
		Rcairo_image_backend *image = (Rcairo_image_backend *)be->priv;
		if (image->buf) free(image->buf);
		if (image->type) free(image->type);
		free(be->priv);
	}
}

Rcairo_backend *Rcairo_new_image_backend(char *type)
{
	Rcairo_backend *be = (Rcairo_backend*) calloc(1,sizeof(Rcairo_backend));
	Rcairo_image_backend *image = (Rcairo_image_backend *)calloc(1,sizeof(Rcairo_image_backend));
	image->type = malloc(strlen(type)+1);
	strcpy(image->type,type);

	be->create_surface = image_backend_create_surface;
	be->destroy_surface = image_backend_destroy_surface;
	be->priv = (void *)image;
	return be;
}

