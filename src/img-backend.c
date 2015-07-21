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

#define default_jpeg_quality 75

static char* img_types[] = {
	"png",
#ifdef SUPPORTS_JPEG
	"jpeg",
#endif
#ifdef SUPPORTS_TIFF
	"tiff",
#endif
	"raster",
	0 };

static Rcairo_backend_def RcairoBackendDef_image_ = {
	BET_IMAGE,
	img_types,
	"image",
	CBDF_FILE|CBDF_CONN,
	0
};

Rcairo_backend_def *RcairoBackendDef_image = &RcairoBackendDef_image_;

static int cairo_op = -1;
SEXP Rcso(SEXP v) {
	cairo_op = asInteger(v);
	return v;
}

static void image_backend_destroy(Rcairo_backend* be)
{
	if (be->backendSpecific){
		Rcairo_image_backend *image = (Rcairo_image_backend *)be->backendSpecific;
		if (image->buf) free(image->buf);
		if (image->filename) free(image->filename);
		free(be->backendSpecific);
		if (image->locator_call && image->locator_call != R_NilValue)
			R_ReleaseObject(image->locator_call);
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
	/* although we don't use alpha, cairo still aligns on 4 so we have to set channels=4 */
	int res = save_jpeg_file(image->buf, width, height, fn, image->quality?image->quality:default_jpeg_quality, 4);
	free(fn);
	if (res == -2)
		error("Sorry, this Cairo was compiled without jpeg support.");
	if (res)
		error("Unable to write jpeg file.");
}

static void image_save_page_tiff(Rcairo_backend* be, int pageno){
	Rcairo_image_backend *image = (Rcairo_image_backend *)be->backendSpecific;
	char *fn = image_filename(be, pageno);
	int width = cairo_image_surface_get_width(be->cs);
	int height = cairo_image_surface_get_height(be->cs);
	cairo_format_t cf = image->format;
	int res = save_tiff_file(image->buf, width, height, fn, (cf==CAIRO_FORMAT_RGB24)?3:4, image->quality);
	free(fn);
	if (res == -2)
		error("Sorry, this Cairo was compiled without tiff support.");
	if (res)
		error("Unable to write tiff file.");
}

static void image_save_page_png(Rcairo_backend* be, int pageno){
	/* Rcairo_image_backend *image = (Rcairo_image_backend *)be->backendSpecific; */
	char *fn;

	fn=image_filename(be, pageno);
	cairo_surface_write_to_png(be->cs, fn);
	free(fn);
}

static void image_save_page_null(Rcairo_backend* be, int pageno) {
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

int  image_locator(struct st_Rcairo_backend *be, double *x, double *y) {
	Rcairo_image_backend *image;
	image = (Rcairo_image_backend *)be->backendSpecific;
	if (image->locator_call && image->locator_call != R_NilValue) {
		SEXP res;
		INTEGER(image->locator_dev)[0] = ndevNumber(be->dd) + 1;
		res = eval(image->locator_call, R_GlobalEnv);
		if (TYPEOF(res) == INTSXP && LENGTH(res) == 2) {
			*x = INTEGER(res)[0];
			*y = INTEGER(res)[1];
			return 1;
		}
		if (TYPEOF(res) == REALSXP && LENGTH(res) == 2) {
			*x = REAL(res)[0];
			*y = REAL(res)[1];
			return 1;
		}
	}
	return 0;
}


Rcairo_backend *Rcairo_new_image_backend(Rcairo_backend *be, int conn, const char *filename, const char *type,
										 int width, int height, int quality, int alpha_plane, SEXP locator_cb)
{
	Rcairo_image_backend *image;
	int stride = 4 * width;

	if ( ! (image = (Rcairo_image_backend *)calloc(1,sizeof(Rcairo_image_backend)))){
		free(be); return NULL;
	}

	/* filename will be irrelevan for raster output */
	if (type && !strcmp(type, "raster")) filename = 0;

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
		be->save_page = image_save_page_null;
#endif
	}

	be->backend_type = BET_IMAGE;
	be->destroy_backend = image_backend_destroy;
	be->locator = image_locator;
	be->backendSpecific = (void *)image;
	be->truncate_rect = 1;
	be->width = width;
	be->height = height;

	if (!strcmp(type,"jpg")) alpha_plane=0; /* forge jpeg to never have alpha */
	if ( ! (image->buf = calloc (stride * height, 1))){
		free(be); free(image->filename); free(image);
		return NULL;
	}
	image->format = alpha_plane ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
	be->cs = cairo_image_surface_create_for_data (image->buf, image->format,
												  width, height, stride);

	if (cairo_surface_status(be->cs) != CAIRO_STATUS_SUCCESS){
		if (image->buf) free(image->buf); free(be); free(image->filename); free(image);
		return NULL;
	}

	if (locator_cb != R_NilValue)
		R_PreserveObject((image->locator_call =
						  lang2(locator_cb,
								(image->locator_dev = allocVector(INTSXP, 1))
								)));
	else
		image->locator_call = R_NilValue;

	if (!strcmp(type,"png") ||!strcmp(type,"png24") ||!strcmp(type,"png32")) {
		if (!alpha_plane)
			be->flags|=CDF_FAKE_BG;
		if (!be->save_page) be->save_page = image_save_page_png;
	} else if (!strcmp(type,"jpg") || !strcmp(type,"jpeg")) {
#ifdef SUPPORTS_JPEG
		image->quality = quality;
		if (!be->save_page) be->save_page = image_save_page_jpg;
		be->flags|=CDF_OPAQUE; /* no transparency at all */
#else
		cairo_surface_destroy(be->cs);
		free(image->buf);
		error("Sorry, this Cairo was compiled without jpeg support.");
#endif
	} else if (!strcmp(type,"tif") || !strcmp(type,"tiff")) {
#ifdef SUPPORTS_TIFF
		image->quality = quality;
		if (!alpha_plane)
			be->flags|=CDF_OPAQUE;
		if (!be->save_page) be->save_page = image_save_page_tiff;
#else
		cairo_surface_destroy(be->cs);
		free(image->buf);
		error("Sorry, this Cairo was compiled without tiff support.");
#endif
	} /* etc. */

	be->cc = cairo_create(be->cs);

	if (cairo_status(be->cc) != CAIRO_STATUS_SUCCESS){
		if (image->buf) free(image->buf); free(be); free(image->filename); free(image);
		return NULL;
	}

	/* Note: back-ends with alpha-plane don't work with ATOP, because cairo doesn't increase
	   the opacity and thus the resulting picture will be fully transparent. */
	cairo_set_operator(be->cc,alpha_plane?CAIRO_OPERATOR_OVER:CAIRO_OPERATOR_ATOP);
	/* debug */
	if (cairo_op != -1) cairo_set_operator(be->cc,cairo_op);
	return be;
}
