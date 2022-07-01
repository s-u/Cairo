#ifndef __JGD_TALK_H__
#define __JGD_TALK_H__

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "cairogd.h"

void Rcairo_setup_gd_functions(NewDevDesc *dd);
Rboolean CairoGD_Open(NewDevDesc *dd, CairoGDDesc *xd, const char *type, int conn, const char *file,
		      double w, double h, double umpl, SEXP aux);

#if USE_CAIRO_FT
void Rcairo_set_font(int i, const char *fcname);
#endif

extern int Rcairo_symbol_font_use_pua;

#endif
