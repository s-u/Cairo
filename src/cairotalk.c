/* -*- mode: C; tab-width: 4; c-basic-offset: 4 -*- */
#include "backend.h"
#include "cairogd.h"
#include "cairotalk.h"
#include "cairobem.h"
#include "img-backend.h"
#include "pdf-backend.h"
#include "svg-backend.h"
#include "ps-backend.h"
#include "xlib-backend.h"
#include "w32-backend.h"
#include "img-tiff.h" /* for TIFF_COMPR_LZW */
#include <Rversion.h>

/* Device Driver Actions */

#if R_VERSION < 0x10900
#error This device needs at least R version 1.9.0
#endif

#define CREDC(C) (((unsigned int)(C))&0xff)
#define CGREENC(C) ((((unsigned int)(C))&0xff00)>>8)
#define CBLUEC(C) ((((unsigned int)(C))&0xff0000)>>16)
#define CALPHA(C) ((((unsigned int)(C))&0xff000000)>>24)

static void CairoGD_Activate(NewDevDesc *dd);
static void CairoGD_Circle(double x, double y, double r,
			  R_GE_gcontext *gc,
			  NewDevDesc *dd);
static void CairoGD_Clip(double x0, double x1, double y0, double y1,
			NewDevDesc *dd);
static void CairoGD_Close(NewDevDesc *dd);
static void CairoGD_Deactivate(NewDevDesc *dd);
static Rboolean CairoGD_Locator(double *x, double *y, NewDevDesc *dd);
static void CairoGD_Line(double x1, double y1, double x2, double y2,
			R_GE_gcontext *gc,
			NewDevDesc *dd);
static void CairoGD_MetricInfo(int c, 
			      R_GE_gcontext *gc,
			      double* ascent, double* descent,
			      double* width, NewDevDesc *dd);
static void CairoGD_Mode(int mode, NewDevDesc *dd);
static void CairoGD_NewPage(R_GE_gcontext *gc, NewDevDesc *dd);
static void CairoGD_Polygon(int n, double *x, double *y,
			   R_GE_gcontext *gc,
			   NewDevDesc *dd);
static void CairoGD_Polyline(int n, double *x, double *y,
			     R_GE_gcontext *gc,
			     NewDevDesc *dd);
static void CairoGD_Rect(double x0, double y0, double x1, double y1,
			 R_GE_gcontext *gc,
			 NewDevDesc *dd);
static void CairoGD_Size(double *left, double *right,
			 double *bottom, double *top,
			 NewDevDesc *dd);
static double CairoGD_StrWidth(char *str, 
			       R_GE_gcontext *gc,
			       NewDevDesc *dd);
static void CairoGD_Text(double x, double y, char *str,
			 double rot, double hadj,
			 R_GE_gcontext *gc,
			 NewDevDesc *dd);

/* fake mbcs support for old R versions */
#if R_GE_version < 4
#define mbcslocale 0
#define Rf_ucstoutf8(X,Y)
#define AdobeSymbol2utf8(A,B,W) { A[0]=s[0]; A[1]=0; }
#endif

#ifdef JGD_DEBUG
#define Rcairo_set_source_rgb(C,R,G,B) { Rprintf("S:RGB> %f %f %f\n", R, G, B); cairo_set_source_rgb(C,R,G,B); }
#define Rcairo_set_source_rgba(C,R,G,B,A) { Rprintf("S:RGBA> %f %f %f (%f)\n", R, G, B, A); cairo_set_source_rgba(C,R,G,B,A); }
#define Rcairo_set_color(cc, col) { if (CALPHA(col)==255) { Rcairo_set_source_rgb (cc, ((double)CREDC(col))/255., ((double)CGREENC(col))/255., ((double)CBLUEC(col))/255.); } else { Rcairo_set_source_rgba (cc, ((double)CREDC(col))/255., ((double)CGREENC(col))/255., ((double)CBLUEC(col))/255., ((double)CALPHA(col))/255.); }; }
#else
#define Rcairo_set_color(cc, col) { if (CALPHA(col)==255) { cairo_set_source_rgb (cc, ((double)CREDC(col))/255., ((double)CGREENC(col))/255., ((double)CBLUEC(col))/255.); } else { cairo_set_source_rgba (cc, ((double)CREDC(col))/255., ((double)CGREENC(col))/255., ((double)CBLUEC(col))/255., ((double)CALPHA(col))/255.); }; }
#endif

