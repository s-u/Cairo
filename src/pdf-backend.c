#include <stdlib.h>
#include <string.h>
#include "pdf-backend.h"
#if defined(HAVE_NEW_CONN_H) || defined(HAVE_RCONN_H)
#include <R.h>
#include <Rinternals.h>
#include <Rdefines.h>
#define R_INTERFACE_PTRS
#include <Rinterface.h>
#include <Rembedded.h>
#include <R_ext/Print.h>
#ifdef HAVE_NEW_CONN_H
#include <R_ext/Connections.h>
#endif
#ifdef HAVE_RCONN_H
#include <R_ext/RConn.h>
#endif
#endif

#if CAIRO_HAS_PDF_SURFACE

static char *types_list[] = { "pdf", 0 };
static Rcairo_backend_def RcairoBackendDef_ = {
  BET_PDF,
  types_list,
  "PDF",
  CBDF_FILE|CBDF_CONN|CBDF_MULTIPAGE,
  0
};
Rcairo_backend_def *RcairoBackendDef_pdf = &RcairoBackendDef_;

static void pdf_save_page(Rcairo_backend* be, int pageno){
	cairo_show_page(be->cc);
}

#if defined(HAVE_NEW_CONN_H) || defined(HAVE_RCONN_H)
static cairo_status_t send_image_data(void *closure, const unsigned char *data, unsigned int length)
{
	Rcairo_backend *be = (Rcairo_backend *)closure;
	Rconnection conn;
	conn = *(Rconnection *)be->backendSpecific;

#if defined(HAVE_NEW_CONN_H)
	if (R_WriteConnection(conn, (void *)data, length))
#else
	if (R_WriteConnection(conn, data, length, 1))
#endif 
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

Rcairo_backend *Rcairo_new_pdf_backend(Rcairo_backend *be, Rconnection conn, const char *filename, double width, double height)
{
	be->backend_type = BET_PDF;
	be->destroy_backend = pdf_backend_destroy;
	be->save_page = pdf_save_page;
	if (filename){
		char *fn = NULL;
		int len = strlen(filename);

		/* Add .pdf extension if necessary */
		if (len>3 && strcmp(filename+len-4,".pdf")){
			fn = malloc(len + 5);
			if (!fn) { free(be); return NULL; }
			strcpy(fn,filename);
			strcat(fn,".pdf");
			filename = fn;
		}
		be->cs = cairo_pdf_surface_create(filename,(double)width,(double)height);
		if (fn) free(fn);
	} else {
#if defined(HAVE_NEW_CONN_H) || defined(HAVE_RCONN_H)
		if ( ! (be->backendSpecific =  calloc(1,sizeof(int)))){
			free(be); return NULL;
		}
		*(Rconnection *)be->backendSpecific = conn;
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
Rcairo_backend_def *RcairoBackendDef_pdf = 0;

Rcairo_backend *Rcairo_new_pdf_backend(Rcairo_backend *be, int conn, const char *filename, double width, double height)
{
	error("cairo library was compiled without PDF back-end.");
	return NULL;
}
#endif
