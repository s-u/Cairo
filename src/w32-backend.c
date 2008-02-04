/* -*- mode: C; tab-width: 4; c-basic-offset: 4 -*-
   Created 4/11/2007 by Simon Urbanek
   Copyright (C) 2007        Simon Urbanek

   GA interface is partially based on devWindows.c from R

   License: GPL v2

   Conditionals:
   NATIVE_UI - if set, we use plain Win32 API (works for stand-alone
               or SDI apps), otherwise we use GraphApp from R.
			   The latter is the default.
*/

#include <stdlib.h>
#include <string.h>
#include "w32-backend.h"

#if CAIRO_HAS_WIN32_SURFACE

static const char *types_list[] = { "win", 0 };
static Rcairo_backend_def RcairoBackendDef_ = {
    BET_W32,
    types_list,
	"Windows",
    CBDF_VISUAL,
    0
};
Rcairo_backend_def *RcairoBackendDef_w32 = &RcairoBackendDef_;

#include <R.h>
#include <R_ext/GraphicsEngine.h>

/* --- GraphApp support --- we use GA from R headers --- */
#ifndef NATIVE_UI
#include <ga.h>
#undef resize

/* this is an excerpt from internal.h objinfo structure
   we need it to get the window handle for native GDI */
typedef struct ga_objinfo {
	int kind;
	int refcount;
	HANDLE handle;
} ga_object;
#endif

typedef struct {
	Rcairo_backend *be; /* back-link */
	HWND    wh;         /* window handle */
	HDC     cdc;        /* cache device context */
	HDC     hdc;        /* window device context */
	HBITMAP cb;         /* cache bitmap */
	int     width;
	int     height;
#ifndef NATIVE_UI
	window  gawin;      /* GA window if we use graphapp */
#endif
} Rcairo_w32_data;


/*---- save page ----*/
static void w32_save_page(Rcairo_backend* be, int pageno){
	cairo_show_page(be->cc);
	cairo_set_source_rgba(be->cc,1,1,1,1);
	cairo_reset_clip(be->cc);
	cairo_new_path(be->cc);
	cairo_paint(be->cc);
}

/*---- resize ----*/
static void w32_resize(Rcairo_backend* be, double width, double height){
	Rcairo_w32_data *xd = (Rcairo_w32_data *) be->backendSpecific;
	RECT r;
	HBITMAP ob;
	HDC hdc;
	if (!xd) return;
	if (xd->cdc) {
		DeleteDC(xd->cdc);
		DeleteObject(xd->cb);
	}

	if (be->cs) {
		cairo_destroy(be->cc); be->cc=0;
		cairo_surface_destroy(be->cs); be->cs=0;
	}

	be->width=xd->width=width;
	be->height=xd->height=height;

	/* first re-paint the window */
	if (!xd->hdc)
		xd->hdc = GetDC( xd->wh );
	hdc=xd->hdc;
	GetClientRect( xd->wh, &r );
	be->cs = cairo_win32_surface_create( hdc );
	be->cc = cairo_create( be->cs );
	Rcairo_backend_repaint(be);
	cairo_copy_page(be->cc);
	cairo_surface_flush(be->cs);

	/* then create a cached copy */
	xd->cdc = CreateCompatibleDC( hdc );
	xd->cb = CreateCompatibleBitmap( hdc, r.right, r.bottom );
	ob = SelectObject(xd->cdc, xd->cb);
	BitBlt(xd->cdc, 0, 0, r.right, r.bottom, hdc, 0, 0, SRCCOPY);
}

/*---- mode ---- (-1=replay finished, 0=done, 1=busy, 2=locator) */
static void w32_mode(Rcairo_backend* be, int which){
	Rcairo_w32_data *xd = (Rcairo_w32_data *) be->backendSpecific;
	if (be->in_replay) return;
	if (which < 1) {
		cairo_copy_page(be->cc);
		cairo_surface_flush(be->cs);

		/* upate cache */
		if (xd->cdc) {
			DeleteDC(xd->cdc);
			DeleteObject(xd->cb);
		}
		xd->cdc = CreateCompatibleDC( xd->hdc );
		xd->cb = CreateCompatibleBitmap( xd->hdc, xd->width, xd->height );
		SelectObject(xd->cdc, xd->cb);
		BitBlt(xd->cdc, 0, 0, xd->width, xd->height, xd->hdc, 0, 0, SRCCOPY);
	}
}

