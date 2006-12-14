#ifndef __JGD_TALK_H__
#define __JGD_TALK_H__

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "cairo.h"

extern void setupGDDfunctions(NewDevDesc *dd);
extern void      setupGDDfunctions(NewDevDesc *dd);
extern Rboolean  GDD_Open(NewDevDesc *dd, GDDDesc *xd,  char *type, int conn, char *file, double w, double h, int bgcolor);

#ifdef CAIRO_HAS_FT_FONT
extern void Rcairo_set_font(int i, char *fcname);
#endif

#endif
