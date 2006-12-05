#include "cairogd.h"
#include "cairotalk.h"
#include "backend.h"
#include "img-backend.h"
#include <Rversion.h>

/* Device Driver Actions */

#if R_VERSION < 0x10900
#error This device needs at least R version 1.9.0
#endif

#define CREDC(C) (((unsigned int)(C))&0xff)
#define CGREENC(C) ((((unsigned int)(C))&0xff00)>>8)
#define CBLUEC(C) ((((unsigned int)(C))&0xff0000)>>16)
#define CALPHA(C) ((((unsigned int)(C))&0xff000000)>>24)

static void GDD_Activate(NewDevDesc *dd);
static void GDD_Circle(double x, double y, double r,
			  R_GE_gcontext *gc,
			  NewDevDesc *dd);
static void GDD_Clip(double x0, double x1, double y0, double y1,
			NewDevDesc *dd);
static void GDD_Close(NewDevDesc *dd);
static void GDD_Deactivate(NewDevDesc *dd);
static void GDD_Hold(NewDevDesc *dd);
static Rboolean GDD_Locator(double *x, double *y, NewDevDesc *dd);
static void GDD_Line(double x1, double y1, double x2, double y2,
			R_GE_gcontext *gc,
			NewDevDesc *dd);
static void GDD_MetricInfo(int c, 
			      R_GE_gcontext *gc,
			      double* ascent, double* descent,
			      double* width, NewDevDesc *dd);
static void GDD_Mode(int mode, NewDevDesc *dd);
static void GDD_NewPage(R_GE_gcontext *gc, NewDevDesc *dd);
static void GDD_Polygon(int n, double *x, double *y,
			   R_GE_gcontext *gc,
			   NewDevDesc *dd);
static void GDD_Polyline(int n, double *x, double *y,
			     R_GE_gcontext *gc,
			     NewDevDesc *dd);
static void GDD_Rect(double x0, double y0, double x1, double y1,
			 R_GE_gcontext *gc,
			 NewDevDesc *dd);
static void GDD_Size(double *left, double *right,
			 double *bottom, double *top,
			 NewDevDesc *dd);
static double GDD_StrWidth(char *str, 
			       R_GE_gcontext *gc,
			       NewDevDesc *dd);
static void GDD_Text(double x, double y, char *str,
			 double rot, double hadj,
			 R_GE_gcontext *gc,
			 NewDevDesc *dd);



#ifdef JGD_DEBUG
#define Rcairo_set_source_rgb(C,R,G,B) { printf("S:RGB> %f %f %f\n", R, G, B); cairo_set_source_rgb(C,R,G,B); }
#define Rcairo_set_source_rgba(C,R,G,B,A) { printf("S:RGBA> %f %f %f (%f)\n", R, G, B, A); cairo_set_source_rgba(C,R,G,B,A); }
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