#ifdef CAIRO_HAS_FT_FONT
FT_Library Rcairo_ft_library = NULL;

typedef struct {
	cairo_font_face_t *face;
	int updated;
} Rcairo_font_face;

Rcairo_font_face Rcairo_fonts[5] = {
	{ NULL, 0 },
	{ NULL, 0 },
	{ NULL, 0 },
	{ NULL, 0 },
	{ NULL, 0 }
};

cairo_font_face_t *Rcairo_set_font_face(int i, const char *file){
	FT_Face face;
	FT_Error er;
	FT_CharMap found = 0; 
	FT_CharMap charmap; 
	int n; 

	/* Ensure that freetype library is ready */
	if (!Rcairo_ft_library){
		if (FT_Init_FreeType(&Rcairo_ft_library)){
			error("Failed to initialize freetype library in Rcairo_set_font_face!\n");
			return FALSE;
		}
	}

	er = FT_New_Face(Rcairo_ft_library, file, 0, &face);
	if ( er == FT_Err_Unknown_File_Format ) { 
		error("Unsupported font file format\n");
		return NULL;
	} else if ( er ) {
		error("Unknown font problem\n");
		return NULL;
	}
	for ( n = 0; n < face->num_charmaps; n++ ) { 
		charmap = face->charmaps[n]; 
		if ( charmap->platform_id == TT_PLATFORM_MACINTOSH) { 
			found = charmap; 
			break; 
		} 
	}

	/* Only do this for symbol font */
	if (found && i == 4){
		er = FT_Set_Charmap( face, found );
	} 

	return cairo_ft_font_face_create_for_ft_face(face,FT_LOAD_DEFAULT);
}

void Rcairo_set_font(int i, const char *fcname){
	FcFontSet	*fs;
	FcPattern   *pat, *match;
	FcResult	result;
	FcChar8	*file;
	int j;

	if (Rcairo_fonts[i].face != NULL){
		cairo_font_face_destroy(Rcairo_fonts[i].face);
		Rcairo_fonts[i].face = NULL;
	}

	pat = FcNameParse((FcChar8 *)fcname);
	if (!pat){
		error("Problem with font config library in Rcairo_set_font\n");
		return;
	}
	FcConfigSubstitute (0, pat, FcMatchPattern);
	FcDefaultSubstitute (pat);

	fs = FcFontSetCreate ();
	match = FcFontMatch (0, pat, &result);
	FcPatternDestroy (pat);
	if (match) {
		FcFontSetAdd (fs, match);
	} else {
		error("No font found in Rcairo_set_font");
		FcFontSetDestroy (fs);
		return;
	}

	/* should be at least one font face in fontset */
	if (fs) {

		for (j = 0; j < fs->nfont; j++) {

			/* Need to make sure a real font file exists */
			if (FcPatternGetString (fs->fonts[j], FC_FILE, 0, &file) == FcResultMatch){
				Rcairo_fonts[i].face = Rcairo_set_font_face(i,(const char *)file);
				break;
			}
		}
		FcFontSetDestroy (fs);
		Rcairo_fonts[i].updated = 1;
	} else {
		error("No font found Rcairo_set_font");
	}

}
#endif


static void Rcairo_setup_font(CairoGDDesc* xd, R_GE_gcontext *gc) {
	cairo_t *cc = xd->cb->cc;

#ifdef CAIRO_HAS_FT_FONT
	int i = gc->fontface - 1;

	if (i < 0 || i >= 5){
		error("font %d not recognized in Rcairo_setup_font\n",i);
		return;
	}

	if (Rcairo_fonts[i].updated || (xd->fontface != gc->fontface)){
		cairo_set_font_face(cc,Rcairo_fonts[i].face);
		Rcairo_fonts[i].updated = 0;
#ifdef JGD_DEBUG
		  Rprintf("  font face changed to \"%d\" %fpt\n", gc->fontface, gc->cex*gc->ps + 0.5);
#endif
	} 

	xd->fontface = gc->fontface;

#else
  char *Cfontface="Helvetica";
  int slant = CAIRO_FONT_SLANT_NORMAL;
  int wght  = CAIRO_FONT_WEIGHT_NORMAL;
  if (gc->fontface==5) Cfontface="Symbol";
  if (gc->fontfamily[0]) Cfontface=gc->fontfamily;
  if (gc->fontface==2 || gc->fontface==4) slant=CAIRO_FONT_SLANT_ITALIC;
  if (gc->fontface==3 || gc->fontface==4) wght=CAIRO_FONT_WEIGHT_BOLD;
  
  cairo_select_font_face (cc, Cfontface, slant, wght);

#ifdef JGD_DEBUG
  Rprintf("  font \"%s\" %fpt (slant:%d, weight:%d)\n", Cfontface, gc->cex*gc->ps + 0.5, slant, wght);
#endif
#endif

  /* Add 0.5 per devX11.c in R src. We want to match it's png output as close
   * as possible. */
  cairo_set_font_size (cc, gc->cex * gc->ps + 0.5);
}

