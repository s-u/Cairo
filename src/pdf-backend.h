#ifndef __CAIRO_PDF_BACKEND_H__
#define __CAIRO_PDF_BACKEND_H__

#include "backend.h"

extern Rcairo_backend_def *RcairoBackendDef_pdf;

#if CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif

Rcairo_backend *Rcairo_new_pdf_backend(Rcairo_backend *be, int conn, const char *filename, double width, double height);

#endif
