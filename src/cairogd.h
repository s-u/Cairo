#ifndef _DEV_GD_H
#define _DEV_GD_H

#define CAIROGD_VER 0x010406 /* Cairo v1.4-6 */

/* cairo R package config */
#include "cconfig.h"

#include "backend.h"
#include <R_ext/Print.h>
#if R_GE_version < 4
#include <Rgraphics.h>
#include <Rdevices.h>
#define BEGIN_SUSPEND_INTERRUPTS
#define END_SUSPEND_INTERRUPTS
#endif

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

#define CAIROGD_MAGIC (0x43614744)

/* for compatibility with older R versions */
#if R_GE_version < 4
#define GEaddDevice(X) addDevice((DevDesc*)(X))
#define GEdeviceNumber(X) devNumber((DevDesc*)(X))
#define GEgetDevice(X) ((GEDevDesc*) GetDevice(X))
#define ndevNumber(X) devNumber((DevDesc*)(X))
#define GEkillDevice(X) KillDevice(X)
#define desc2GEDesc(X) ((DevDesc*) GetDevice(devNumber((DevDesc*) (X))))
#endif

typedef struct {
#ifdef USE_MAGIC
    int magic; /* magic number such that we recognize Cairo */
#endif
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

  /* --- custom fields --- */
  Rcairo_backend   *cb; /* cairo backend */

  /* those are set before Open is invoked such that we can pass
     any number of initial parameters without modifying the code */
  int bg;  /* bg */
  double gamma; /* gamma */
  double dpix, dpiy, asp;

  int gd_fill, gd_draw; /* current GD colors */
  double gd_ftsize, gd_ftm_ascent, gd_ftm_descent, gd_ftm_width;
  int gd_ftm_char; /*gd_ftm_xxx are font-metric cached values - char specifying the last query */
	
  int npages; /* sequence # in case multiple pages are requested */
} CairoGDDesc;

#endif

