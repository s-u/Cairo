/* -*- mode: C; tab-width: 4; c-basic-offset: 4 -*-
   Copyright (C) 2007  Simon Urbanek
   License: GPL v2 or GPL v3 */

#include "cconfig.h"

#ifdef SUPPORTS_JPEG

#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>

int save_jpeg_file(void *buf, int w, int h, char *fn, int quality, int channels) {
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *output_file = fopen(fn, "wb");
	JSAMPROW slr;

	if (!output_file) return -1;
	if (channels!=3 && channels!=4) return -1;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;
	cinfo.image_width=w;
	cinfo.image_height=h;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, (quality<25)?0:1);
	jpeg_stdio_dest(&cinfo, output_file);
	jpeg_start_compress(&cinfo, TRUE);
	if (channels == 3) {
		unsigned char *src = (unsigned char*) buf;
		int sl = 0;
		while (sl<h) {
			slr=src;
			jpeg_write_scanlines(&cinfo, &slr, 1);
			src+=w*3;
			h++;
		}
	} else {
		unsigned char *dst;
		unsigned int *src = (unsigned int*)buf;
		int sl = 0;
		dst = (unsigned char*) malloc(w*3);
		while (sl<h) {
			unsigned char *dp = dst;
			int x = 0;
			while (x < w) {
				dp[0] = (*src >> 16) & 255;
				dp[1] = (*src >> 8) & 255;
				dp[2] = *src & 255;
				src++; dp+=3;
				x++;
			}
			slr=dst;
			jpeg_write_scanlines(&cinfo, &slr, 1);
			sl++;
		}
		free(dst);
	}
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	fclose(output_file);
	return 0;
}

#else

	int save_jpeg_file(void *buf, int w, int h, char *fn, int quality, int channels) {
	return -2;
}

#endif