/*---- locator ----*/
int  w32_locator(struct st_Rcairo_backend *be, double *x, double *y) {
	Rcairo_w32_data *cd = (Rcairo_w32_data *) be->backendSpecific;
	return 0;
}

/*---- destroy ----*/
static void w32_backend_destroy(Rcairo_backend* be)
{
	Rcairo_w32_data *xd = (Rcairo_w32_data *) be->backendSpecific;

	if (xd->cdc) {
		DeleteDC(xd->cdc); xd->cdc=0;
		DeleteObject(xd->cb); xd->cb=0;
	}
	if (xd->gawin) {
		del(xd->gawin);
		doevent();
		xd->gawin=0;
	} else if (xd->wh) {
		DestroyWindow(xd->wh);
	}
	cairo_surface_destroy(be->cs);
	cairo_destroy(be->cc);	
	free(be);
}

/*-----------------*/

#ifdef NATIVE_UI

static int inited_w32 = 0;

typedef struct w32chain_s {
	HWND w;
	window gawin;
	Rcairo_backend *be;
	struct w32chain_s *next;
} w32chain_t;

static w32chain_t wchain;

static HWND      parent;   /* parent of all windows - i.e. the Rgui Workspace or similar */
static HINSTANCE instance; /* instance handle for the current process */
static int       isMDI;    /* set to 1 if we are hooked into Rgui's MDI, 0 otherwise */
static char*     appname;  /* name of the application we hook into (e.g. "Rgui") */

