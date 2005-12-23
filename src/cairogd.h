#ifndef _DEV_GD_H
#define _DEV_GD_H

#define CAIROGD_VER 0x000101 /* Cairo v0.1-1 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef HAVE_GDDCONFIG_H
# include <cairoconfig.h>
#endif

#include <R.h>
#include <Rinternals.h>
#include <R_ext/GraphicsDevice.h>
#include <R_ext/GraphicsEngine.h>
#include <Rgraphics.h>
#include <Rdevices.h>
#include <cairo.h>
#include "backend.h"

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
  cairo_t          *cc; /* cairo context */
  cairo_surface_t  *cs; /* cairo surface */
  Rcairo_backend   *cb; /* cairo backend */

  int gd_bgcolor;

  int gd_fill, gd_draw; /* current GD colors */
  double gd_ftsize, gd_ftm_ascent, gd_ftm_descent, gd_ftm_width;
  int gd_ftm_char; /*gd_ftm_xxx are font-metric cached values - char specifying the last query */
	
  char *img_name; /* file name prefix */
  int img_seq; /* sequence # in case multiple pages are requested */
  char img_type[8]; /* image type [png/png8/png24/gif] */
  char *gd_ftfont; /* path to the current TTF file */
} GDDDesc;

void      setupGDDfunctions(NewDevDesc *dd);
Rboolean  GDD_Open(NewDevDesc *dd, GDDDesc *xd,  char *type, char *file, double w, double h, int bgcolor);
Rboolean  gdd_new_device_driver(DevDesc*, char*, char *, double, double, double, int);
int       gdd_set_new_device_data(NewDevDesc *dd, double gamma_fac, GDDDesc *xd);
GDDDesc * gdd_alloc_device_desc(double ps);

#endif

