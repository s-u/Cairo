/* -*- mode: C; tab-width: 4; c-basic-offset: 4 -*-
 *  Cairo-based gd   (C)2004,5,7 Simon Urbanek (simon.urbanek@r-project.org)
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

#define R_CairoGD 1
#include "cairogd.h"
#include "cairotalk.h"
#include "img-backend.h"

double jGDdpiX = 100.0;
double jGDdpiY = 100.0;
double jGDasp  = 1.0;

#ifdef USE_GAMMA
/* actually we use it in cairotalk as well - we may want to do soemthing about it ... */
static SEXP findArg(char *name, SEXP list) {
    SEXP ns = install(name);
    while (list && list!=R_NilValue) {
        if (TAG(list)==ns) return CAR(list);
        list=CDR(list);
    }
    return 0;
}
#endif

/* type: png[24]
   file: file to write to
   width/heigth
   initps: initial PS
   bgcolor: currently only -1 (transparent) and 0xffffff (white) are really supported
   -
   gamma: 0.6
*/
Rboolean Rcairo_new_device_driver(DevDesc *dd_arg, char *type, int conn, char *file,
								  double width, double height, double initps,
								  int bgcolor, int canvas, double umul, double *dpi, SEXP aux)
{
	CairoGDDesc *xd;
	NewDevDesc *dd = (NewDevDesc *) dd_arg;
	
#ifdef JGD_DEBUG
	Rprintf("Rcairo_new_device_driver(\"%s\", \"%s\", %f, %f, %f)\n",type,file,width,height,initps);
#endif
	
	
    /* allocate new device description */
    if (!(xd = (CairoGDDesc*)calloc(1, sizeof(CairoGDDesc))))
		return FALSE;
	
#ifdef USE_MAGIC
	xd->magic = CAIROGD_MAGIC
#endif
    xd->fontface = -1;
    xd->fontsize = -1;
    xd->basefontface = 1;
    xd->basefontsize = initps;
	xd->canvas = canvas;
	xd->bg = bgcolor;
	xd->gamma = 1.0;
#ifdef USE_GAMMA
	{
		SEXP g = findArg("gamma", aux);
		if (g ** g!=R_NilValue) xd->gamma = asReal(g);
	}
#endif
	xd->asp = 1.0;
	if (dpi) {
		xd->dpix = dpi[0];
		xd->dpiy = dpi[1];
		if (xd->dpix>0 && xd->dpiy>0) xd->asp = xd->dpix / xd->dpiy;
	} else {
		xd->dpix = xd->dpiy = 0.0;
	}

	/* ---- fill dd ----- */
    dd->newDevStruct = 1;

    /*	Set up Data Structures. */
    Rcairo_setup_gd_functions(dd);

    dd->left = dd->clipLeft = 0;			/* left */
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

    /* Device capabilities */

    dd->canResizePlot = TRUE; /* opt */
    dd->canChangeFont = TRUE;
    dd->canRotateText = TRUE;
    dd->canResizeText = TRUE;
    dd->canClip = TRUE;
    dd->canHAdj = 2;
    dd->canChangeGamma = FALSE;

    dd->startlty   = LTY_SOLID;
    dd->startfont  = 1;

    dd->deviceSpecific = (void *) xd;

#ifdef JGD_DEBUG
	Rprintf("Cairo-Open, dimensions: %f x %f, umul=%f, dpi=%f/%f\n", width, height, umul, xd->dpix, xd->dpiy);
#endif
	/* open the device */
    if (!CairoGD_Open(dd, xd, type, conn, file, width, height, umul, aux) || !xd->cb) {
		free(xd);
		dd->deviceSpecific = 0;
		return FALSE;
	}

	if (!xd->cb->resize)
		dd->canResizePlot = FALSE;

	/* those were deferred, because they depend on xd and Open may have modified them */
    dd->right   = dd->clipRight = xd->cb->width;	/* right */
    dd->bottom  = dd->clipBottom = xd->cb->height;	/* bottom */
    dd->startps    = xd->basefontsize; /* = initps */
    dd->startcol   = xd->col;
    dd->startfill  = xd->fill;
    dd->startgamma = xd->gamma;

    /* Inches per raster unit */
	if (xd->dpix > 0 && xd->dpiy <= 0) xd->dpiy = xd->dpix;
   	dd->ipr[0] = (xd->dpix > 0)?(1/xd->dpix):(1.0/72.0);
   	dd->ipr[1] = (xd->dpiy > 0)?(1/xd->dpiy):(1.0/72.0);
    dd->asp = (xd->asp > 0)?xd->asp:1.0;

#ifdef JGD_DEBUG
	Rprintf("Cairo-Open, returned: %f x %f, dpi=%f/%f\n", xd->cb->width, xd->cb->height, xd->dpix, xd->dpiy);
	Rprintf("  ipr=%f/%f (1/ipr)=%f/%f\n", dd->ipr[0], dd->ipr[1], 1/dd->ipr[0], 1/dd->ipr[1]);
#endif

    dd->displayListOn = TRUE;

    return(TRUE);
}