static void Rcairo_set_line(CairoGDDesc* xd, R_GE_gcontext *gc) {
	cairo_t *cc = xd->cb->cc;
	R_GE_lineend lend = CAIRO_LINE_CAP_SQUARE;
	R_GE_linejoin ljoin = CAIRO_LINE_JOIN_ROUND;
	
	/* Line width: par lwd  */
	cairo_set_line_width(cc, gc->lwd);

	/* Line end: par lend  */
	switch(gc->lend){
		case GE_ROUND_CAP: lend = CAIRO_LINE_CAP_ROUND; break;
		case GE_BUTT_CAP: lend = CAIRO_LINE_CAP_BUTT; break;
		case GE_SQUARE_CAP: lend = CAIRO_LINE_CAP_SQUARE; break;
	}
	cairo_set_line_cap(cc,lend);

	/* Line join: par ljoin */
	switch(gc->ljoin){
		case GE_ROUND_JOIN: ljoin = CAIRO_LINE_JOIN_ROUND; break;
		case GE_MITRE_JOIN: ljoin = CAIRO_LINE_JOIN_MITER; break;
		case GE_BEVEL_JOIN: ljoin = CAIRO_LINE_JOIN_BEVEL; break;
	} 
	cairo_set_line_join(cc,ljoin);

	if (gc->lty==0 || gc->lty==-1)
		cairo_set_dash(cc,0,0,0);
	else {
		double ls[16]; /* max 16x4=64 bit */
		int l=0, dt=gc->lty;
		while (dt>0) {
			ls[l]=(double)(dt&15);
			dt>>=4;
			l++;
		}
		cairo_set_dash(cc, ls, l, 0);
	}
}

/*------- the R callbacks begin here ... ------------------------*/

static void CairoGD_Activate(NewDevDesc *dd)
{
	CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
	if(!xd || !xd->cb) return;
	if (xd->cb->activation) xd->cb->activation(xd->cb, 1);
}

static void CairoGD_Circle(double x, double y, double r,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
	CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
	if(!xd || !xd->cb) return;
	{
		cairo_t *cc = xd->cb->cc;

#ifdef JGD_DEBUG
		Rprintf("circle %x %f/%f %f [%08x/%08x]\n",cc, x, y, r, gc->col, gc->fill);
#endif

		cairo_new_path(cc);
		cairo_arc(cc, x, y, r + 0.5 , 0., 2 * M_PI); /* Add 0.5 like devX11 */
		if (CALPHA(gc->fill)) {
			Rcairo_set_color(cc, gc->fill);
			cairo_fill_preserve(cc);
		}
		if (CALPHA(gc->col) && gc->lty!=-1) {
			Rcairo_set_color(cc, gc->col);
			Rcairo_set_line(xd, gc);
			cairo_stroke(cc);
		} else cairo_new_path(cc);
	}
}

static void CairoGD_Clip(double x0, double x1, double y0, double y1,  NewDevDesc *dd)
{
    CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
	cairo_t *cc;
    if(!xd || !xd->cb) return;

	cc = xd->cb->cc;
    if (x1<x0) { double h=x1; x1=x0; x0=h; };
    if (y1<y0) { double h=y1; y1=y0; y0=h; };

    cairo_reset_clip(cc);
    cairo_new_path(cc);
    cairo_rectangle(cc, x0, y0, x1-x0 + 1, y1-y0 + 1); /* Add 1 per newX11_Clip */
    cairo_clip(cc);

#ifdef JGD_DEBUG
    Rprintf("clipping %f/%f %f/%f\n", x0, y0, x1, y1);
#endif
}

static void CairoGD_Close(NewDevDesc *dd)
{
  CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
  if(!xd || !xd->cb) return;
  
  xd->cb->save_page(xd->cb,xd->npages);
  xd->cb->destroy_backend(xd->cb);

  free(xd);
  dd->deviceSpecific=NULL;
}

