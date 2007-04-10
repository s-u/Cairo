/* -*- mode: C; tab-width: 4; c-basic-offset: 4 -*-
   Copyright (C) 2007  Simon Urbanek
   License: GPL v2 */

#include "cconfig.h"

#ifdef SUPPORTS_TIFF

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>

int save_tiff_file(void *buf, int w, int h, char *fn, int channels) {
	tsize_t linebytes = channels * w;
	unsigned char *tbuf = NULL; 
	TIFF *out= TIFFOpen(fn, "w");
	unsigned int row =  0;
	if (!out) return -1;
	TIFFSetField (out, TIFFTAG_IMAGEWIDTH, w);
	TIFFSetField(out, TIFFTAG_IMAGELENGTH, h);
	TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, channels);
	TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	
	if (TIFFScanlineSize(out) < linebytes)
		tbuf = (unsigned char *)_TIFFmalloc(linebytes);
	else
		tbuf = (unsigned char *)_TIFFmalloc(TIFFScanlineSize(out));

	TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, w * channels));
	while (row < h)	{
		memcpy(tbuf, &((unsigned char*)buf)[(h-row-1)*linebytes], linebytes);
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
