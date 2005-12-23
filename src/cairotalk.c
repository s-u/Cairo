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



#define Rcairo_set_source_rgb(C,R,G,B) { printf("S:RGB> %f %f %f\n", R, G, B); cairo_set_source_rgb(C,R,G,B); }
#define Rcairo_set_source_rgba(C,R,G,B,A) { printf("S:RGBA> %f %f %f (%f)\n", R, G, B, A); cairo_set_source_rgb(C,R,G,B); }
#define Rcairo_set_color(cc, col) { if (CALPHA(col)==255) { Rcairo_set_source_rgb (cc, ((double)CREDC(col))/255., ((double)CGREENC(col))/255., ((double)CBLUEC(col))/255.); } else { Rcairo_set_source_rgba (cc, ((double)CREDC(col))/255., ((double)CGREENC(col))/255., ((double)CBLUEC(col))/255., ((double)CALPHA(col))/255.); }; }


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
      cairo_arc(cc, x, y, r, 0., 2 * M_PI);
      if (CALPHA(gc->fill)) {
	Rcairo_set_color(cc, gc->fill);
	cairo_fill_preserve(cc);
      }
      if (CALPHA(gc->col)) {
	Rcairo_set_color(cc, gc->col);
	cairo_set_line_width(cc, gc->lwd);
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
    /*
    cairo_new_path(xd->cc);
    cairo_rectangle(xd->cc, x0, y0, x1-x0, y1-y0);
    cairo_clip(xd->cc);
    */
    
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
    return;
  }
  
  error("Unsupported image type (%s).", it);
}

static void GDD_Close(NewDevDesc *dd)
{
  GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
  if(!xd || !xd->cc) return;
  
  saveActiveImage(xd);
  if (xd->cb) xd->cb->destroy_surface(xd->cb); xd->cb=0;
  if (xd->cc) cairo_destroy(xd->cc); xd->cc=0;
  if (xd->cs) cairo_surface_destroy(xd->cs); xd->cs=0;
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

    {
      cairo_t *cc = xd->cc;
      cairo_new_path(cc);
      cairo_move_to(cc, x1, y1);
      cairo_line_to(cc, x2, y2);
      Rcairo_set_color(cc, gc->col);
      cairo_set_line_width(cc, gc->lwd);
      cairo_stroke(cc);
    }
}

static void GDD_MetricInfo(int c,  R_GE_gcontext *gc,  double* ascent, double* descent,  double* width, NewDevDesc *dd)
{
    GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
    if(!xd || !xd->cc) return;

    /* if (c != xd->gd_ftm_char) */ {
      cairo_t *cc = xd->cc;
      cairo_text_extents_t te;
      int br[8];
      char str[3];
      str[0]=(char)c; str[1]=0;
      if (!c) { str[0]='M'; str[1]='g'; str[2]=0; /* this should give us reasonable descent (g) and almost max width (M) */ }

      cairo_text_extents(cc, str, &te);
      
#ifdef JGD_DEBUG
      printf("metric %x [%c]: bearing %f:%f, size %f:%f, advance %f:%f\n",c, (char)c, te.x_bearing, te.y_bearing,
	     te.width, te.height, te.x_advance, te.y_advance);
#endif
      xd->gd_ftm_ascent=*ascent=-te.y_bearing; xd->gd_ftm_descent=*descent=te.height+te.y_bearing;
      xd->gd_ftm_width=*width=(c)?te.width:(0.5*te.width);
      xd->gd_ftm_char=c;
    } /* caching is disabled for now
else {
      *ascent=xd->gd_ftm_ascent; *descent=xd->gd_ftm_descent;
      *width=xd->gd_ftm_width;
      } */

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
  int devNr, white;
  if(!xd || !xd->cc) return;
    
  devNr = devNumber((DevDesc*) dd);

  if (xd->img_seq!=-1)  /* first request is not saved as this is part of the init */
    saveActiveImage(xd);
  xd->img_seq++;

  Rcairo_set_color(xd->cc, xd->gd_bgcolor);
  cairo_set_operator(xd->cc, CAIRO_OPERATOR_SOURCE);
  cairo_new_path(xd->cc);
  cairo_paint(xd->cc);
}