LRESULT CALLBACK WindowProc(HWND hwnd,  UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Rcairo_backend *be = 0;
	Rcairo_w32_data *xd = 0;
	w32chain_t *l = &wchain;
	while (l && l->be) {
		if (l->w == hwnd) {
			be = l->be;
			break;
		}
		l = l -> next;
	}
	if (!be) be=wchain.be; /*HACK: DEBUG: FIXME! */
	if (!be) {
		{ FILE *f=fopen("c:\\debug.txt","a"); fprintf(f,"Window: %x: be is NULL!\n", hwnd); fclose(f); }
		return
			isMDI?DefMDIChildProc(hwnd, uMsg, wParam, lParam):DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	xd = (Rcairo_w32_data*) be->backendSpecific;

	{ FILE *f=fopen("c:\\debug.txt","a"); fprintf(f,"Window: %x, msg: %x, [%x, %x]\n", (unsigned int)hwnd, (unsigned int)uMsg, (unsigned int)wParam, (unsigned int)lParam); fclose(f); }
	

	switch (uMsg) {
		/*
		  case WM_DESTROY:
		  PostQuitMessage(0);
		  break; 

		  case WM_QUERYENDSESSION:
		  return((long) TRUE);  // we agree to end session.                                                                            
		*/

	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;

	case WM_SIZE:
		{
			RECT r;
			GetClientRect( hwnd, &r );
			{ FILE *f=fopen("c:\\debug.txt","a"); fprintf(f," - WM_SIZE: %d x %d\n", r.right, r.bottom); fclose(f); }
			Rcairo_backend_resize(be, r.right, r.bottom);
		}
		return 0;

	case WM_PAINT:
		{
			RECT r, ur;

			{ FILE *f=fopen("c:\\debug.txt","a"); fprintf(f," - WM_PAINT: %d\n", (int)GetUpdateRect(hwnd,&ur,1)); fclose(f); }
			if (GetUpdateRect(hwnd,&ur,1)) {
				PAINTSTRUCT ps;
				HDC wdc=BeginPaint(hwnd,&ps);
				GetClientRect( hwnd, &r );
				BitBlt(wdc, 0, 0, r.right, r.bottom, xd->cdc, 0, 0, SRCCOPY);
				ValidateRect(hwnd, NULL);
				EndPaint(hwnd,&ps);
			}
			return 0;
		}

	default:
		return
			isMDI?DefMDIChildProc(hwnd, uMsg, wParam, lParam):DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

BOOL CALLBACK EnumWin( HWND hwnd, LPARAM lParam) {
	char buf[128];
	*buf=0;
	GetClassName(hwnd, buf, 127);
	Rprintf("win> %08x (%s)\n", hwnd, buf);
	return 1;
}
/* just for fun - an easter egg - never really used in Cairo */
void list_all_windows() {
	EnumWindows(&EnumWin,0);
}

static void get_R_window() {
	char mname[127];
	char fn[64];
	char *c = mname, *d = mname;
	instance = GetModuleHandle(0);
	GetModuleFileName(instance, mname, sizeof(mname));
	Rprintf(" Module name: '%s'\n", mname);
	/* GetFileTitle(mname, fn, sizeof(fn));  we do this manually so we don't need -lcomdlg32 */
	for (;*c;c++) if (*c=='/' || *c=='\\') d=c+1;
	c=d; while (*c) c++; c--; while (c>d && *c!='.') c--;
	if (*c=='.') *c=0;
	appname=strdup(d);
	strcpy(fn, d);
	strcat(fn, " Workspace");
	parent=FindWindow(fn, 0);
	if (parent)
		isMDI = 1;
	else
		parent = FindWindow(appname, 0);
}

#else

static void HelpClose(window w)
{
	Rcairo_backend *be = (Rcairo_backend *) getdata(w);
	Rcairo_backend_kill(be);
}

static void HelpExpose(window w, rect r)
{
	Rcairo_backend *be = (Rcairo_backend *) getdata(w);
	Rcairo_w32_data *xd = (Rcairo_w32_data *) be->backendSpecific;
	HWND hwnd = (HWND) ((ga_object*)(xd->gawin))->handle;
	RECT cr;
	
	HDC wdc=GetDC(hwnd);
	GetClientRect( hwnd, &cr );

	/*
	if (cr.right != xd->width || cr.bottom != xd->height) {
		ReleaseDC(hwnd, wdc);
		Rprintf("HelpExpose: size mismatch, calling resize\n");
		Rcairo_backend_resize(be, cr.right, cr.bottom);
		return;
		}*/

	BitBlt(wdc, 0, 0, cr.right, cr.bottom, xd->cdc, 0, 0, SRCCOPY);
	ValidateRect(hwnd, NULL);
	ReleaseDC(hwnd, wdc);
}

static void HelpResize(window w, rect r)
{
	Rcairo_backend *be = (Rcairo_backend *) getdata(w);
	Rcairo_backend_resize(be, r.width, r.height);
}

#endif

Rcairo_backend *Rcairo_new_w32_backend(Rcairo_backend *be, const char *display, double width, double height, double umpl)
{
	Rcairo_w32_data *xd;
#ifdef NATIVE_UI
	w32chain_t *l = &wchain;
#endif

	if ( ! (xd = (Rcairo_w32_data*) calloc(1,sizeof(Rcairo_w32_data)))) {
		free(be);
		return NULL;
	}

	be->backend_type = BET_W32;
	be->backendSpecific = xd;
	xd->be = be;
	be->destroy_backend = w32_backend_destroy;
	be->save_page = w32_save_page;
	be->resize = w32_resize;
	be->mode = w32_mode;
	be->locator = w32_locator;
	be->truncate_rect = 1;

	if (be->dpix<=0) { /* try to find out the DPI setting */
		HWND dw = GetDesktopWindow();
		if (dw) {
			HDC  dc = GetDC(dw);
			if (dc) {
				int dpix = GetDeviceCaps(dc, LOGPIXELSX);
				int dpiy = GetDeviceCaps(dc, LOGPIXELSY);
				ReleaseDC(dw, dc);
				if (dpix>0) be->dpix=(double)dpix;
				if (dpiy>0) be->dpiy=(double)dpiy;
			}
		}
	}
	/* adjust width and height to be in pixels */
	if (umpl>0 && be->dpix<=0) {
		be->dpix = 96; be->dpiy = 96;
	}
	if (be->dpiy==0 && be->dpix>0) be->dpiy=be->dpix;
	if (umpl>0) {
		width = width * umpl * be->dpix;
		height = height * umpl * be->dpiy;
		umpl=-1;
	}
	if (umpl!=-1) {
		width *= (-umpl);
		height *= (-umpl);
	}

	be->width = width;
	be->height = height;

#ifdef NATIVE_UI
	if (!inited_w32) {
		WNDCLASS wc;

		get_R_window();

		{ FILE *f=fopen("c:\\debug.txt","w"); fprintf(f,"init W32; isMDI=%d, parent=%x\n", isMDI, (unsigned int) parent); fclose(f); }

		wc.style=0; wc.lpfnWndProc=WindowProc;
		wc.cbClsExtra=0; wc.cbWndExtra=0;
		wc.hInstance=instance;
		wc.hIcon=0;
		wc.hCursor=LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
		wc.lpszMenuName=NULL;
		wc.lpszClassName="RCairoWindow";

		RegisterClass(&wc);

		wc.cbWndExtra=/* CBWNDEXTRA */ 1024;
		wc.lpszClassName="RCairoDoc";

		RegisterClass(&wc);

		inited_w32 = 1;
	}

	while (l->be && l->next) l=l->next;
	if (l->be) { l->next = (w32chain_t*) calloc(1,sizeof(w32chain_t)); l = l->next; }

	l->be = be;	
#else
	{
		window gawin = newwindow("Cairo Graphics", rect(20,20,width,height),
								 Document|StandardWindow);
		xd->gawin = gawin;
		xd->wh = (HWND) ((ga_object*)gawin)->handle;
		addto(gawin);
		gsetcursor(gawin, ArrowCursor);
		if (ismdi()) {
			int btsize = 24;
			control tb;

			tb = newtoolbar(btsize + 4);
			gsetcursor(tb, ArrowCursor);
			addto(tb);
		}
		addto(gawin);
		newmenubar(NULL);
		newmdimenu();
		show(gawin); /* twice, for a Windows bug */
		show(gawin);
		BringToTop(gawin, 0);
		setresize(gawin, HelpResize);
		setredraw(gawin, HelpExpose);
		setclose(gawin, HelpClose);
		setdata(gawin, (void *) be);
		w32_resize(be, width, height);
	}
#endif

#ifdef NATIVE_UI
	if (isMDI) {
		HWND mdiClient;
		CLIENTCREATESTRUCT ccs;
		HMENU rm = GetMenu(parent);
		HMENU wm = GetSubMenu(rm, 4);
		ccs.hWindowMenu = wm;
		ccs.idFirstChild = 512+0xE000; /* from graphapp/internal.h -> MinDocID */
		Rprintf("Menu = %x, Windows = %x, ID = %x\n", rm, wm, ccs.idFirstChild);

		mdiClient = CreateWindow("MDICLIENT","Cairo",WS_CHILD|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,width,height,
								 parent,NULL,instance,&ccs);
		ShowWindow(mdiClient, SW_SHOW);

		Rprintf("mdiClient: %x\n", mdiClient);
		{
			MDICREATESTRUCT mcs;
			mcs.szTitle = "Cairo device";
			mcs.szClass = "RCairoDoc";
			mcs.hOwner  = instance;
			mcs.x = CW_USEDEFAULT; mcs.cx = width;
			mcs.y = CW_USEDEFAULT; mcs.cy = height;
			mcs.style = WS_MAXIMIZE|WS_MINIMIZE|WS_CHILD|WS_VISIBLE;
			xd->wh = l->w = (HWND) SendMessage (mdiClient, WM_MDICREATE, 0, 
												(LONG) (LPMDICREATESTRUCT) &mcs);
			Rprintf("MDICREATE result: %x\n", l->w);
		}
		{ FILE *f=fopen("c:\\debug.txt","a"); fprintf(f,"parent: %x\nMDIclient: %x\ndoc: %x\n", parent, mdiClient, l->w); fclose(f); }
	} else {
		l->w = xd->wh = CreateWindow("RCairoWindow","Cairo",WS_OVERLAPPEDWINDOW,
									 100,100,width,height,
									 parent,NULL,instance,NULL);
	}

	ShowWindow(xd->wh, SW_SHOWNORMAL);

	w32_resize(be, width, height);

	UpdateWindow(xd->wh);
#endif

	return be;
}
#else
Rcairo_backend_def *RcairoBackendDef_w32 = 0;

Rcairo_backend *Rcairo_new_w32_backend(Rcairo_backend *be, const char *display, double width, double height, double umpl)
{
	error("cairo library was compiled without win32 back-end.");
	return NULL;
}
#endif