static void CairoGD_Deactivate(NewDevDesc *dd)
{
    CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
    if(!xd || !xd->cb) return;
	if (xd->cb->activation) xd->cb->activation(xd->cb, 0);
}

static Rboolean CairoGD_Locator(double *x, double *y, NewDevDesc *dd)
{
    CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
    if(!xd || !xd->cb) return FALSE;
    if (xd->cb && xd->cb->locator) return xd->cb->locator(xd->cb, x, y);
    return FALSE;
}

static void CairoGD_Line(double x1, double y1, double x2, double y2,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
    if(!xd || !xd->cb) return;
    
#ifdef JGD_DEBUG
    Rprintf("line %f/%f %f/%f [%08x/%08x]\n", x1, y1, x2, y2, gc->col, gc->fill);
#endif

    if (CALPHA(gc->col) && gc->lty!=-1) {
      cairo_t *cc = xd->cb->cc;
      cairo_new_path(cc);
	  if ((x1==x2 || y1==y2) &&xd->cb->truncate_rect) {
		  /* if we are snapping rectangles to grid, we also need to snap straight
			 lines to make sure they match - e.g. tickmarks, baselines etc. */
		  x1=trunc(x1)+0.5; x2=trunc(x2)+0.5;
		  y1=trunc(y1)+0.5; y2=trunc(y2)+0.5;
	  }
      cairo_move_to(cc, x1, y1);
      cairo_line_to(cc, x2, y2);
      Rcairo_set_color(cc, gc->col);
      Rcairo_set_line(xd, gc);
      cairo_stroke(cc);
    }
}

static void CairoGD_MetricInfo(int c,  R_GE_gcontext *gc,  double* ascent, double* descent,  double* width, NewDevDesc *dd)
{
	CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
	cairo_t *cc;
	cairo_text_extents_t te = {0, 0, 0, 0, 0, 0};
	char str[16];
	int Unicode = mbcslocale;

	if(!xd || !xd->cb) return;
	if(c < 0) {c = -c; Unicode = 1;}

	cc = xd->cb->cc;

	Rcairo_setup_font(xd, gc);

	if (!c) { 
		str[0]='M'; str[1]='g'; str[2]=0;
		/* this should give us a reasonably decent (g) and almost max width (M) */
	} else if (gc->fontface == 5) {
		char s[2];
		s[0] = c; s[1] = '\0';
		AdobeSymbol2utf8(str, s, 16);		
	} else if(Unicode) {
		Rf_ucstoutf8(str, (unsigned int) c);
	} else {
		str[0] = c; str[1] = 0;
		/* Here, we assume that c < 256 */
	}

	cairo_text_extents(cc, str, &te);

#ifdef JGD_DEBUG
	Rprintf("metric %x [%c]: bearing %f:%f, size %f:%f, advance %f:%f\n",c, (char)c, te.x_bearing, te.y_bearing,
			te.width, te.height, te.x_advance, te.y_advance);
#endif

	*ascent  = -te.y_bearing; 
	*descent = te.height+te.y_bearing;
	/* cairo doesn't report width of whitespace, so use x_advance */
	*width = te.x_advance;

#ifdef JGD_DEBUG
	Rprintf("FM> ascent=%f, descent=%f, width=%f\n", *ascent, *descent, *width);
#endif
}

static void CairoGD_Mode(int mode, NewDevDesc *dd)
{
    CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
    if(!xd || !xd->cb) return;
	if (xd->cb->mode) xd->cb->mode(xd->cb, mode);
}

static void CairoGD_NewPage(R_GE_gcontext *gc, NewDevDesc *dd)
{
	CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
	cairo_t *cc;
	if(!xd || !xd->cb) return;

	cc = xd->cb->cc;

	xd->npages++;
	if (xd->npages > 0)  /* first request is not saved as this is part of the init */
		xd->cb->save_page(xd->cb,xd->npages);

	cairo_reset_clip(cc);
	/* we don't need to fill if the back-end sets nozero and bg is transparent */
	if (! (R_TRANSPARENT(xd->bg) && (xd->cb->flags & CDF_NOZERO)) ) {
		/* we are assuming that the operator is reasonable enough
		   to clear the page. Setting CAIRO_OPERATOR_SOURCE seemed
		   to trigger rasterization for PDF and SVG backends, so we
		   leave the operator alone. Make sure the back-end sets
		   an operator that is optimal for the back-end */
		Rcairo_set_color(cc, xd->bg);
		if (xd->cb->flags & CDF_OPAQUE) {
			/* Opaque devices use canvas if bg is transparent */
			if (R_TRANSPARENT(xd->bg))
				Rcairo_set_color(cc, xd->canvas);
		} else {
			if (xd->cb->flags & CDF_FAKE_BG) {
				if (R_TRANSPARENT(xd->bg))
					Rcairo_set_color(cc, fake_bg_color);
			}
		}
		cairo_new_path(cc);
		cairo_paint(cc);
	}
}

