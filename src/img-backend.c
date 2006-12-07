#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>
#include "img-backend.h"

typedef struct st_Rcairo_image_backend {
	void *buf;
	char *filename;
} Rcairo_image_backend;

static void image_backend_destroy(Rcairo_backend* be)
{
	if (be->backendSpecific){
		Rcairo_image_backend *image = (Rcairo_image_backend *)be->backendSpecific;
		if (image->buf) free(image->buf);
		if (image->filename) free(image->filename);
		free(be->backendSpecific);
	}

	cairo_surface_destroy(be->cs);
	cairo_destroy(be->cc);
	free(be);
}

/* static void image_save_page_jpg(Rcairo_backend* be, int pageno){
} */

static void image_save_page_png(Rcairo_backend* be, int pageno){
	Rcairo_image_backend *image = (Rcairo_image_backend *)be->backendSpecific;
	char *fn;
	int   nl;

	fn=(char*) malloc(strlen(image->filename)+16);
	strcpy(fn, image->filename);
	if (pageno>0) sprintf(fn+strlen(fn),"%d",pageno);
	nl = strlen(fn);

	if (nl>3 && strcmp(fn+nl-4,".png")) strcat(fn, ".png");
	cairo_surface_write_to_png(be->cs, fn);

	free(fn);
}

Rcairo_backend *Rcairo_new_image_backend(char *filename, char *type, int width, int height)
{
	Rcairo_backend *be;
	Rcairo_image_backend *image;
	
	if ( ! (be = (Rcairo_backend*) calloc(1,sizeof(Rcairo_backend))))
		return NULL;

	if ( ! (image = (Rcairo_image_backend *)calloc(1,sizeof(Rcairo_image_backend)))){
		free(be); return NULL;
	}

	if ( !(image->filename = malloc(strlen(filename)+1))){
		free(be); free(image); return NULL;
	}

	strcpy(image->filename,filename);

	be->destroy_backend = image_backend_destroy;
	be->backendSpecific = (void *)image;

	if (!strcmp(type,"png24")){

		int stride = 4 * width;

		if ( ! (image->buf = calloc (stride * height, 1))){
			free(be); free(image->filename); free(image);
			return NULL;
		}

		/* What does this do? JRH */
		/* start off with fully transparent image */
		/* memset(image->buf, 0xff, stride * height); */

		be->cs = cairo_image_surface_create_for_data (image->buf,
				CAIRO_FORMAT_ARGB32,
				width, height, stride);
		be->save_page = image_save_page_png;

	} else if (!strcmp(type,"png")){

		be->cs = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
		be->save_page = image_save_page_png;

	} /* else if (!strcmp(image->type,"jpg")) {

		be->cs = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
		be->save_page = image_save_page_jpg;

	} */ /* etc. */

	if (cairo_surface_status(be->cs) != CAIRO_STATUS_SUCCESS){
		if (image->buf) free(image->buf); free(be); free(image->filename); free(image);
		return NULL;
	}

	be->cc = cairo_create(be->cs);

	if (cairo_status(be->cc) != CAIRO_STATUS_SUCCESS){
		if (image->buf) free(image->buf); free(be); free(image->filename); free(image);
		return NULL;
	}

	cairo_set_operator(be->cc,CAIRO_OPERATOR_SOURCE);

	return be;
}