SEXP cairo_create_new_device(SEXP args)
{
    NewDevDesc *dev = NULL;
    GEDevDesc *dd;
    
    char *devname="Cairo";
	char *type, *file = NULL;
	double width, height, initps, umul, dpi[2];
	int bgcolor = -1, canvas = -1;
	int conn = -1;

	SEXP v;
	args=CDR(args);
	v=CAR(args); args=CDR(args);
	if (!isString(v) || LENGTH(v)<1) error("output type must be a string");
	PROTECT(v);
	type=CHAR(STRING_ELT(v,0));
	UNPROTECT(1);

	v=CAR(args); args=CDR(args);
	if (isString(v)){
		PROTECT(v);
		file=CHAR(STRING_ELT(v,0));
		UNPROTECT(1);
	} else if (isInteger(v)){
#ifdef HAVE_RCONN_H
		conn = asInteger(v);
#else
		error("file must be a filename. to support writing to a connection, recompile R and Cairo with the R Connection Patch. ");
#endif
	} else {
		error("file must be a filename");
	}


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
	v=CAR(args); args=CDR(args);
	if (!isString(v) && !isInteger(v) && !isLogical(v) && !isReal(v))
		error("invalid color specification for `canvas'");
	canvas = RGBpar(v, 0);
	v=CAR(args); args=CDR(args);
	if (!isNumeric(v)) error("unit multiplier must be a number");
	umul = asReal(v);
	v=CAR(args); args=CDR(args);
	if (!isNumeric(v)) error("dpi must be a number");
	dpi[0] = dpi[1] = asReal(v);	
#ifdef JGD_DEBUG
	Rprintf("type=%s, file=%s, bgcolor=0x%08x, canvas=0x%08x, umul=%f, dpi=%f\n", type, file, bgcolor, canvas, umul, dpi[0]);
#endif
	
    R_CheckDeviceAvailable();

	if (!(dev = (NewDevDesc*)calloc(1, sizeof(NewDevDesc))))
	    return R_NilValue;

	dev->newDevStruct = 1;
	dev->displayList = R_NilValue;

	dev->savedSnapshot = R_NilValue;

	if (!Rcairo_new_device_driver((DevDesc*)(dev), type, conn, file, width, height, initps,
								 bgcolor, canvas, umul, dpi, args))
	{
	    free(dev);
	    error("unable to start device %s", devname);
	}
	
	gsetVar(install(".Device"), mkString(devname), R_NilValue);
	dd = GEcreateDevDesc(dev);
	addDevice((DevDesc*) dd);
	GEinitDisplayList(dd);
#ifdef JGD_DEBUG
	Rprintf("CairoGD> devNum=%d, dd=%x\n", devNumber((DevDesc*) dd), dd);
#endif
	PROTECT(v = allocVector(INTSXP, 1));
	INTEGER(v)[0] = 0;
	/* for some reason both devNumber and deviceNumber return 0, so we have
	   to find the dev # the hard way */
	{
		int nd = NumDevices();
		int i = 0;
		while (i<nd) {
			if ((void*)GetDevice(i)==(void*)dd) {
				INTEGER(v)[0] = 1 + i; break;
			}
   			i++;
		}
	}
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
#endif
	return R_NilValue;
}

/* experimental */
SEXP get_img_backplane(SEXP dev) {
	int devNr = asInteger(dev)-1;
	GEDevDesc *gd = (GEDevDesc*) GetDevice(devNr);
	if (gd) {
		NewDevDesc *dd=gd->dev;
		if (dd) {
			CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
#ifdef USE_MAGIC
			if (xd->magic != CAIROGD_MAGIC)
				error("Not a Cairo device");
#endif
			if(xd && xd->cb) {
				int bet = xd->cb->backend_type;
				switch (bet) {
				case BET_IMAGE:
					{
						Rcairo_image_backend *image = (Rcairo_image_backend*) xd->cb->backendSpecific;
						SEXP l = allocVector(VECSXP, 2);
						unsigned char *data = image->buf;
						cairo_format_t dformat = image->format;
						int width = cairo_image_surface_get_width(xd->cb->cs);
						int height = cairo_image_surface_get_height(xd->cb->cs);
						PROTECT(l);
						{
							SEXP info = allocVector(INTSXP, 3);
							int *iv = INTEGER(info);
							iv[0] = width;
							iv[1] = height;
							iv[2] = dformat;
							SET_VECTOR_ELT(l, 1, info);
						}
						{
							SEXP ref = R_MakeExternalPtr(data, R_NilValue, R_NilValue);
							SET_VECTOR_ELT(l, 0, ref);
						}
						UNPROTECT(1);
						return l;
					}
				}
				error("unsupported backend");
			}
		}
	}
	error("invalid device number");
	return R_NilValue;
}

SEXP ptr_to_raw(SEXP ptr, SEXP off, SEXP len) {
	int o = asInteger(off);
	int l = asInteger(len);
	if (TYPEOF(ptr) != EXTPTRSXP)
		error("ptr argument must be an external pointer");
	{
		unsigned char *data = (unsigned char*) EXTPTR_PTR(ptr);
		if (data) {
			SEXP v = allocVector(RAWSXP, l);
			Rbyte *rc = RAW(v);
			memcpy(rc, data+o, l);
			return v;
		}
	}
	return R_NilValue;
}

SEXP raw_to_ptr(SEXP ptr, SEXP woff, SEXP raw, SEXP roff, SEXP len) {
	int o1 = asInteger(woff);
	int o2 = asInteger(roff);
	int l = asInteger(len);
	if (TYPEOF(ptr) != EXTPTRSXP)
		error("ptr argument must be an external pointer");
	if (TYPEOF(raw) != RAWSXP)
		error("raw argument must be a raw vector");
	{
		unsigned char *data = (unsigned char*) EXTPTR_PTR(ptr);
		Rbyte *rc = RAW(raw);
		memcpy(data+o1, rc+o2, l);
		return ptr;
	}
	return R_NilValue;
}