static SEXP findArg(const char *name, SEXP list) {
	SEXP ns = install(name);
	while (list && list!=R_NilValue) {
		if (TAG(list)==ns) return CAR(list);
		list=CDR(list);
	}
	return 0;
}

Rboolean CairoGD_Open(NewDevDesc *dd, CairoGDDesc *xd,  const char *type, int conn, const char *file, double w, double h,
					  double umpl, SEXP aux)
{
	cairo_t *cc;

	if (umpl==0) error("unit multiplier cannot be zero");

	xd->fill   = 0xffffffff; /* transparent, was R_RGB(255, 255, 255); */
	xd->col    = R_RGB(0, 0, 0);

	xd->npages=-1;	
	xd->cb = (Rcairo_backend*) calloc(1,sizeof(Rcairo_backend));
	if (!xd->cb) return FALSE;

	xd->cb->dd = dd;
	xd->cb->dpix = xd->dpix;	
	xd->cb->dpiy = xd->dpiy;	

	/* Select Cairo backend */
	/* Cairo 1.2-0: jpeg and tiff are created via external libraries (libjpeg/libtiff) by our code */
	if (!strcmp(type,"png") || !strcmp(type,"png24")  || !strcmp(type,"jpeg") || !strcmp(type,"jpg") ||
		!strcmp(type,"tif")  || !strcmp(type,"tiff")) {
		int alpha_plane = 0;
		int quality = 0; /* find out if we have quality setting */
		if (R_ALPHA(xd->bg) < 255) alpha_plane=1;
		if (!strcmp(type,"jpeg") || !strcmp(type,"jpg")) {
			SEXP arg = findArg("quality", aux);
			if (arg && arg!=R_NilValue)
				quality = asInteger(arg);
			if (quality<0) quality=0;
			if (quality>100) quality=100;
			alpha_plane=0;
		}
		if (!strcmp(type,"tif")  || !strcmp(type,"tiff")) {
			SEXP arg = findArg("compression", aux);
			if (arg && arg!=R_NilValue)
				quality = asInteger(arg);
			else
				quality = TIFF_COMPR_LZW; /* default */
		}
		if (umpl>=0) {
			if (xd->dpix <= 0)
				error("images cannot be created with other units than 'px' if dpi is not specified");
			if (xd->dpiy <= 0) xd->dpiy=xd->dpix;
			w = w*umpl*xd->dpix;
			h = h*umpl*xd->dpiy;
		} else {
			if (umpl != -1) {
				w = (-umpl)*w;
				h = (-umpl)*h;
			}
		}
		xd->cb->width = w; xd->cb->height = h;
		xd->cb = Rcairo_new_image_backend(xd->cb, conn, file, type, (int)(w+0.5), (int)(h+0.5), quality, alpha_plane);
	}
	else if (!strcmp(type,"pdf") || !strcmp(type,"ps") || !strcmp(type,"postscript") || !strcmp(type,"svg")) {
		/* devices using native units, covert those to points */
		if (umpl<0) {
			if (xd->dpix <= 0)
				error("dpi must be specified when creating vector devices with units='px'");
			if (xd->dpiy <= 0) xd->dpiy=xd->dpix;
			w = w/xd->dpix;
			h = h/xd->dpiy;
			umpl=1;
		}
		w = w * umpl * 72; /* inches * 72 = pt */
		h = h * umpl * 72;
		xd->cb->width = w; xd->cb->height = h;
		xd->cb->flags|=CDF_NOZERO;
		if (!strcmp(type,"pdf"))
			xd->cb = Rcairo_new_pdf_backend(xd->cb, conn, file, w, h);
		else if (!strcmp(type,"ps") || !strcmp(type,"postscript"))
			xd->cb = Rcairo_new_ps_backend(xd->cb, conn, file, w, h);
		else if (!strcmp(type,"svg"))
			xd->cb = Rcairo_new_svg_backend(xd->cb, conn, file, w, h);
	} else {
		/* following devices are pixel-based yet supporting umpl */
		if (umpl>0 && xd->dpix>0) { /* convert if we can */
			if (xd->dpiy <= 0) xd->dpiy=xd->dpix;
			w = w * umpl * xd->dpix;
			h = h * umpl * xd->dpiy;
			umpl = -1;
		} /* otherwise device's dpi will be used */
		xd->cb->width = w; xd->cb->height = h;
		/* also following devices are opaque UI-devices */
		xd->cb->flags |= CDF_HAS_UI|CDF_OPAQUE;
		if (!strcmp(type,"x11") || !strcmp(type,"X11") || !strcmp(type,"xlib"))
			xd->cb = Rcairo_new_xlib_backend(xd->cb, file, w, h, umpl);
		else if (!strncmp(type,"win",3))
			xd->cb = Rcairo_new_w32_backend(xd->cb, file, w, h, umpl);
		else {
			error("Unsupported output type \"%s\" - choose from png, png24, jpeg, tiff, pdf, ps, svg, win and x11.", type);
			return FALSE;
		}
	}
	if (!xd->cb) {
		error("Failed to create Cairo backend!");
		return FALSE;
	}

	cc = xd->cb->cc;

	/* get modified dpi in case the backend has set it */
	xd->dpix = xd->cb->dpix;
	xd->dpiy = xd->cb->dpiy;
	if (xd->dpix>0 && xd->dpiy>0) xd->asp = xd->dpix / xd->dpiy;

	Rcairo_backend_init_surface(xd->cb);
	/* cairo_save(cc); */

#ifdef JGD_DEBUG
	Rprintf("open [type='%s'] %d x %d (flags %04x)\n", type, (int)w, (int)h, xd->cb->flags);
#endif

	return TRUE;
}

