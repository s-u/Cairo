/* -*- mode: C; tab-width: 4; c-basic-offset: 4 -*-
   Created 4/11/2007 by Simon Urbanek
   Copyright (C) 2007        Simon Urbanek

   License: GPL v2
*/

#include <stdlib.h>
#include <string.h>
#include "w32-backend.h"

#if CAIRO_HAS_WIN32_SURFACE

#include <R.h>
#include <R_ext/eventloop.h>
#include <Rdevices.h>
#include <R_ext/GraphicsEngine.h>

typedef struct {
	Rcairo_backend *be; /* back-link */
	HWND    wh;         /* window handle */
	HDC     cdc;        /* cache device context */
	HBITMAP cb;         /* cache bitmap */
	int     width;
	int     height;
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
static void w32_resize(Rcairo_backend* be, int width, int height){
	Rcairo_w32_data *xd = (Rcairo_w32_data *) be->backendSpecific;
	RECT r;
	HBITMAP ob;
	HDC hdc;
	if (!xd) return;
	if (xd->cdc) {
		DeleteDC(xd->cdc);
		DeleteObject(xd->cb);
	}

	xd->width=width;
	xd->height=height;

	/* first re-paint the window */
	hdc = GetDC( xd->wh );
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

	ReleaseDC(xd->wh, hdc);
}

/*---- mode ---- (0=done, 1=busy, 2=locator) */
static void w32_mode(Rcairo_backend* be, int which){
	if (which == 0)	{
		Rcairo_w32_data *xd = (Rcairo_w32_data *) be->backendSpecific;
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

	DestroyWindow(xd->wh);
	cairo_surface_destroy(be->cs);
	cairo_destroy(be->cc);	
	free(be);
}

/*-----------------*/

static int inited_w32 = 0;

typedef struct w32chain_s {
	HWND w;
	Rcairo_backend *be;
	struct w32chain_s *next;
} w32chain_t;

static w32chain_t wchain;

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
	if (!be)
		return DefWindowProc(hwnd, uMsg, wParam, lParam);

	xd = (Rcairo_w32_data*) be->backendSpecific;

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
			Rcairo_backend_resize(be, r.right, r.bottom);
		}
		return 0;

	case WM_PAINT:
		{
			RECT r, ur;

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
		return(DefWindowProc(hwnd, uMsg, wParam, lParam));
	}
	return 0;
}

Rcairo_backend *Rcairo_new_w32_backend(char *display, int width, int height)
{
	Rcairo_backend *be;
	Rcairo_w32_data *xd;
	w32chain_t *l = &wchain;
	HINSTANCE instance = GetModuleHandle(0);

	if ( ! (be = (Rcairo_backend*) calloc(1,sizeof(Rcairo_backend))))
		return NULL;
	if ( ! (xd = (Rcairo_w32_data*) calloc(1,sizeof(Rcairo_w32_data)))) {
		free(be);
		return NULL;
	}

	be->backendSpecific = xd;
	xd->be = be;
	be->destroy_backend = w32_backend_destroy;
	be->save_page = w32_save_page;
	be->resize = w32_resize;
	be->mode = w32_mode;
	be->locator = w32_locator;

	if (!inited_w32) {
		WNDCLASS wc;
		wc.style=0; wc.lpfnWndProc=WindowProc;
		wc.cbClsExtra=0; wc.cbWndExtra=0;
		wc.hInstance=instance;
		wc.hIcon=0;
		wc.hCursor=LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
		wc.lpszMenuName=NULL;
		wc.lpszClassName="RCairoWindow";

		RegisterClass(&wc);
		inited_w32 = 1;
	}

	while (l->be && l->next) l=l->next;
	if (l->be) { l->next = (w32chain_t*) calloc(1,sizeof(w32chain_t)); l = l->next; }
	l->w = xd->wh = CreateWindow("RCairoWindow","Cairo",WS_OVERLAPPEDWINDOW,100,100,width,height,
									 NULL,NULL,instance,NULL);
	l->be = be;

	ShowWindow(xd->wh, SW_SHOWNORMAL);

	w32_resize(be, width, height);

	UpdateWindow(xd->wh);

	return be;
}
#else
Rcairo_backend *Rcairo_new_w32_backend(char *filename, int width, int height)
{
	error("cairo library was compiled without win32 back-end.");
	return NULL;
}
#endif
