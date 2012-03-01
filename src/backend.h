#ifndef __CAIRO_BACKEND_H__
#define __CAIRO_BACKEND_H__

/* Cario config from configure */
#include "cconfig.h"

#include <cairo.h>
#include <R.h>
#include <Rversion.h>
#include <Rinternals.h>
#include <R_ext/GraphicsEngine.h>
#include <R_ext/GraphicsDevice.h>

#if R_VERSION >= R_Version(2,8,0)
#ifndef NewDevDesc
#define NewDevDesc DevDesc
#endif
#endif

#define CDF_HAS_UI    0x0001  /* backend has UI (e.g. window) */
#define CDF_FAKE_BG   0x0002  /* fake transparent background */
#define CDF_OPAQUE    0x0004  /* device doesn't support any kind of alpha, not even fake */
#define CDF_NOZERO    0x0008  /* if set pages don't need to be zeroed */

#define CBDF_FILE      0x0001  /* can output to file(s) */
#define CBDF_CONN      0x0002  /* can output to a connection */
#define CBDF_VISUAL    0x0004  /* can produce visual output (window/screen) */
#define CBDF_MULTIPAGE 0x0008  /* supports multiple pages in one document (relevant to file/conn only) */

#define fake_bg_color (0xfffefefe)

/* known cairo back-ends (as of cairographics 1.4.2) */
#define BET_IMAGE   1
#define BET_PDF     2
#define BET_PS      3
#define BET_SVG     4
#define BET_XLIB    5
#define BET_W32     6
#define BET_QUARTZ  7
#define BET_USER   64

typedef struct st_Rcairo_backend_def {
    int backend_type;      /* see BET_xxx constants */
    char **types;          /* supported types (0-terminated list) */
    char *name;            /* human-readable back-end name */
    int  flags;            /* see CBDF_xxx constants */
    /*----- creation call-back -----*/
    void *create;          /* not used yet, should be set to 0 */
} Rcairo_backend_def;

typedef struct st_Rcairo_backend {
    int backend_type;      /* see BET_xxx constants */
    /*----- instance variables -----*/
    void *backendSpecific; /* private data for backend use */
    cairo_t          *cc;  /* cairo context */
    cairo_surface_t  *cs;  /* cairo surface */
    NewDevDesc       *dd;  /* device descriptor */
    double width, height;  /* size in native units (pixels or pts) */
    int               in_replay; /* set to 1 if it is known where the painting stops
				    and thus the backend doesn't need to perform
				    any synchronization until we're done.
				    if mode is set, it is called ater replay
				    with mode=-1
				 */
    int               truncate_rect; /* set to 1 to truncate rectangle coordinates
					to integers. Useful with bitmap back-ends. */
    int               serial; /* this variable is increased with each operation
				 so it can be used to track whether the content
				 has potentially changed since last poll */
    SEXP              onSave; /* optional R callback to call after the page
				 has been saved */
    /*----- back-end global variables (capabilities etc.) -----*/
    int  flags; /* see CDF_xxx above */
    double dpix, dpiy;
    
    /*----- back-end global callbacks (=methods) -----*/
    /* cairo_surface_t *(*create_surface)(struct st_Rcairo_backend *be, int width, int height); */
    void (*save_page)(struct st_Rcairo_backend *be, int pageno);
    void (*destroy_backend)(struct st_Rcairo_backend *be);
    
    /* optional callbacks - must be set to 0 if unsupported */
    int  (*locator)(struct st_Rcairo_backend *be, double *x, double *y);
    void (*activation)(struct st_Rcairo_backend *be, int activate);  /* maps both Activate/Deactivate */
    void (*mode)(struct st_Rcairo_backend *be, int mode); /* in addition it is called after internal replay with mode -1 */
    void (*resize)(struct st_Rcairo_backend *be, double width, double height);
    void (*sync)(struct st_Rcairo_backend *be); /* force sync for devices that do asynchronous drawing */
} Rcairo_backend;

/* implemented in cairotalk but can be used by any back-end to talk to the GD system */
void Rcairo_backend_resize(Rcairo_backend *be, double width, double height); /* won't do anything if resize is not implemented in the back-end */
void Rcairo_backend_repaint(Rcairo_backend *be); /* re-plays the display list */
void Rcairo_backend_kill(Rcairo_backend *be); /* kills the devide */
void Rcairo_backend_init_surface(Rcairo_backend *be); /* initialize a new surface */
#endif