static void CairoGD_Polygon(int n, double *x, double *y,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
	CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
	if(!xd || !xd->cb || n<2) return;
	{
		int i=1;
		cairo_t *cc = xd->cb->cc;

		Rcairo_set_line(xd, gc);

#ifdef JGD_DEBUG
		Rprintf("polygon %d points [%08x/%08x]\n", n, gc->col, gc->fill);
#endif
		cairo_new_path(cc);
		cairo_move_to(cc, x[0], y[0]);
		while (i<n) { cairo_line_to(cc, x[i], y[i]); i++; }
		cairo_close_path(cc);
		if (CALPHA(gc->fill)) {
			Rcairo_set_color(cc, gc->fill);
			cairo_fill_preserve(cc);
		}
		if (CALPHA(gc->col) && gc->lty!=-1) {
			Rcairo_set_color(cc, gc->col);
			cairo_stroke(cc);
		} else cairo_new_path(cc);
	}
}

static void CairoGD_Polyline(int n, double *x, double *y,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
	CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
	if(!xd || !xd->cb || n<2) return;
	{
		int i=1;
		cairo_t *cc = xd->cb->cc;

#ifdef JGD_DEBUG
		Rprintf("poly-line %d points [%08x]\n", n, gc->col);
#endif
		if (CALPHA(gc->col) && gc->lty!=-1) {
			cairo_new_path(cc);
			cairo_move_to(cc, x[0], y[0]);
			while (i<n) { cairo_line_to(cc, x[i], y[i]); i++; }
			Rcairo_set_color(cc, gc->col);
			Rcairo_set_line(xd, gc);
			cairo_stroke(cc);
		}
	}
}