Rboolean GDD_Open(NewDevDesc *dd, GDDDesc *xd,  char *type, char *file, double w, double h, int bgcolor)
{
	int white;
    
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
    xd->cb = Rcairo_new_image_backend(); /* FIXME: should select the backend here */
    xd->cs = xd->cb->create_surface(xd->cb, (int)w, (int)h);
    xd->cc = cairo_create(xd->cs);
    
    Rcairo_set_color(xd->cc, bgcolor);
    cairo_set_operator(xd->cc, CAIRO_OPERATOR_SOURCE);
    cairo_paint(xd->cc);
    cairo_select_font_face (xd->cc, "Helvetica",
			    CAIRO_FONT_SLANT_NORMAL,
			    CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size (xd->cc, 10);
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

#ifdef JGD_DEBUG
      printf("polygon %d points\n", n);
#endif
      cairo_new_path(cc);
      cairo_move_to(cc, x[0], y[0]);
      while (i<n) { cairo_line_to(cc, x[i], y[i]); i++; }
      cairo_close_path(cc);
      Rcairo_set_color(cc, gc->fill);
      cairo_fill_preserve(cc);
      Rcairo_set_color(cc, gc->col);
      cairo_set_line_width(cc, gc->lwd);
      cairo_stroke(cc);
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
      printf("polygon %d points\n", n);
#endif
      if (CALPHA(gc->col)) {
	cairo_new_path(cc);
	cairo_move_to(cc, x[0], y[0]);
	while (i<n) { cairo_line_to(cc, x[i], y[i]); i++; }
	Rcairo_set_color(cc, gc->col);
	cairo_set_line_width(cc, gc->lwd);
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
#ifdef JGD_DEBUG
      printf("gdRect: %x %f/%f %f/%f [%08x/%08x]\n", xd->cc, x0, y0, x1, y1, gc->col, gc->fill);
#endif
      cairo_new_path(cc);
      cairo_rectangle(cc, x0, y0, x1-x0, y1-y0);
      if (CALPHA(gc->fill)) {
	Rcairo_set_color(cc, gc->fill);
	cairo_fill_preserve(cc);
      }
      Rcairo_set_color(cc, gc->col);
      cairo_stroke(cc);
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
    if (!str) return 0;
    if(!xd || !xd->cc) return strlen(str)*8;
    {
      cairo_text_extents_t te;
      cairo_text_extents(xd->cc, str, &te);
      return te.width;
    }
}

static void GDD_Text(double x, double y, char *str,  double rot, double hadj,  R_GE_gcontext *gc,  NewDevDesc *dd)
{
    GDDDesc *xd = (GDDDesc *) dd->deviceSpecific;
    if(!xd || !xd->cc) return;
    {
      cairo_t *cc=xd->cc;
      
#ifdef JGD_DEBUG
      printf("text \"%s\" @ %f:%f rot=%f hadj=%f [%08x:%08x]\n", str, x, y, rot, hadj, gc->col, gc->fill);
#endif

      /* debug - mark the origin of the text */
      Rcairo_set_color(cc, 0x8080ff);
      cairo_new_path(cc); cairo_move_to(cc,x-3.,y); cairo_line_to(cc,x+3.,y); cairo_stroke(cc);
      cairo_new_path(cc); cairo_move_to(cc,x,y-3.); cairo_line_to(cc,x,y+3.); cairo_stroke(cc);

      cairo_save(cc);
      cairo_move_to(cc, x, y);
      if (hadj!=0. || rot!=0.) {
	cairo_text_extents_t te;
	cairo_text_extents(cc, str, &te);
	if (rot!=0.)
	  cairo_rotate(cc, -rot/180.*M_PI);
	if (hadj!=0.)
	  cairo_rel_move_to(cc, -te.width*hadj, 0);
	
	/* Rcairo_set_color(cc, 0xff80ff); */
      }
      Rcairo_set_color(cc, gc->col);
      cairo_show_text(cc, str);
      cairo_restore(cc);
    }
      /*
		int br[8];
		double rad=rot/180.0*3.141592;
		gdImageStringFT(0, br, xd->gd_draw, xd->gd_ftfont, xd->gd_ftsize, 0.0, 0, 0, str);
		if (hadj!=0.0) {
			double tw=(double)br[2];
			x-=cos(rad)*(tw*hadj);
			y+=sin(rad)*(tw*hadj);
			} */
#ifdef JGD_DEBUG
      /* printf("FT text using font \"%s\", size %.1f\n", xd->gd_ftfont, xd->gd_ftsize); */
#endif

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
