#ifndef RAPI_H__
#define RAPI_H__

#include <Rinternals.h>

SEXP Cairo_get_serial(SEXP dev);
SEXP Cairo_set_onSave(SEXP dev, SEXP fn);
SEXP Rcairo_capture(SEXP dev);
SEXP Rcairo_initialize(void);
SEXP Rcairo_snapshot(SEXP dev, SEXP sLast);
SEXP Rcairo_supported_types(void);
SEXP get_img_backplane(SEXP dev);
SEXP ptr_to_raw(SEXP ptr, SEXP off, SEXP len);
SEXP raw_to_ptr(SEXP ptr, SEXP woff, SEXP raw, SEXP roff, SEXP len);
SEXP cairo_create_new_device(SEXP args);
SEXP cairo_font_match(SEXP args);
SEXP cairo_font_set(SEXP args);

#endif
