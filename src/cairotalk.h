#ifndef __JGD_TALK_H__
#define __JGD_TALK_H__

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "cairo.h"

void Rcairo_setup_gd_functions(NewDevDesc *dd);
Rboolean CairoGD_Open(NewDevDesc *dd, CairoGDDesc *xd,  char *type, int conn, char *file,
		      double w, double h, double umpl, SEXP aux);

#ifdef CAIRO_HAS_FT_FONT
void Rcairo_set_font(int i, char *fcname);
#endif

#endif