static void CairoGD_Rect(double x0, double y0, double x1, double y1,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
	CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
	if(!xd || !xd->cb) return;
	{
		cairo_t *cc = xd->cb->cc;
		double snap_add = 0.0;
		if (x1<x0) { double h=x1; x1=x0; x0=h; }
		if (y1<y0) { double h=y1; y1=y0; y0=h; }
		/* if (x0<0) x0=0; if (y0<0) y0=0; */

		Rcairo_set_line(xd, gc);

#ifdef JGD_DEBUG
		Rprintf("rect: %x %f/%f %f/%f [%08x/%08x]\n", cc, x0, y0, x1, y1, gc->col, gc->fill);
#endif

		/* Snap to grid so that image() plots and rectangles look nicer, but only if the backend wishes so */
		if (xd->cb->truncate_rect) {
			cairo_user_to_device(cc,&x0,&y0);
			cairo_user_to_device(cc,&x1,&y1);
			x0 = trunc(x0); x1 = trunc(x1);
			y0 = trunc(y0); y1 = trunc(y1);
			cairo_device_to_user(cc,&x0,&y0);
			cairo_device_to_user(cc,&x1,&y1);
			snap_add=1;
		}

		cairo_new_path(cc);
		cairo_rectangle(cc, x0, y0, x1-x0+snap_add, y1-y0+snap_add);
		if (CALPHA(gc->fill)) {
			Rcairo_set_color(cc, gc->fill);
			cairo_fill_preserve(cc);
		}
		if (CALPHA(gc->col) && gc->lty!=-1) {
			/* if we are snapping, note that lines must be in 0.5 offset to fills in order
			   hit the same pixels, because lines extend to both sides */
			if (xd->cb->truncate_rect) {
				cairo_new_path(cc);
				cairo_rectangle(cc, x0+0.5, y0+0.5, x1-x0, y1-y0);
			}
			Rcairo_set_color(cc, gc->col);
			cairo_stroke(cc);
		} else cairo_new_path(cc);
	}
}

static void CairoGD_Size(double *left, double *right,  double *bottom, double *top,  NewDevDesc *dd)
{
	CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
	if(!xd || !xd->cb) return;	
	*left=*top=0.0;
	*right=xd->cb->width;
	*bottom=xd->cb->height;
}

static double CairoGD_StrWidth(char *str,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
	CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
	int slen = strlen(str);
	if (!str) return 0;
	if(!xd || !xd->cb) return slen*8;

	Rcairo_setup_font(xd, gc);

	{
		cairo_t *cc = xd->cb->cc;
		cairo_text_extents_t te;
		cairo_text_extents(cc, str, &te);
		/* Cairo doesn't take into account whitespace char widths, 
		 * but the x_advance does */
		return te.x_advance;
	}
}

static void CairoGD_Text(double x, double y, char *str,  double rot, double hadj,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
	CairoGDDesc *xd = (CairoGDDesc *) dd->deviceSpecific;
	cairo_t *cc;

	if(!xd || !xd->cb) return;

	cc = xd->cb->cc;
		
	Rcairo_setup_font(xd,gc);

#ifdef JGD_DEBUG
	Rprintf("text \"%s\" (%d) face %d  %f:%f rot=%f hadj=%f [%08x:%08x]\n", str, len, gc->fontface, x, y, rot, hadj, gc->col, gc->fill);
#endif

	cairo_save(cc);
	cairo_move_to(cc, x, y);
	if (hadj!=0. || rot!=0.) {
		cairo_text_extents_t te;
		cairo_text_extents(cc, str, &te);
		if (rot!=0.)
			cairo_rotate(cc, -rot/180.*M_PI);
		if (hadj!=0.)
			cairo_rel_move_to(cc, -te.x_advance*hadj, 0);

		/* Rcairo_set_color(cc, 0xff80ff); */
	}
	Rcairo_set_color(cc, gc->col);
	cairo_show_text(cc, str);
#ifdef JGD_DEBUG
	{
		cairo_text_extents_t te;
		cairo_text_extents(cc, str, &te);

		if (hadj!=0.) x = x - (te.x_advance*hadj);

		/* debug - mark the origin of the text */
		Rcairo_set_color(cc, 0x8080ff);
		cairo_new_path(cc); cairo_move_to(cc,x-3.,y); cairo_line_to(cc,x+3.,y); cairo_stroke(cc);
		cairo_new_path(cc); cairo_move_to(cc,x,y-3.); cairo_line_to(cc,x,y+3.); cairo_stroke(cc);

		x = x + te.x_advance;
		cairo_new_path(cc); cairo_move_to(cc,x-3.,y); cairo_line_to(cc,x+3.,y); cairo_stroke(cc);
		cairo_new_path(cc); cairo_move_to(cc,x,y-3.); cairo_line_to(cc,x,y+3.); cairo_stroke(cc);

	}
#endif

	cairo_restore(cc);
}

