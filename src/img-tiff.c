/* -*- mode: C; tab-width: 4; c-basic-offset: 4 -*-
   Copyright (C) 2007  Simon Urbanek
   License: GPL v2 or GPL v3 */

#include "cconfig.h"

#ifdef SUPPORTS_TIFF

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>

int save_tiff_file(void *buf, int w, int h, char *fn, int channels, int compression) {
	tsize_t linebytes = channels * w;
	unsigned char *tbuf = NULL; 
	TIFF *out= TIFFOpen(fn, "w");
	unsigned int row =  0;
	unsigned short extra[1] = { EXTRASAMPLE_ASSOCALPHA };
	if (!out) return -1;
	TIFFSetField (out, TIFFTAG_IMAGEWIDTH, w);
	TIFFSetField(out, TIFFTAG_IMAGELENGTH, h);
	TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, channels);
	TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	if (channels>3) 
		TIFFSetField(out, TIFFTAG_EXTRASAMPLES, (unsigned short) 1, extra);
	if (compression) TIFFSetField(out, TIFFTAG_COMPRESSION, compression);

	/* Rprintf(" %d x %d, %d chs, scanls=%d, strips=%d, compr=%d\n", w, h, channels, TIFFScanlineSize(out), TIFFDefaultStripSize(out, w * channels), compression); */

	if (TIFFScanlineSize(out) < linebytes)
		tbuf = (unsigned char *)_TIFFmalloc(linebytes);
	else
		tbuf = (unsigned char *)_TIFFmalloc(TIFFScanlineSize(out));

	TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, w * channels));
	while (row < h)	{
		unsigned int *src = (unsigned int*)(&((unsigned char*)buf)[row*w*4]);
		unsigned char *dst = tbuf;
		int x = 0;
		while (x < w) {
			*dst = (*src >> 16) & 255; dst++;
			*dst = (*src >> 8) & 255; dst++;
			*dst = *src & 255; dst++;
			if (channels>3) {
				*dst = *src >> 24; dst++;
			}
			src++; x++;
		}
		if (TIFFWriteScanline(out, tbuf, row, 0) < 0) break;
		row++;
	}
	TIFFClose(out);
	if (tbuf)
		_TIFFfree(tbuf);
	return 0;
}

#else

int save_tiff_file(void *buf, int w, int h, char *fn, int channels) {
	return -2;
}

#endif
