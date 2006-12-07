/*
 *  Cairo-based gd   (C)2004,5 Simon Urbanek (simon.urbanek@r-project.org)
 *
 *  Parts of this code are based on the X11 device skeleton from the R project
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define R_GDD 1
#include "cairogd.h"
#include "cairotalk.h"

double jGDdpiX = 100.0;
double jGDdpiY = 100.0;
double jGDasp  = 1.0;

/* type: png[24]
   file: file to write to
   width/heigth
   initps: initial PS
   bgcolor: currently only -1 (transparent) and 0xffffff (white) are really supported */
Rboolean gdd_new_device_driver(DevDesc *dd, char *type, char *file,
							   double width, double height, double initps,
							   int bgcolor)
{
	GDDDesc *xd;
	
#ifdef JGD_DEBUG
	printf("gdd_new_device_driver(\"%s\", \"%s\", %f, %f, %f)\n",type,file,width,height,initps);
#endif
	
	
    /* allocate new device description */
    if (!(xd = (GDDDesc*)calloc(1, sizeof(GDDDesc))))
		return FALSE;
	
    xd->fontface = -1;
    xd->fontsize = -1;
    xd->basefontface = 1;
    xd->basefontsize = initps;
	
	if (!GDD_Open((NewDevDesc*)(dd), xd, type, file, width, height, bgcolor)) {
		free(xd);
		return FALSE;
	}
	
	gdd_set_new_device_data((NewDevDesc*)(dd), 0.6, xd);
	
	return TRUE;
}

/**
  This fills the general device structure (dd) with the XGD-specific
  methods/functions. It also specifies the current values of the
  dimensions of the device, and establishes the fonts, line styles, etc.
 */
int gdd_set_new_device_data(NewDevDesc *dd, double gamma_fac, GDDDesc *xd)
{
#ifdef JGD_DEBUG
	printf("gdd_set_new_device_data\n");
#endif
    dd->newDevStruct = 1;

    /*	Set up Data Structures. */
    setupGDDfunctions(dd);

    dd->left = dd->clipLeft = 0;			/* left */
    dd->right = dd->clipRight = xd->windowWidth;	/* right */
    dd->bottom = dd->clipBottom = xd->windowHeight;	/* bottom */
    dd->top = dd->clipTop = 0;			/* top */

    /* Nominal Character Sizes in Pixels */

    dd->cra[0] = 9;
    dd->cra[1] = 14;

    /* Character Addressing Offsets */
    /* These are used to plot a single plotting character */
    /* so that it is exactly over the plotting point */

    dd->xCharOffset = 0.4900;
    dd->yCharOffset = 0.3333;
    dd->yLineBias = 0.1;

    /* Inches per raster unit */

    dd->ipr[0] = 1/jGDdpiX;
    dd->ipr[1] = 1/jGDdpiY;
    dd->asp = jGDasp;

    /* Device capabilities */

    dd->canResizePlot = FALSE;
    dd->canChangeFont = TRUE;
    dd->canRotateText = TRUE;
    dd->canResizeText = TRUE;
    dd->canClip = TRUE;
    dd->canHAdj = 2;
    dd->canChangeGamma = FALSE;

    dd->startps = xd->basefontsize;
    dd->startcol = xd->col;
    dd->startfill = xd->fill;
    dd->startlty = LTY_SOLID;
    dd->startfont = 1;
    dd->startgamma = gamma_fac;

    dd->deviceSpecific = (void *) xd;

    dd->displayListOn = TRUE;

    return(TRUE);
}

SEXP cairo_create_new_device(SEXP args)
{
    NewDevDesc *dev = NULL;
    GEDevDesc *dd;
    
    char *devname="Cairo";
	char *type, *file;
	double width, height, initps;
	int bgcolor = -1;

	SEXP v;
	args=CDR(args);
	v=CAR(args); args=CDR(args);
	if (!isString(v) || LENGTH(v)<1) error("output type must be a string");
	PROTECT(v);
	type=CHAR(STRING_ELT(v,0));
	v=CAR(args); args=CDR(args);
	if (!isString(v) || LENGTH(v)<1) error("file name must be a string");
	PROTECT(v);
	file=CHAR(STRING_ELT(v,0));
	v=CAR(args); args=CDR(args);
	if (!isNumeric(v)) error("`width' must be a number");
	width=asReal(v);
	v=CAR(args); args=CDR(args);
	if (!isNumeric(v)) error("`height' must be a number");
	height=asReal(v);
	v=CAR(args); args=CDR(args);
	if (!isNumeric(v)) error("initial point size must be a number");
	initps=asReal(v);
	v=CAR(args); args=CDR(args);
	if (!isString(v) && !isInteger(v) && !isLogical(v) && !isReal(v))
		error("invalid color specification for `bg'");
	bgcolor = RGBpar(v, 0);
#ifdef JGD_DEBUG
	printf("type=%s, file=%s, bgcolor=0x%08x\n", type, file, bgcolor);
#endif
	
    R_CheckDeviceAvailable();

	if (!(dev = (NewDevDesc*)calloc(1, sizeof(NewDevDesc))))
	    return R_NilValue;

	dev->newDevStruct = 1;
	dev->displayList = R_NilValue;

	dev->savedSnapshot = R_NilValue;

	if (!gdd_new_device_driver((DevDesc*)(dev), type, file, width, height, initps, bgcolor))
	{
	    free(dev);
	    error("unable to start device %s", devname);
	}
	
	UNPROTECT(2); /* file and type strings */
	
	gsetVar(install(".Device"), mkString(devname), R_NilValue);
	dd = GEcreateDevDesc(dev);
	addDevice((DevDesc*) dd);
	GEinitDisplayList(dd);
#ifdef JGD_DEBUG
	printf("XGD> devNum=%d, dd=%x\n", devNumber((DevDesc*) dd), dd);
#endif
    
	PROTECT(v = allocVector(INTSXP, 1));
	INTEGER(v)[0] = 1 + devNumber((DevDesc*) dd);
	UNPROTECT(1);
    return v;
}

