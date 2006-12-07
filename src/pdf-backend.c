#include <stdlib.h>
#include <string.h>
#include "pdf-backend.h"

#if CAIRO_HAS_PDF_SURFACE

static void pdf_save_page(Rcairo_backend* be, int pageno){
	cairo_show_page(be->cc);
}

static void pdf_backend_destroy(Rcairo_backend* be)
{
	cairo_surface_destroy(be->cs);
	cairo_destroy(be->cc);
	free(be);
}

Rcairo_backend *Rcairo_new_pdf_backend(char *filename, int width, int height)
{
	Rcairo_backend *be;
	char *be_filename;

	if ( ! (be = (Rcairo_backend*) calloc(1,sizeof(Rcairo_backend))))
		return NULL;

	be->destroy_backend = pdf_backend_destroy;
	be->save_page = pdf_save_page;
	be->cs = cairo_pdf_surface_create(filename,(double)width,(double)height);

	if (cairo_surface_status(be->cs) != CAIRO_STATUS_SUCCESS){
		free(be); free(be_filename);
		return NULL;
	}

	be->cc = cairo_create(be->cs);

	if (cairo_status(be->cc) != CAIRO_STATUS_SUCCESS){
		free(be); free(be_filename);
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
