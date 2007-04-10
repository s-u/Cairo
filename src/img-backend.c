/* -*- mode: C; tab-width: 4; c-basic-offset: 4 -*-
   Copyright (C) 2004-2007   Simon Urbanek
   Copyright (C) 2006        Jeffrey Horner
   License: GPL v2 */

#ifdef HAVE_RCONN_H
#include <R.h>
#include <Rinternals.h>
#include <Rdefines.h>
#define R_INTERFACE_PTRS
#include <Rinterface.h>
#include <Rembedded.h>
#include <R_ext/Print.h>
#include <R_ext/RConn.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>
#include "img-backend.h"
#include "img-jpeg.h"
#include "img-tiff.h"

#define default_jpeg_quality 60

typedef struct st_Rcairo_image_backend {
	void *buf;
	char *filename;
	int  conn;
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

/* utility function: create a filename based on the original specification and the pageno
   result must be freed by the caller */
static char *image_filename(Rcairo_backend* be, int pageno) {
	Rcairo_image_backend *image = (Rcairo_image_backend *)be->backendSpecific;
	char *fn;
	int l = strlen(image->filename)+16;

	fn=(char*) malloc(l);
	fn[l-1] = 0;
	snprintf(fn, l-1, image->filename, pageno);
	return fn;
}

static void image_save_page_jpg(Rcairo_backend* be, int pageno){
	Rcairo_image_backend *image = (Rcairo_image_backend *)be->backendSpecific;
	char *fn = image_filename(be, pageno);
	int width = cairo_image_surface_get_width(be->cs);
	int height = cairo_image_surface_get_height(be->cs);
	unsigned char *buf = cairo_image_surface_get_data (be->cs);
	int res = save_jpeg_file(buf, width, height, fn, default_jpeg_quality);
	free(fn);
	if (res == -2)
		error("Sorry, this Cario was compiled without jpeg support.");
	if (res)
		error("Unable to write jpeg file.");
}

static void image_save_page_tiff(Rcairo_backend* be, int pageno){
	Rcairo_image_backend *image = (Rcairo_image_backend *)be->backendSpecific;
	char *fn = image_filename(be, pageno);
	int width = cairo_image_surface_get_width(be->cs);
	int height = cairo_image_surface_get_height(be->cs);
	int stride = cairo_image_surface_get_stride(be->cs);
	unsigned char *buf = cairo_image_surface_get_data (be->cs);
	int res = save_tiff_file(buf, width, height, fn, stride/width);
	free(fn);
	if (res == -2)
		error("Sorry, this Cario was compiled without tiff support.");
	if (res)
		error("Unable to write tiff file.");
}

static void image_save_page_png(Rcairo_backend* be, int pageno){
	Rcairo_image_backend *image = (Rcairo_image_backend *)be->backendSpecific;
	char *fn;
	int   nl;

	fn=image_filename(be, pageno);
	cairo_surface_write_to_png(be->cs, fn);
	free(fn);
}

#ifdef HAVE_RCONN_H
static cairo_status_t send_image_data(void *closure, const unsigned char *data, unsigned int length)
{
	Rcairo_backend *be = (Rcairo_backend *)closure;
	Rcairo_image_backend *image;
	image = (Rcairo_image_backend *)be->backendSpecific;

	if (R_WriteConnection(image->conn, data, length, 1))
		return CAIRO_STATUS_SUCCESS;
	else
		return CAIRO_STATUS_WRITE_ERROR;
}
static void image_send_page(Rcairo_backend* be, int pageno){
			cairo_surface_write_to_png_stream (be->cs, send_image_data , (void *)be);
}
#endif

Rcairo_backend *Rcairo_new_image_backend(int conn, char *filename, char *type, int width, int height)
{
	Rcairo_backend *be;
	Rcairo_image_backend *image;
	int alpha_plane = 0;

	if ( ! (be = (Rcairo_backend*) calloc(1,sizeof(Rcairo_backend))))
		return NULL;

	if ( ! (image = (Rcairo_image_backend *)calloc(1,sizeof(Rcairo_image_backend)))){
		free(be); return NULL;
	}

	if (filename){
		if ( !(image->filename = malloc(strlen(filename)+1))){
			free(be); free(image); return NULL;
		}

		strcpy(image->filename,filename);
	} else {
#ifdef HAVE_RCONN_H
		image->conn = conn;
		be->save_page = image_send_page;
#else
		free(be); free(image); return NULL;
		return NULL;
#endif
	}

	be->destroy_backend = image_backend_destroy;
	be->backendSpecific = (void *)image;

	if (!strcmp(type,"png24")){
		int stride = 4 * width;

		if ( ! (image->buf = calloc (stride * height, 1))){
			free(be); free(image->filename); free(image);
			return NULL;
		}

		/* initialize image (default is 0/0/0/0 due to calloc) 
		   memset(image->buf, 255, stride * height); */

		be->cs = cairo_image_surface_create_for_data (image->buf,
				CAIRO_FORMAT_ARGB32,
				width, height, stride);
		alpha_plane = 1;
		if (!be->save_page) be->save_page = image_save_page_png;

	} else if (!strcmp(type,"png")){

		be->cs = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
		if (!be->save_page) be->save_page = image_save_page_png;

	} else if (!strcmp(type,"jpg") || !strcmp(type,"jpeg")) {
#ifdef SUPPORTS_JPEG
		be->cs = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
		be->save_page = image_save_page_jpg;
#else
		error("Sorry, this Cairo was compiled without jpeg support.");
#endif
	} else if (!strcmp(type,"tif") || !strcmp(type,"tiff")) {
#ifdef SUPPORTS_TIFF
		be->cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
		be->save_page = image_save_page_tiff;
#else
		error("Sorry, this Cairo was compiled without tiff support.");
#endif
	} /* etc. */

	if (cairo_surface_status(be->cs) != CAIRO_STATUS_SUCCESS){
		if (image->buf) free(image->buf); free(be); free(image->filename); free(image);
		return NULL;
	}

	be->cc = cairo_create(be->cs);

	if (cairo_status(be->cc) != CAIRO_STATUS_SUCCESS){
		if (image->buf) free(image->buf); free(be); free(image->filename); free(image);
		return NULL;
	}

	/* Note: back-ends with alpha-plane don't work with ATOP, because cairo doesn't increase
	   the opacity and thus the resulting picture will be fully transparent. OVER doesn't
	   blend alpha, so the result is not as expected, but at least in works on transparent
	   backgrounds. */
	cairo_set_operator(be->cc,alpha_plane?CAIRO_OPERATOR_OVER:CAIRO_OPERATOR_ATOP);
	return be;
}

