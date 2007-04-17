#ifndef __IMG_TIFF_H__
#define __IMG_TIFF_H__

/* compression values: */
#define TIFF_COMPR_NONE 1
#define TIFF_COMPR_RLE  2
#define TIFF_COMPR_LZW  5
#define TIFF_COMPR_JPEG 7

/* the source is assumed to be 32-bit aligned pixels plane (even if channels=3) in
   native endianness format. really supported are only channels values 3 (RGB Tiff)
   and 4 (RGBA Tiff). compression value of 0 means that no flag will be set */
int save_tiff_file(void *buf, int w, int h, char *fn, int channels, int compression);

#endif