void cairo_set_display_param(double *par) {
	jGDdpiX = par[0];
	jGDdpiY = par[1];
	jGDasp  = par[2];
}

void cairo_get_display_param(double *par) {
	par[0] = jGDdpiX;
	par[1] = jGDdpiY;
	par[2] = jGDasp;
}

void gdd_get_version(int *ver) {
	*ver=CAIROGD_VER;
}

SEXP cairo_font_match(SEXP args){
#if CAIRO_HAS_FT_FONT
	SEXP v;
	char *fcname;
	int sort;
	int verbose;

	args = CDR(args);

	/* Get fontname */
	v = CAR(args); args = CDR(args);
	if (!isString(v) || LENGTH(v)<1){
		warning("fontname must be a character vector of length 1\n");
		return R_NilValue;
	}

	fcname = CHAR(STRING_ELT(v,0));

	/* Get sort option */
	v=CAR(args); args=CDR(args);
	if (!isLogical(v) || LENGTH(v)<1){
		warning("sort options must be a logical\n");
		return R_NilValue;
	}
	
	sort = LOGICAL(v)[0];

	/* Get verbose option */
	v=CAR(args); args=CDR(args);
	if (!isLogical(v) || LENGTH(v)<1){
		warning("verbose options must be a logical\n");
		return R_NilValue;
	}

	verbose = LOGICAL(v)[0];

	/* Now search for fonts */
	{
		FcFontSet	*fs;
		FcPattern   *pat;
		FcResult	result;

		if (!FcInit ()) {
			warning ("Can't init font config library\n");
			return R_NilValue;
		}
		pat = FcNameParse ((FcChar8 *)fcname);

		if (!pat){
			warning ("Problem with font config library in FcNameparse\n");
			return R_NilValue;
		}

		FcConfigSubstitute (0, pat, FcMatchPattern);
		FcDefaultSubstitute (pat);
		if (sort) {
			fs = FcFontSort (0, pat, FcTrue, 0, &result);
		} else {
			FcPattern   *match;
			fs = FcFontSetCreate ();
			match = FcFontMatch (0, pat, &result);
			if (match) FcFontSetAdd (fs, match);
		}
		FcPatternDestroy (pat);

		if (fs) {
			int	j;

			for (j = 0; j < fs->nfont; j++) {
				FcChar8	*family;
				FcChar8	*style;
				FcChar8	*file;

				if (FcPatternGetString (fs->fonts[j], FC_FILE, 0, &file) != FcResultMatch)
					file = (FcChar8 *) "<unknown filename>";
				/* else
				   {
				   FcChar8 *slash = (FcChar8 *) strrchr ((char *) file, '/');
				   if (slash)
				   file = slash+1;
				   } */
				if (FcPatternGetString (fs->fonts[j], FC_FAMILY, 0, &family) != FcResultMatch)
					family = (FcChar8 *) "<unknown family>";
				if (FcPatternGetString (fs->fonts[j], FC_STYLE, 0, &style) != FcResultMatch)
					file = (FcChar8 *) "<unknown style>";

				Rprintf ("%d. family: \"%s\", style: \"%s\", file: \"%s\"\n", j+1, family, style, file);
				if (verbose) {
					FcPattern   *vpat;
					FcChar8 *fname;

					fname = FcNameUnparse(fs->fonts[j]);
					if (fname){
						vpat = FcNameParse(fname);
						FcPatternDel(vpat,FC_CHARSET);
						FcPatternDel(vpat,FC_LANG);
						free(fname);
						fname = FcNameUnparse(vpat);
						Rprintf("   \"%s\"\n",fname);
						free(fname);
						FcPatternDestroy (vpat);
					}
				}
			}
			FcFontSetDestroy (fs);
		}
	}
#else 
	warning("the R Cairo package was not installed with font matching capability. Please consider installing the cairo graphics engine (www.cairographics.org) with freetype and fontconfig support");
#endif
	return R_NilValue;
}

SEXP cairo_font_set(SEXP args){
#if CAIRO_HAS_FT_FONT
	SEXP v;
	int i;
	char *font;

	args = CDR(args);

	/* regular font */
	for (i = 0; i < 5; i++){
		v = CAR(args); args = CDR(args);
		if (!isNull(v) && isString(v) && LENGTH(v)==1){
			font = CHAR(STRING_ELT(v,0));
			Rcairo_set_font(i,font);
		}
	}
#else
	warning("the R Cairo package was not installed with fontconfig. Please consider installing the cairo graphics engine (www.cairographics.org) with freetype and fontconfig support");
	return R_NilValue;
#endif
}