cairo_font_face_t *Rcairo_set_font_face(int i, char *file){
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

void Rcairo_set_font(int i, char *fcname){
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
				Rcairo_fonts[i].face = Rcairo_set_font_face(i,(char *)file);
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

/* ret must be large enough to hold strlen(ascii)*2 + 1 bytes */
static char *ascii2utf8(char *ascii, char *ret){
	char *aux = ret;

	while (*ascii) {
		unsigned char ch = *ascii++;
		if (ch < 0x80) {
			*aux++ = ch;
		} else {
			*aux++ = 0xc0 | (ch >> 6 & 0x1f);
			*aux++ = 0x80 | (ch & 0x3f);
		}
	}
	*aux = 0;
	return ret;
}

static void Rcairo_setup_font(GDDDesc* xd, R_GE_gcontext *gc) {

#ifdef CAIRO_HAS_FT_FONT
	int i = gc->fontface - 1;

	if (i < 0 || i >= 5){
		error("font %d not recognized in Rcairo_setup_font\n",i);
		return;
	}

	if (Rcairo_fonts[i].updated || (xd->fontface != gc->fontface)){
		cairo_set_font_face(xd->cc,Rcairo_fonts[i].face);
		Rcairo_fonts[i].updated = 0;
#ifdef JGD_DEBUG
		  printf("  font face changed to \"%d\" %fpt\n", gc->fontface, gc->cex*gc->ps + 0.5);
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
  
  cairo_select_font_face (xd->cc, Cfontface, slant, wght);

#ifdef JGD_DEBUG
  printf("  font \"%s\" %fpt (slant:%d, weight:%d)\n", Cfontface, gc->cex*gc->ps + 0.5, slant, wght);
#endif
#endif

  /* Add 0.5 per devX11.c in R src. We want to match it's png output as close
   * as possible. */
  cairo_set_font_size (xd->cc, gc->cex * gc->ps + 0.5);
}

static void Rcairo_set_line(GDDDesc* xd, R_GE_gcontext *gc) {
	R_GE_lineend lend;
	R_GE_linejoin ljoin;
	
	/* Line width: par lwd  */
  cairo_set_line_width(xd->cc, gc->lwd);

	/* Line end: par lend  */
	switch(gc->lend){
		case GE_ROUND_CAP: lend = CAIRO_LINE_CAP_ROUND; break;
		case GE_BUTT_CAP: lend = CAIRO_LINE_CAP_BUTT; break;
		case GE_SQUARE_CAP: lend = CAIRO_LINE_CAP_SQUARE; break;
	}
	cairo_set_line_cap(xd->cc,lend);

	/* Line join: par ljoin */
	switch(gc->ljoin){
		case GE_ROUND_JOIN: ljoin = CAIRO_LINE_JOIN_ROUND; break;
		case GE_MITRE_JOIN: ljoin = CAIRO_LINE_JOIN_MITER; break;
		case GE_BEVEL_JOIN: ljoin = CAIRO_LINE_JOIN_BEVEL; break;
	} 
	cairo_set_line_join(xd->cc,ljoin);

  if (gc->lty==0 || gc->lty==-1)
    cairo_set_dash(xd->cc,0,0,0);
  else {
    double ls[16]; /* max 16x4=64 bit */
    int l=0, dt=gc->lty;
    while (dt>0) {
      ls[l]=(double)(dt&15);
      dt>>=4;
      l++;
    }
    cairo_set_dash(xd->cc, ls, l, 0);
  }
}

/*------- the R callbacks begin here ... ------------------------*/

static void GDD_Activate(NewDevDesc *dd)
{
    GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
    if(!xd || !xd->cc) return;
}

static void GDD_Circle(double x, double y, double r,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
	GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
	if(!xd || !xd->cc) return;
	{
		cairo_t *cc = xd->cc;

#ifdef JGD_DEBUG
		printf("circle %x %f/%f %f [%08x/%08x]\n",xd->cc, x, y, r, gc->col, gc->fill);
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

static void GDD_Clip(double x0, double x1, double y0, double y1,  NewDevDesc *dd)
{
    GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
    if(!xd || !xd->cc) return;
    if (x1<x0) { double h=x1; x1=x0; x0=h; };
    if (y1<y0) { double h=y1; y1=y0; y0=h; };

    cairo_reset_clip(xd->cc);
    cairo_new_path(xd->cc);
    cairo_rectangle(xd->cc, x0, y0, x1-x0 + 1, y1-y0 + 1); /* Add 1 per newX11_Clip */
    cairo_clip(xd->cc);

    
#ifdef JGD_DEBUG
    printf("clipping %f/%f %f/%f\n", x0, y0, x1, y1);
#endif
}

/** note that saveActiveImage doesn't increase the sequence number to avoid confusion */
static void saveActiveImage(GDDDesc * xd)
{
	char *it = xd->img_type;
	char *fn;
	FILE *out;
	int   nl;

	fn=(char*) malloc(strlen(xd->img_name)+16);
	strcpy(fn, xd->img_name);
	if (xd->img_seq>0)
		sprintf(fn+strlen(fn),"%d",xd->img_seq);
	nl = strlen(fn);

	if (!strcmp(it, "png") || !strcmp(it, "png24")) {
		if (nl>3 && strcmp(fn+nl-4,".png")) strcat(fn, ".png");
		if (xd->cs) cairo_surface_write_to_png(xd->cs, fn);
	} else {
		error("Unsupported image type (%s).", it);
	}
	free(fn);
}

static void GDD_Close(NewDevDesc *dd)
{
  GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
  if(!xd || !xd->cc) return;
  
  saveActiveImage(xd);

  if (xd->img_name) free(xd->img_name);
  if (xd->cb) { xd->cb->destroy_surface(xd->cb); free(xd->cb); }
  if (xd->cc) cairo_destroy(xd->cc); 
  if (xd->cs) cairo_surface_destroy(xd->cs);
  free(xd);
  dd->deviceSpecific=NULL;
}

static void GDD_Deactivate(NewDevDesc *dd)
{
    GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
}

static void GDD_Hold(NewDevDesc *dd)
{
    GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
}

static Rboolean GDD_Locator(double *x, double *y, NewDevDesc *dd)
{
    GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
    if(!xd || !xd->cc) return FALSE;
    if (xd->cb && xd->cb->locator) return xd->cb->locator(xd->cb, x, y);
    return FALSE;
}

static void GDD_Line(double x1, double y1, double x2, double y2,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
    if(!xd || !xd->cc) return;
    
#ifdef JGD_DEBUG
    printf("line %f/%f %f/%f [%08x/%08x]\n", x1, y1, x2, y2, gc->col, gc->fill);
#endif

    if (CALPHA(gc->col) && gc->lty!=-1) {
      cairo_t *cc = xd->cc;
      cairo_new_path(cc);
      cairo_move_to(cc, x1, y1);
      cairo_line_to(cc, x2, y2);
      Rcairo_set_color(cc, gc->col);
      Rcairo_set_line(xd, gc);
      cairo_stroke(cc);
    }
}

static void GDD_MetricInfo(int c,  R_GE_gcontext *gc,  double* ascent, double* descent,  double* width, NewDevDesc *dd)
{
	GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
	cairo_t *cc = xd->cc;
	cairo_text_extents_t te = {0, 0, 0, 0, 0, 0};
	char str[3], utf8[8];

	if(!xd || !xd->cc) return;

	Rcairo_setup_font(xd, gc);

	if (!c) { 
		str[0]='M'; str[1]='g'; str[2]=0;
		/* this should give us a reasonably decent (g) and almost max width (M) */
	} else {
		str[0]=(char)c; str[1]=0;
		/* Here, we assume that c < 256 */
	}
	ascii2utf8(str,utf8);

	cairo_text_extents(cc, utf8, &te);

#ifdef JGD_DEBUG
	printf("metric %x [%c]: bearing %f:%f, size %f:%f, advance %f:%f\n",c, (char)c, te.x_bearing, te.y_bearing,
			te.width, te.height, te.x_advance, te.y_advance);
#endif

	*ascent  = -te.y_bearing; 
	*descent = te.height+te.y_bearing;
	/* cairo doesn't report width of whitespace, so use x_advance */
	*width = te.x_advance;

#ifdef JGD_DEBUG
	printf("FM> ascent=%f, descent=%f, width=%f\n", *ascent, *descent, *width);
#endif
}

static void GDD_Mode(int mode, NewDevDesc *dd)
{
    GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
}

static void GDD_NewPage(R_GE_gcontext *gc, NewDevDesc *dd)
{
	GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
	if(!xd || !xd->cc) return;

	if (xd->img_seq!=-1)  /* first request is not saved as this is part of the init */
		saveActiveImage(xd);
	xd->img_seq++;

	/* Set new parameters from graphical context.
	 * do we need more than fill color for background?
	 */
	xd->gd_bgcolor = gc->fill;

	Rcairo_set_color(xd->cc, xd->gd_bgcolor);
	cairo_set_operator(xd->cc, CAIRO_OPERATOR_SOURCE);
	cairo_new_path(xd->cc);
	cairo_paint(xd->cc);
}

Rboolean GDD_Open(NewDevDesc *dd, GDDDesc *xd,  char *type, char *file, double w, double h, int bgcolor)
{

	xd->fill = 0xffffffff; /* transparent, was R_RGB(255, 255, 255); */
	xd->col = R_RGB(0, 0, 0);
	xd->canvas = R_RGB(255, 255, 255);
	xd->windowWidth = w;
	xd->windowHeight = h;

	xd->img_type[7]=0;
	strncpy(xd->img_type, type, 7);
	xd->img_name=(char*) malloc(strlen(file)+1);
	strcpy(xd->img_name, file);
	xd->img_seq=-1;	
	xd->gd_bgcolor = bgcolor;
	/* FIXME: should select the backend here */
	xd->cb = Rcairo_new_image_backend(xd->img_type); 
	xd->cs = xd->cb->create_surface(xd->cb, (int)w, (int)h);
	if (cairo_surface_status(xd->cs) != CAIRO_STATUS_SUCCESS){
		if (xd->img_name) free(xd->img_name);
		xd->cb->destroy_surface(xd->cb);
		free(xd->cb);
		error("Failed to create Cairo Surface!");
		return FALSE;
	}
	xd->cc = cairo_create(xd->cs);

	Rcairo_set_color(xd->cc, bgcolor);
	cairo_set_operator(xd->cc, CAIRO_OPERATOR_SOURCE);
	cairo_paint(xd->cc);

#ifdef CAIRO_HAS_FT_FONT
	/* Ensure that fontconfig library is ready */
	if (!FcInit ()) {
		error ("Can't init font config library\n");
		return FALSE;
	}
	/* Ensure that freetype library is ready */
	if (!Rcairo_ft_library){
		if (FT_Init_FreeType(&Rcairo_ft_library)){
			error("Failed to initialize freetype library in GDD_Open!\n");
			return FALSE;
		}
	}
	if (Rcairo_fonts[0].face == NULL) Rcairo_set_font(0,"Helvetica:style=Regular");
	if (Rcairo_fonts[1].face == NULL) Rcairo_set_font(1,"Helvetica:style=Bold");
	if (Rcairo_fonts[2].face == NULL) Rcairo_set_font(2,"Helvetica:style=Italic");
	if (Rcairo_fonts[3].face == NULL) Rcairo_set_font(3,"Helvetica:style=Bold Italic,BoldItalic");
	if (Rcairo_fonts[4].face == NULL) Rcairo_set_font(4,"Symbol");
#else
	cairo_select_font_face (xd->cc, "Helvetica",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size (xd->cc, 14);
#endif

	/* cairo_save(xd->cc); */

#ifdef JGD_DEBUG
	printf("open %dx%d\n", (int)w, (int)h);
#endif

	return TRUE;
}

static void GDD_Polygon(int n, double *x, double *y,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
	GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
	if(!xd || !xd->cc || n<2) return;
	{
		int i=1;
		cairo_t *cc = xd->cc;

		Rcairo_set_line(xd, gc);

#ifdef JGD_DEBUG
		printf("polygon %d points [%08x/%08x]\n", n, gc->col, gc->fill);
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

static void GDD_Polyline(int n, double *x, double *y,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
	GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
	if(!xd || !xd->cc || n<2) return;
	{
		int i=1;
		cairo_t *cc = xd->cc;

#ifdef JGD_DEBUG
		printf("poly-line %d points [%08x]\n", n, gc->col);
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

static void GDD_Rect(double x0, double y0, double x1, double y1,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
	GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
	if(!xd || !xd->cc) return;
	{
		cairo_t *cc = xd->cc;
		if (x1<x0) { double h=x1; x1=x0; x0=h; }
		if (y1<y0) { double h=y1; y1=y0; y0=h; }
		/* if (x0<0) x0=0; if (y0<0) y0=0; */

		Rcairo_set_line(xd, gc);

#ifdef JGD_DEBUG
		printf("rect: %x %f/%f %f/%f [%08x/%08x]\n", xd->cc, x0, y0, x1, y1, gc->col, gc->fill);
#endif

		/* Snap to grid so that image() plots look nicer, but only do this
		 * for raster outputs. */
		if (cairo_surface_get_type(xd->cs) == CAIRO_SURFACE_TYPE_IMAGE) {
			int ix0,ix1,iy0,iy1;
			cairo_user_to_device(xd->cc,&x0,&y0);
			cairo_user_to_device(xd->cc,&x1,&y1);
			ix0 = (int)x0; x0 = (double)ix0;
			ix1 = (int)x1; x1 = (double)ix1;
			iy0 = (int)y0; y0 = (double)iy0;
			iy1 = (int)y1; y1 = (double)iy1;

			cairo_device_to_user(xd->cc,&x0,&y0);
			cairo_device_to_user(xd->cc,&x1,&y1);
		}

		cairo_new_path(cc);
		cairo_rectangle(cc, x0, y0, x1-x0, y1-y0);
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

static void GDD_Size(double *left, double *right,  double *bottom, double *top,  NewDevDesc *dd)
{
    GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
    if(!xd || !xd->cc) return;	
    *left=*top=0.0;
    *right=xd->windowWidth;
    *bottom=xd->windowHeight;
}

static double GDD_StrWidth(char *str,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
	GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
	int slen = strlen(str);
	if (!str) return 0;
	if(!xd || !xd->cc) return slen*8;

	Rcairo_setup_font(xd, gc);

	{
		cairo_text_extents_t te;
#ifdef CAIRO_HAS_FT_FONT
		char buf[32];

		/* Minimize memory allocation calls */
		char *utf8 = (slen > 16)? calloc(1,strlen(str)*2 + 1) : buf;
		if (utf8){
			ascii2utf8(str,utf8);
			cairo_text_extents(xd->cc, utf8, &te);
			if (slen > 16) free(utf8);
		} else {
			warning("No memory from GDD_StrWidth");
			return slen*8;
		}
#else
		cairo_text_extents(xd->cc, str, &te);
#endif
		/* Cairo doesn't take into account whitespace char widths, 
		 * but the x_advance does */
		return te.x_advance;
	}
}

static void GDD_Text(double x, double y, char *str,  double rot, double hadj,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
	GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
	char buf[32];
	char *utf8;
	int len = strlen(str);

	if(!xd || !xd->cc) return;
	{
		
		/* Only convert to utf8 when Symbol font is in use. */
		/* Also try not to call calloc for short strings. */
		if (gc->fontface == 5){
			if (len > 16){
				utf8 = calloc(len * 2 + 1, sizeof(char));
			} else {
				utf8 = buf;
			}
			ascii2utf8(str,utf8);
			str = utf8;
		}
		cairo_t *cc=xd->cc;

		Rcairo_setup_font(xd,gc);
#ifdef JGD_DEBUG
		printf("text \"%s\" face %d  %f:%f rot=%f hadj=%f [%08x:%08x]\n", str, gc->fontface, x, y, rot, hadj, gc->col, gc->fill);
		printf(" length %d\n",len);
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

        /* Remember len is strlen(str), need to free when len > 16 */
        if (gc->fontface == 5 && len > 16) free(utf8);
		cairo_restore(cc);
	}
}

/** fill the R device structure with callback functions */
void setupGDDfunctions(NewDevDesc *dd) {
    dd->open = GDD_Open;
    dd->close = GDD_Close;
    dd->activate = GDD_Activate;
    dd->deactivate = GDD_Deactivate;
    dd->size = GDD_Size;
    dd->newPage = GDD_NewPage;
    dd->clip = GDD_Clip;
    dd->strWidth = GDD_StrWidth;
    dd->text = GDD_Text;
    dd->rect = GDD_Rect;
    dd->circle = GDD_Circle;
    dd->line = GDD_Line;
    dd->polyline = GDD_Polyline;
    dd->polygon = GDD_Polygon;
    dd->locator = GDD_Locator;
    dd->mode = GDD_Mode;
    dd->hold = GDD_Hold;
    dd->metricInfo = GDD_MetricInfo;
}
