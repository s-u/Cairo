#include <stdlib.h>
#include <string.h>
#include "ps-backend.h"
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

static char *types_list[] = { "ps", 0 };
static Rcairo_backend_def RcairoBackendDef_ = {
  BET_PS,
  types_list,
  "PostScript",
  CBDF_FILE|CBDF_CONN|CBDF_MULTIPAGE,
  0
};
Rcairo_backend_def *RcairoBackendDef_ps = &RcairoBackendDef_;

static void ps_save_page(Rcairo_backend* be, int pageno){
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

static void ps_backend_destroy(Rcairo_backend* be)
{
	cairo_surface_destroy(be->cs);
	cairo_destroy(be->cc);
	free(be);
}

Rcairo_backend *Rcairo_new_ps_backend(Rcairo_backend *be, int conn, char *filename, double width, double height)
{
	be->backend_type = BET_PS;
	be->destroy_backend = ps_backend_destroy;
	be->save_page = ps_save_page;
	if (filename){
		char *fn = NULL;
		int len = strlen(filename);

		/* Add .ps extension if necessary */
		if (len>3 && strcmp(filename+len-4,".ps")){
			fn = malloc(len + 5);
			if (!fn) { free(be); return NULL; }
			strcpy(fn,filename);
			strcat(fn,".ps");
			filename = fn;
		}
		be->cs = cairo_ps_surface_create(filename,(double)width,(double)height);
		if (fn) free(fn);
	} else {
#ifdef HAVE_RCONN_H
		if ( ! (be->backendSpecific =  calloc(1,sizeof(int)))){
			free(be); return NULL;
		}
		*(int *)be->backendSpecific = conn;
		be->cs = cairo_ps_surface_create_for_stream(send_image_data,(void *)be,(double)width,(double)height);
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
Rcairo_backend_def *RcairoBackendDef_ps = 0;

Rcairo_backend *Rcairo_new_ps_backend(Rcairo_backend *be, int conn, char *filename, double width, double height)
{
        error("cairo library was compiled without PostScript back-end.");
	return NULL;
}
#endif