/** fill the R device structure with callback functions */
void Rcairo_setup_gd_functions(NewDevDesc *dd) {
    dd->close = CairoGD_Close;
    dd->activate = CairoGD_Activate;
    dd->deactivate = CairoGD_Deactivate;
    dd->size = CairoGD_Size;
    dd->newPage = CairoGD_NewPage;
    dd->clip = CairoGD_Clip;
    dd->strWidth = CairoGD_StrWidth;
    dd->text = CairoGD_Text;
    dd->rect = CairoGD_Rect;
    dd->circle = CairoGD_Circle;
    dd->line = CairoGD_Line;
    dd->polyline = CairoGD_Polyline;
    dd->polygon = CairoGD_Polygon;
    dd->locator = CairoGD_Locator;
    dd->mode = CairoGD_Mode;
    dd->metricInfo = CairoGD_MetricInfo;
#if R_GE_version >= 4
	dd->hasTextUTF8 = TRUE;
    dd->strWidthUTF8 = CairoGD_StrWidth;
    dd->textUTF8 = CairoGD_Text;
	dd->wantSymbolUTF8 = TRUE;
#endif
}

void Rcairo_backend_resize(Rcairo_backend *be, double width, double height) {
	if (!be || !be->dd) return;
	if (be->resize) {
		CairoGDDesc *xd = (CairoGDDesc *) be->dd->deviceSpecific;
		if(!xd) return;
		/* Rprintf("cairotalk.resize(%d,%d) %d:%d %dx%d\n", width, height, be->dd->top, be->dd->left, be->dd->right, be->dd->bottom); */
		
		be->width=width;
		be->height=height;
		be->dd->size(&(be->dd->left), &(be->dd->right), &(be->dd->bottom), &(be->dd->top), be->dd);
		be->resize(be, width, height);
	}
}

void Rcairo_backend_repaint(Rcairo_backend *be) {
	if (!be || !be->dd) return;
	{
		int devNum = ndevNumber(be->dd);
		if (devNum > 0) {
			be->in_replay = 1;
			GEplayDisplayList(GEgetDevice(devNum));
			be->in_replay = 0;
			if (be->mode) be->mode(be, -1);
		}
	}
}

void Rcairo_backend_kill(Rcairo_backend *be) {
	if (!be || !be->dd) return;
	GEkillDevice(desc2GEDesc(be->dd));
}

static int has_initd_fc = 0;

void Rcairo_backend_init_surface(Rcairo_backend *be) {
	cairo_t *cc = be->cc;
	/* clearing of the surface is done directly in NewPage */
	/* CairoGDDesc *cd = (CairoGDDesc *) be->dd->deviceSpecific; */

	cairo_reset_clip(cc);

#ifdef CAIRO_HAS_FT_FONT
	/* Ensure that fontconfig library is ready */
	if (!has_initd_fc && !FcInit ())
		error ("Can't init font config library\n");
	has_initd_fc = 1;

	/* Ensure that freetype library is ready */
	if (!Rcairo_ft_library &&
		FT_Init_FreeType(&Rcairo_ft_library))
		error("Failed to initialize freetype library in CairoGD_Open!\n");

	if (Rcairo_fonts[0].face == NULL) Rcairo_set_font(0,"Helvetica:style=Regular");
	if (Rcairo_fonts[1].face == NULL) Rcairo_set_font(1,"Helvetica:style=Bold");
	if (Rcairo_fonts[2].face == NULL) Rcairo_set_font(2,"Helvetica:style=Italic");
	if (Rcairo_fonts[3].face == NULL) Rcairo_set_font(3,"Helvetica:style=Bold Italic,BoldItalic");
	if (Rcairo_fonts[4].face == NULL) Rcairo_set_font(4,"Symbol");
#else
	cairo_select_font_face (cc, "Helvetica",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size (cc, 14);
#endif
}

/* add any new back-ends to this list */
void Rcairo_register_builtin_backends() {
	if (RcairoBackendDef_image) Rcairo_register_backend(RcairoBackendDef_image);
	if (RcairoBackendDef_pdf) Rcairo_register_backend(RcairoBackendDef_pdf);
	if (RcairoBackendDef_ps) Rcairo_register_backend(RcairoBackendDef_ps);
	if (RcairoBackendDef_svg) Rcairo_register_backend(RcairoBackendDef_svg);
	if (RcairoBackendDef_xlib) Rcairo_register_backend(RcairoBackendDef_xlib);
	if (RcairoBackendDef_w32) Rcairo_register_backend(RcairoBackendDef_w32);
}

/* called on load */
SEXP Rcairo_initialize() {
	Rcairo_register_builtin_backends();
	return R_NilValue;
}

