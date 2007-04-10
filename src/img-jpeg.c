/* -*- mode: C; tab-width: 4; c-basic-offset: 4 -*-
   Copyright (C) 2007  Simon Urbanek
   License: GPL v2 */

#include "cconfig.h"

#ifdef SUPPORTS_JPEG

#include <stdio.h>
#include <jpeglib.h>

int save_jpeg_file(void *buf, int w, int h, char *fn, int quality) {
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *output_file = fopen(fn, "wb");
	
	if (!output_file) return -1;
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
	jpeg_write_scanlines(&cinfo, buf, h);
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	fclose(output_file);
	return 0;
}

#else

int save_jpeg_file(void *buf, int w, int h, char *fn, int quality) {
	return -2;
}

#endif
