#include <stdlib.h>
#include <string.h>
#include "pdf-backend.h"
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

#if CAIRO_HAS_PDF_SURFACE

static void pdf_save_page(Rcairo_backend* be, int pageno){
	cairo_show_page(be->cc);
}

#ifdef HAVE_RCONN_H
static cairo_status_t send_image_data(void *closure, const unsigned char *data, unsigned int length)
{
	Rcairo_backend *be = (Rcairo_backend *)closure;
	int conn;
	conn = (int)be->backendSpecific;

	if (R_WriteConnection(conn, data, length, 1))
		return CAIRO_STATUS_SUCCESS;
	else
		return CAIRO_STATUS_WRITE_ERROR;
}
#endif

static void pdf_backend_destroy(Rcairo_backend* be)
{
	cairo_surface_destroy(be->cs);
	cairo_destroy(be->cc);
	free(be);
}

Rcairo_backend *Rcairo_new_pdf_backend(int conn, char *filename, int width, int height)
{
	Rcairo_backend *be;

	if ( ! (be = (Rcairo_backend*) calloc(1,sizeof(Rcairo_backend))))
		return NULL;

	be->destroy_backend = pdf_backend_destroy;
	be->save_page = pdf_save_page;
	if (filename){
		be->cs = cairo_pdf_surface_create(filename,(double)width,(double)height);
	} else {
#ifdef HAVE_RCONN_H
		if ( ! (be->backendSpecific =  calloc(1,sizeof(int)))){
			free(be); return NULL;
		}
		*(int *)be->backendSpecific = conn;
		be->cs = cairo_pdf_surface_create_for_stream(send_image_data,(void *)be,(double)width,(double)height);
#else
		free(be); return NULL;
#endif
	}

	if (cairo_surface_status(be->cs) != CAIRO_STATUS_SUCCESS){
		free(be);
		return NULL;
	}

	be->cc = cairo_create(be->cs);

	if (cairo_status(be->cc) != CAIRO_STATUS_SUCCESS){
		free(be);
		return NULL;
	}

	cairo_set_operator(be->cc,CAIRO_OPERATOR_OVER);

	return be;
}
#else
Rcairo_backend *Rcairo_new_pdf_backend(char *filename, int width, int height)
{
	return NULL;
}
#endif
