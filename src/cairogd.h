#ifndef _DEV_GD_H
#define _DEV_GD_H

#define CAIROGD_VER 0x010200 /* Cairo v1.2-0 */

/* cairo R package config */
#include "cconfig.h"

#include <R.h>
#include <Rinternals.h>
#include <R_ext/GraphicsDevice.h>
#include <R_ext/GraphicsEngine.h>
#include <R_ext/Print.h>
#include <Rgraphics.h>
#include <Rdevices.h>

#include "backend.h"

#include <cairo.h>
#if CAIRO_HAS_FT_FONT
#include <cairo-ft.h>
#include FT_TRUETYPE_IDS_H
#include <fontconfig/fontconfig.h>
#endif

/* the internal representation of a color in this (R) API is RGBa with a=0 meaning transparent and a=255 meaning opaque (hence a means 'opacity'). previous implementation was different (inverse meaning and 0x80 as NA), so watch out. */
#if R_VERSION < 0x20000
#define CONVERT_COLOR(C) ((((C)==0x80000000) || ((C)==-1))?0:(((C)&0xFFFFFF)|((0xFF000000-((C)&0xFF000000)))))
#else
#define CONVERT_COLOR(C) (C)
#endif

typedef struct {
    double cex;				/* Character expansion */
    int lty;				/* Line type */
    double lwd;
    int col;				/* Color */
    int fill;
    int canvas;				/* Canvas */
    int fontface;			/* Typeface */
    int fontsize;			/* Size in points */
    int basefontface;	    /* Typeface */
    int basefontsize;		/* Size in points */

    int windowWidth;		/* Window width (pixels) */
    int windowHeight;		/* Window height (pixels) */
    int resize;				/* Window resized */

  /* --- custom fields --- */
  Rcairo_backend   *cb; /* cairo backend */

  /* These have moved to Rcairo_backend */
  /* cairo_t          *cc; */ /* cairo context */
  /* cairo_surface_t  *cs; */ /* cairo surface */

  int gd_bgcolor;

  int gd_fill, gd_draw; /* current GD colors */
  double gd_ftsize, gd_ftm_ascent, gd_ftm_descent, gd_ftm_width;
  int gd_ftm_char; /*gd_ftm_xxx are font-metric cached values - char specifying the last query */
	
  /* Moved to Rcairo_backend */
  /* char *img_name; */ /* file name prefix */
  /* char img_type[8]; */ /* image type [png/png8/png24/gif] */

  int npages; /* sequence # in case multiple pages are requested */
} CairoGDDesc;

Rboolean  gdd_new_device_driver(DevDesc*, char*, int conn, char *, double, double, double, int);
int       gdd_set_new_device_data(NewDevDesc *dd, double gamma_fac, CairoGDDesc *xd);

#endif

