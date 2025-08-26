/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 2008--2025  R Core Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, a copy is available at
 *  https://www.R-project.org/Licenses/
 */

/* This file is a fork of src/library/grDevices/src/devCairo.c
   including only parts that have been added to support advanced
   GE features not present when Cairo was written. */

#include "backend.h"
#include "cairogd.h"
#include "cairotalk.h"
#include "cairobem.h"
#include <Rversion.h>
#define USE_RINTERNALS 1
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h> /* attribute_visible */

#include "Rapi.h"

/* compatibility macros so we don't have to rename things */
#define pX11Desc CairoGDDesc*
#define pDevDesc NewDevDesc*
#define _(X) (X)
#define windowHeight cb->height
#define windowWidth  cb->width
/* xd->cc  ==>  xd_cc */
#define xd_cc xd->cb->cc
void Rcairo_set_line(CairoGDDesc* xd, R_GE_gcontext *gc); /* cairotalk.c */
#define CairoLineType(GC, XD) Rcairo_set_line(XD, GC)

static void CairoCol(unsigned int col, double* R, double* G, double* B)
{
    *R = R_RED(col)/255.0;
    *G = R_GREEN(col)/255.0;
    *B = R_BLUE(col)/255.0;
 /* *R = pow(*R, RedGamma);
    *G = pow(*G, GreenGamma);
    *B = pow(*B, BlueGamma); */
}

static void CairoColor(unsigned int col, pX11Desc xd)
{
    unsigned int alpha = R_ALPHA(col);
    double red, blue, green;

    CairoCol(col, &red, &green, &blue);

    /* This optimization should not be necessary, but alpha = 1 seems
       to cause image fallback in some backends */
    if (alpha == 255)
	cairo_set_source_rgb(xd_cc, red, green, blue);
    else
	cairo_set_source_rgba(xd_cc, red, green, blue, alpha/255.0);
}

/*
 ***************************
 * Patterns
 ***************************
 */

/* Just a starting value */
#define maxPatterns 64

static void CairoInitPatterns(pX11Desc xd)
{
    int i;
    xd->numPatterns = maxPatterns;
    xd->patterns = malloc(sizeof(cairo_pattern_t*) * xd->numPatterns);
    for (i = 0; i < xd->numPatterns; i++) {
        xd->patterns[i] = NULL;
    }
}

static int CairoGrowPatterns(pX11Desc xd)
{
    int i, newMax = 2*xd->numPatterns;
    void *tmp;
    tmp = realloc(xd->patterns, sizeof(cairo_pattern_t*) * newMax);
    if (!tmp) { 
        warning(_("Cairo patterns exhausted (failed to increase maxPatterns)"));
        return 0;
    }
    xd->patterns = tmp;
    for (i = xd->numPatterns; i < newMax; i++) {
        xd->patterns[i] = NULL;
    }
    xd->numPatterns = newMax;
    return 1;
}

static void CairoCleanPatterns(pX11Desc xd)
{
    int i;
    for (i = 0; i < xd->numPatterns; i++) {
        if (xd->patterns[i] != NULL) {
            cairo_pattern_destroy(xd->patterns[i]);
            xd->patterns[i] = NULL;
        }
    }    
}

static void CairoDestroyPatterns(pX11Desc xd)
{
    int i;
    for (i = 0; i < xd->numPatterns; i++) {
        if (xd->patterns[i] != NULL) {
            cairo_pattern_destroy(xd->patterns[i]);
        }
    }    
    free(xd->patterns);
}

static int CairoNewPatternIndex(pX11Desc xd)
{
    int i;
    for (i = 0; i < xd->numPatterns; i++) {
        if (xd->patterns[i] == NULL) {
            return i;
        } else {
            if (i == (xd->numPatterns - 1) &&
                !CairoGrowPatterns(xd)) {
                return -1;
            }
        }
    }    
    /* Should never get here, but just in case */
    warning(_("Cairo patterns exhausted"));
    return -1;
}

static cairo_pattern_t* CairoLinearGradient(SEXP gradient, pX11Desc xd)
{
    unsigned int col;
    unsigned int alpha;
    double red, blue, green;
    int i, nStops = R_GE_linearGradientNumStops(gradient);
    double stop;
    cairo_extend_t extend = CAIRO_EXTEND_NONE;
    cairo_pattern_t *cairo_gradient;  
    cairo_gradient = 
        cairo_pattern_create_linear(R_GE_linearGradientX1(gradient),
                                    R_GE_linearGradientY1(gradient),
                                    R_GE_linearGradientX2(gradient),
                                    R_GE_linearGradientY2(gradient));
    for (i = 0; i < nStops; i++) {
        col = R_GE_linearGradientColour(gradient, i);
        stop = R_GE_linearGradientStop(gradient, i);
        CairoCol(col, &red, &green, &blue);
        alpha = R_ALPHA(col);
        if (alpha == 255)
            cairo_pattern_add_color_stop_rgb(cairo_gradient, stop, 
                                             red, green, blue);
        else
            cairo_pattern_add_color_stop_rgba(cairo_gradient, stop, 
                                              red, green, blue, alpha/255.0);
    }
    switch(R_GE_linearGradientExtend(gradient)) {
    case R_GE_patternExtendNone: extend = CAIRO_EXTEND_NONE; break;
    case R_GE_patternExtendPad: extend = CAIRO_EXTEND_PAD; break;
    case R_GE_patternExtendReflect: extend = CAIRO_EXTEND_REFLECT; break;
    case R_GE_patternExtendRepeat: extend = CAIRO_EXTEND_REPEAT; break;
    }
    cairo_pattern_set_extend(cairo_gradient, extend);    
    return cairo_gradient;
}

static cairo_pattern_t* CairoRadialGradient(SEXP gradient, pX11Desc xd)
{
    unsigned int col;
    unsigned int alpha;
    double red, blue, green;
    int i, nStops = R_GE_radialGradientNumStops(gradient);
    double stop;
    cairo_extend_t extend = CAIRO_EXTEND_NONE;
    cairo_pattern_t *cairo_gradient;  
    cairo_gradient = 
        cairo_pattern_create_radial(R_GE_radialGradientCX1(gradient),
                                    R_GE_radialGradientCY1(gradient),
                                    R_GE_radialGradientR1(gradient),
                                    R_GE_radialGradientCX2(gradient),
                                    R_GE_radialGradientCY2(gradient),
                                    R_GE_radialGradientR2(gradient));
    for (i = 0; i < nStops; i++) {
        col = R_GE_radialGradientColour(gradient, i);
        stop = R_GE_radialGradientStop(gradient, i);
        CairoCol(col, &red, &green, &blue);
        alpha = R_ALPHA(col);
        if (alpha == 255)
            cairo_pattern_add_color_stop_rgb(cairo_gradient, stop, 
                                             red, green, blue);
        else
            cairo_pattern_add_color_stop_rgba(cairo_gradient, stop, 
                                              red, green, blue, alpha/255.0);
    }
    switch(R_GE_radialGradientExtend(gradient)) {
    case R_GE_patternExtendNone: extend = CAIRO_EXTEND_NONE; break;
    case R_GE_patternExtendPad: extend = CAIRO_EXTEND_PAD; break;
    case R_GE_patternExtendReflect: extend = CAIRO_EXTEND_REFLECT; break;
    case R_GE_patternExtendRepeat: extend = CAIRO_EXTEND_REPEAT; break;
    }
    cairo_pattern_set_extend(cairo_gradient, extend);    
    return cairo_gradient;
}

static cairo_pattern_t *CairoTilingPattern(SEXP pattern, pX11Desc xd)
{
    cairo_t *cc = xd_cc;
    SEXP R_fcall;
    cairo_pattern_t *cairo_tiling;
    cairo_extend_t extend = CAIRO_EXTEND_NONE;
    /* Start new group - drawing is redirected to this group */
    cairo_push_group(cc);
    /* Scale the drawing to fill the temporary group surface */
    cairo_matrix_t tm;
    cairo_matrix_init_identity(&tm);
    cairo_matrix_scale(&tm, 
                       xd->windowWidth/R_GE_tilingPatternWidth(pattern),
                       xd->windowHeight/R_GE_tilingPatternHeight(pattern));
    cairo_matrix_translate(&tm, 
                           -R_GE_tilingPatternX(pattern),
                           -R_GE_tilingPatternY(pattern));
    cairo_set_matrix(cc, &tm);
    /* Play the pattern function to build the pattern */
    R_fcall = PROTECT(lang1(R_GE_tilingPatternFunction(pattern)));
    eval(R_fcall, R_GlobalEnv);
    UNPROTECT(1);
    /* Close group and return resulting pattern */
    cairo_tiling = cairo_pop_group(cc);
    /* Scale the pattern to its proper size */
    cairo_matrix_init_identity(&tm);
    cairo_matrix_scale(&tm, 
                       xd->windowWidth/R_GE_tilingPatternWidth(pattern),
                       xd->windowHeight/R_GE_tilingPatternHeight(pattern));
    cairo_matrix_translate(&tm, 
                           -R_GE_tilingPatternX(pattern), 
                           -R_GE_tilingPatternY(pattern));
    cairo_pattern_set_matrix(cairo_tiling, &tm);
    switch(R_GE_tilingPatternExtend(pattern)) {
    case R_GE_patternExtendNone: extend = CAIRO_EXTEND_NONE; break;
    case R_GE_patternExtendPad: extend = CAIRO_EXTEND_PAD; break;
    case R_GE_patternExtendReflect: extend = CAIRO_EXTEND_REFLECT; break;
    case R_GE_patternExtendRepeat: extend = CAIRO_EXTEND_REPEAT; break;
    }
    cairo_pattern_set_extend(cairo_tiling, extend);
    return cairo_tiling;
}

static cairo_pattern_t *CairoCreatePattern(SEXP pattern, pX11Desc xd)
{
    cairo_pattern_t *cairo_pattern = NULL;
    switch(R_GE_patternType(pattern)) {
    case R_GE_linearGradientPattern: 
        cairo_pattern = CairoLinearGradient(pattern, xd);
        break;
    case R_GE_radialGradientPattern:
        cairo_pattern = CairoRadialGradient(pattern, xd);            
        break;
    case R_GE_tilingPattern:
        cairo_pattern = CairoTilingPattern(pattern, xd);
        break;
    }
    return cairo_pattern;
}

static int CairoSetPattern(SEXP pattern, pX11Desc xd)
{
    int index = CairoNewPatternIndex(xd);
    if (index >= 0) {
        cairo_pattern_t *cairo_pattern = CairoCreatePattern(pattern, xd);

        xd->patterns[index] = cairo_pattern;
    }
    return index;
}

static void CairoReleasePattern(int index, pX11Desc xd)
{
    if (xd->patterns[index]) {
        cairo_pattern_destroy(xd->patterns[index]);
        xd->patterns[index] = NULL;
    } else {
        warning(_("Attempt to release non-existent pattern"));
    }
}

static void CairoPatternFill(SEXP ref, pX11Desc xd)
{
    int index = INTEGER(ref)[0];
    if (index >= 0) {
        cairo_set_source(xd_cc, xd->patterns[index]);
    } else {
        /* Patterns may have been exhausted */
	cairo_set_source_rgba(xd_cc, 0.0, 0.0, 0.0, 0.0);
    }
    cairo_fill_preserve(xd_cc);
}

/*
 ***************************
 * Clipping paths
 ***************************
 */

/* Just a starting value */
#define maxClipPaths 64

static void CairoInitClipPaths(pX11Desc xd)
{
    int i;
    /* Zero clip paths */
    xd->numClipPaths = maxClipPaths;
    xd->clippaths = malloc(sizeof(cairo_path_t*) * xd->numClipPaths);
    for (i = 0; i < xd->numClipPaths; i++) {
        xd->clippaths[i] = NULL;
    }
}

static int CairoGrowClipPaths(pX11Desc xd)
{
    int i, newMax = 2*xd->numClipPaths;
    void *tmp;
    tmp = realloc(xd->clippaths, sizeof(cairo_path_t*) * newMax);
    if (!tmp) { 
        warning(_("Cairo clipping paths exhausted (failed to increase maxClipPaths)"));
        return 0;
    }
    xd->clippaths = tmp;
    for (i = xd->numClipPaths; i < newMax; i++) {
        xd->clippaths[i] = NULL;
    }
    xd->numClipPaths = newMax;
    return 1;
}

static void CairoCleanClipPaths(pX11Desc xd)
{
    int i;
    for (i = 0; i < xd->numClipPaths; i++) {
        if (xd->clippaths[i] != NULL) {
            cairo_path_destroy(xd->clippaths[i]);
            xd->clippaths[i] = NULL;
        }
    }    
}

static void CairoDestroyClipPaths(pX11Desc xd)
{
    int i;
    for (i = 0; i < xd->numClipPaths; i++) {
        if (xd->clippaths[i] != NULL) {
            cairo_path_destroy(xd->clippaths[i]);
            xd->clippaths[i] = NULL;
        }
    }    
    free(xd->clippaths);
}

static int CairoNewClipPathIndex(pX11Desc xd)
{
    int i;
    for (i = 0; i < xd->numClipPaths; i++) {
        if (xd->clippaths[i] == NULL) {
            return i;
        } else {
            if (i == (xd->numClipPaths - 1) &&
                !CairoGrowClipPaths(xd)) {
                return -1;
            }
        }
    }    
    warning(_("Cairo clipping paths exhausted"));
    return -1;
}

static cairo_path_t* CairoCreateClipPath(SEXP clipPath, int index, pX11Desc xd)
{
    cairo_t *cc = xd_cc;
    SEXP R_fcall;
    cairo_path_t *cairo_clippath;
    /* Save the current path */
    cairo_path_t *cairo_saved_path = cairo_copy_path(cc);
    /* Increment the "appending" count */
    xd->appending++;
    /* Clear the current path */
    cairo_new_path(cc);
    /* Play the clipPath function to build the clipping path */
    R_fcall = PROTECT(lang1(clipPath));
    eval(R_fcall, R_GlobalEnv);
    UNPROTECT(1);
    /* Apply path fill rule */
    switch (R_GE_clipPathFillRule(clipPath)) {
    case R_GE_nonZeroWindingRule: 
        cairo_set_fill_rule(xd_cc, CAIRO_FILL_RULE_WINDING); break;
    case R_GE_evenOddRule:
        cairo_set_fill_rule(xd_cc, CAIRO_FILL_RULE_EVEN_ODD); break;
    }
    /* Set the clipping region from the path */
    cairo_reset_clip(cc);
    cairo_clip_preserve(cc);
    /* Save the clipping path (for reuse) */
    cairo_clippath = cairo_copy_path(cc);
    /* Clear the path again */
    cairo_new_path(cc);
    /* Decrement the "appending" count */
    xd->appending--;
    /* Restore the saved path */
    cairo_append_path(cc, cairo_saved_path);
    /* Destroy the saved path */
    cairo_path_destroy(cairo_saved_path);
    /* Return the clipping path */
    return cairo_clippath;
}

static void CairoReuseClipPath(cairo_path_t *cairo_clippath, pX11Desc xd)
{
    cairo_t *cc = xd_cc;
    /* Save the current path */
    cairo_path_t *cairo_saved_path = cairo_copy_path(cc);
    /* Clear the current path */
    cairo_new_path(cc);
    /* Append the clipping path */
    cairo_append_path(cc, cairo_clippath);
    /* Set the clipping region from the path (which clears the path) */
    cairo_reset_clip(cc);
    cairo_clip(cc);
    /* Restore the saved path */
    cairo_append_path(cc, cairo_saved_path);
    /* Destroy the saved path */
    cairo_path_destroy(cairo_saved_path);
}

static SEXP CairoSetClipPath(SEXP path, SEXP ref, pX11Desc xd)
{
    cairo_path_t *cairo_clippath;
    SEXP newref = R_NilValue;
    int index;

    if (isNull(ref)) {
        /* Must generate new ref */
        index = CairoNewClipPathIndex(xd);
        if (index < 0) {
            /* Unless we have run out of space */
        } else {
            /* Create this clipping path */
            cairo_clippath = CairoCreateClipPath(path, index, xd);
            xd->clippaths[index] = cairo_clippath;
            PROTECT(newref = allocVector(INTSXP, 1));
            INTEGER(newref)[0] = index;
            UNPROTECT(1);
        }
    } else {
        /* Reuse indexed clip path */
        int index = INTEGER(ref)[0];
        if (xd->clippaths[index]) {
            CairoReuseClipPath(xd->clippaths[index], xd);
        } else {
            /* BUT if index clip path does not exist, create a new one */
            cairo_clippath = CairoCreateClipPath(path, index, xd);
            xd->clippaths[index] = cairo_clippath;
            warning(_("Attempt to reuse non-existent clipping path"));
        }
    }

    return newref;
}

static void CairoReleaseClipPath(int index, pX11Desc xd)
{
    if (xd->clippaths[index]) {
        cairo_path_destroy(xd->clippaths[index]);
        xd->clippaths[index] = NULL;
    } else {
        warning(_("Attempt to release non-existent clipping path"));
    }
}

/*
 ***************************
 * Masks
 ***************************
 */

/* Just a starting value */
#define maxMasks 64

static void CairoInitMasks(pX11Desc xd)
{
    int i;
    xd->numMasks = 20;
    xd->masks = malloc(sizeof(cairo_pattern_t*) * xd->numMasks);
    for (i = 0; i < xd->numMasks; i++) {
        xd->masks[i] = NULL;
    }
    xd->currentMask = -1;
}

static int CairoGrowMasks(pX11Desc xd)
{
    int i, newMax = 2*xd->numMasks;
    void *tmp;
    tmp = realloc(xd->masks, sizeof(cairo_pattern_t*) * newMax);
    if (!tmp) { 
        warning(_("Cairo masks exhausted (failed to increase maxMasks)"));
        return 0;
    }
    xd->masks = tmp;
    for (i = xd->numMasks; i < newMax; i++) {
        xd->masks[i] = NULL;
    }
    xd->numMasks = newMax;
    return 1;
}

static void CairoCleanMasks(pX11Desc xd)
{
    int i;
    for (i = 0; i < xd->numMasks; i++) {
        if (xd->masks[i] != NULL) {
            cairo_pattern_destroy(xd->masks[i]);
            xd->masks[i] = NULL;
        }
    }    
    xd->currentMask = -1;
}

static void CairoDestroyMasks(pX11Desc xd)
{
    int i;
    for (i = 0; i < xd->numMasks; i++) {
        if (xd->masks[i] != NULL) {
            cairo_pattern_destroy(xd->masks[i]);
            xd->masks[i] = NULL;
        }
    }    
    free(xd->masks);
}

static int CairoNewMaskIndex(pX11Desc xd)
{
    int i;
    for (i = 0; i < xd->numMasks; i++) {
        if (xd->masks[i] == NULL) {
            return i;
        } else {
            if (i == (xd->numMasks - 1) &&
                !CairoGrowMasks(xd)) {
                return -1;
            }
        }
    }    
    warning(_("Cairo masks exhausted"));
    return -1;
}

static cairo_pattern_t *CairoCreateMask(SEXP mask, pX11Desc xd)
{
    cairo_t *cc = xd_cc;
    SEXP R_fcall;
    /* Start new group - drawing is redirected to this group */
    cairo_push_group(cc);
    /* Start with "over" operator */
    cairo_set_operator(cc, CAIRO_OPERATOR_OVER);
    /* Play the mask function to build the mask */
    R_fcall = PROTECT(lang1(mask));
    eval(R_fcall, R_GlobalEnv);
    UNPROTECT(1);
    /* Close group and return resulting mask */
    return cairo_pop_group(cc);
}

static SEXP CairoSetMask(SEXP mask, SEXP ref, pX11Desc xd)
{
    int index;
    cairo_pattern_t *cairo_mask;
    SEXP newref = R_NilValue;

    if (isNull(mask)) {
        /* Set NO mask */
        index = -1;
    } else if (R_GE_maskType(mask) == R_GE_luminanceMask) {
        warning(_("Ignored luminance mask (not supported on this device)"));
        /* Set NO mask */
        index = -1;        
    } else {
        if (isNull(ref)) {
            /* Create a new mask */
            index = CairoNewMaskIndex(xd);
            if (index >= 0) {
                cairo_mask = CairoCreateMask(mask, xd);
                xd->masks[index] = cairo_mask;
            }
        } else {
            /* Reuse existing mask */
            index = INTEGER(ref)[0];
            if (index >= 0 && !xd->masks[index]) {
                /* But if it does not exist, make a new one */
                index = CairoNewMaskIndex(xd);
                if (index >= 0) {
                    cairo_mask = CairoCreateMask(mask, xd);
                    xd->masks[index] = cairo_mask;
                }
            }
        }
        newref = PROTECT(allocVector(INTSXP, 1));
        INTEGER(newref)[0] = index;
        UNPROTECT(1);
    }

    xd->currentMask = index;

    return newref;
}

static void CairoReleaseMask(int index, pX11Desc xd)
{
    if (xd->masks[index]) {
        cairo_pattern_destroy(xd->masks[index]);
        xd->masks[index] = NULL;
    } else {
        warning(_("Attempt to release non-existent mask"));
    }
}

/*
 ***************************
 * Groups
 ***************************
 */

/* Just a starting value */
#define maxGroups 64

static void CairoInitGroups(pX11Desc xd)
{
    int i;
    xd->numGroups = 20;
    xd->groups = malloc(sizeof(cairo_pattern_t*) * xd->numGroups);
    for (i = 0; i < xd->numGroups; i++) {
        xd->groups[i] = NULL;
    }
    xd->nullGroup = cairo_pattern_create_rgb(0, 0, 0);
    xd->currentGroup = -1;
}

static int CairoGrowGroups(pX11Desc xd)
{
    int i, newMax = 2*xd->numGroups;
    void *tmp;
    tmp = realloc(xd->groups, sizeof(cairo_pattern_t*) * newMax);
    if (!tmp) { 
        warning(_("Cairo groups exhausted (failed to increase maxGroups)"));
        return 0;
    }
    xd->groups = tmp;
    for (i = xd->numGroups; i < newMax; i++) {
        xd->groups[i] = NULL;
    }
    xd->numGroups = newMax;
    return 1;
}

static void CairoCleanGroups(pX11Desc xd)
{
    int i;
    for (i = 0; i < xd->numGroups; i++) {
        if (xd->groups[i] != NULL &&
            xd->groups[i] != xd->nullGroup) {
            cairo_pattern_destroy(xd->groups[i]);
            xd->groups[i] = NULL;
        }
    }    
    xd->currentGroup = -1;
}

static void CairoDestroyGroups(pX11Desc xd)
{
    int i;
    for (i = 0; i < xd->numGroups; i++) {
        if (xd->groups[i] != NULL &&
            xd->groups[i] != xd->nullGroup) {
            cairo_pattern_destroy(xd->groups[i]);
            xd->groups[i] = NULL;
        }
    }    
    free(xd->groups);
    cairo_pattern_destroy(xd->nullGroup);
}

static int CairoNewGroupIndex(pX11Desc xd)
{
    int i;
    for (i = 0; i < xd->numGroups; i++) {
        if (xd->groups[i] == NULL) {
            /* Place temporary hold on this slot in case of 
             * group within group */
            xd->groups[i] = xd->nullGroup;
            return i;
        } else {
            if (i == (xd->numGroups - 1) &&
                !CairoGrowGroups(xd)) {
                return -1;
            }
        }
    }    
    warning(_("Cairo groups exhausted"));
    return -1;
}


static cairo_operator_t CairoOperator(int op) 
{
    cairo_operator_t cairo_op = CAIRO_OPERATOR_OVER;
    switch(op) {
    case R_GE_compositeClear: cairo_op = CAIRO_OPERATOR_CLEAR; break;
    case R_GE_compositeSource: cairo_op = CAIRO_OPERATOR_SOURCE; break;
    case R_GE_compositeOver: cairo_op = CAIRO_OPERATOR_OVER; break;
    case R_GE_compositeIn: cairo_op = CAIRO_OPERATOR_IN; break;
    case R_GE_compositeOut: cairo_op = CAIRO_OPERATOR_OUT; break;
    case R_GE_compositeAtop: cairo_op = CAIRO_OPERATOR_ATOP; break;
    case R_GE_compositeDest: cairo_op = CAIRO_OPERATOR_DEST; break;
    case R_GE_compositeDestOver: cairo_op = CAIRO_OPERATOR_DEST_OVER; break;
    case R_GE_compositeDestIn: cairo_op = CAIRO_OPERATOR_DEST_IN; break;
    case R_GE_compositeDestOut: cairo_op = CAIRO_OPERATOR_DEST_OUT; break;
    case R_GE_compositeDestAtop: cairo_op = CAIRO_OPERATOR_DEST_ATOP; break;
    case R_GE_compositeXor: cairo_op = CAIRO_OPERATOR_XOR; break;
    case R_GE_compositeAdd: cairo_op = CAIRO_OPERATOR_ADD; break;
    case R_GE_compositeSaturate: cairo_op = CAIRO_OPERATOR_SATURATE; break;
    case R_GE_compositeMultiply: cairo_op = CAIRO_OPERATOR_MULTIPLY; break;
    case R_GE_compositeScreen: cairo_op = CAIRO_OPERATOR_SCREEN; break;
    case R_GE_compositeOverlay: cairo_op = CAIRO_OPERATOR_OVERLAY; break;
    case R_GE_compositeDarken: cairo_op = CAIRO_OPERATOR_DARKEN; break;
    case R_GE_compositeLighten: cairo_op = CAIRO_OPERATOR_LIGHTEN; break;
    case R_GE_compositeColorDodge: cairo_op = CAIRO_OPERATOR_COLOR_DODGE; break;
    case R_GE_compositeColorBurn: cairo_op = CAIRO_OPERATOR_COLOR_BURN; break;
    case R_GE_compositeHardLight: cairo_op = CAIRO_OPERATOR_HARD_LIGHT; break;
    case R_GE_compositeSoftLight: cairo_op = CAIRO_OPERATOR_SOFT_LIGHT; break;
    case R_GE_compositeDifference: cairo_op = CAIRO_OPERATOR_DIFFERENCE; break;
    case R_GE_compositeExclusion: cairo_op = CAIRO_OPERATOR_EXCLUSION; break;
    }
    return cairo_op;
}

static cairo_pattern_t *CairoCreateGroup(SEXP src, int op, SEXP dst, 
                                         pX11Desc xd)
{
    cairo_t *cc = xd_cc;
    cairo_pattern_t *cairo_group;
    SEXP R_fcall;

    /* Start new group - drawing is redirected to this group */
    cairo_push_group(cc);
    /* Start with "over" operator */
    cairo_set_operator(cc, CAIRO_OPERATOR_OVER);

    if (dst != R_NilValue) {
        /* Play the destination function to draw the destination */
        R_fcall = PROTECT(lang1(dst));
        eval(R_fcall, R_GlobalEnv);
        UNPROTECT(1);
    }
    /* Set the group operator */
    cairo_set_operator(cc, CairoOperator(op));
    /* Play the source function to draw the source */
    R_fcall = PROTECT(lang1(src));
    eval(R_fcall, R_GlobalEnv);
    UNPROTECT(1);

    /* Close group and return the resulting pattern */
    cairo_group = cairo_pop_group(cc);

    return cairo_group;
}

static SEXP CairoDefineGroup(SEXP src, int op, SEXP dst, pX11Desc xd)
{
    cairo_pattern_t *cairo_group;
    SEXP ref;
    int index;

    /* Create a new group */
    index = CairoNewGroupIndex(xd);
    if (index >= 0) {
        int savedGroup = xd->currentGroup;
        xd->currentGroup = index;
        cairo_group = CairoCreateGroup(src, op, dst, xd);
        xd->currentGroup = savedGroup;
        xd->groups[index] = cairo_group;
    }

    ref = PROTECT(allocVector(INTSXP, 1));
    INTEGER(ref)[0] = index;
    UNPROTECT(1);

    return ref;
}

bool cairoBegin(pX11Desc xd);
void cairoEnd(bool grouping, pX11Desc xd);

static void CairoUseGroup(SEXP ref, SEXP trans, pX11Desc xd)
{
    cairo_t *cc = xd_cc;
    int index;
    cairo_matrix_t transform;
    bool grouping = false;

    index = INTEGER(ref)[0];
    if (index < 0) {
        warning(_("Groups exhausted"));
        return;
    }

    if (index >= 0 && !xd->groups[index]) {
        warning("Unknown group ");
        return;
    } 

    if (!xd->appending) {
        grouping = cairoBegin(xd);
    }

    /* Draw the group */
    cairo_save(cc);
    
    if (trans != R_NilValue) {
        transform.xx = REAL(trans)[0];
        transform.yx = REAL(trans)[3];
        transform.xy = REAL(trans)[1];
        transform.yy = REAL(trans)[4];
        transform.x0 = REAL(trans)[2];
        transform.y0 = REAL(trans)[5];
        
        cairo_transform(cc, &transform);
    } 

    cairo_set_source(cc, xd->groups[index]);
    cairo_paint(cc);

    cairo_restore(cc);

    if (!xd->appending) {
        cairoEnd(grouping, xd);
    }
}

static void CairoReleaseGroup(int index, pX11Desc xd)
{
    if (xd->groups[index]) {
        cairo_pattern_destroy(xd->groups[index]);
        xd->groups[index] = NULL;
    } else {
        warning(_("Attempt to release non-existent group"));
    }
}


/*
 ***************************
 * Rendering
 ***************************
 */
static bool implicitGroup(pX11Desc xd) {
    return xd->currentGroup >= 0 && 
        (cairo_get_operator(xd_cc) == CAIRO_OPERATOR_CLEAR ||
         cairo_get_operator(xd_cc) == CAIRO_OPERATOR_SOURCE);
}

/* Set up for drawing a shape */
bool cairoBegin(pX11Desc xd)
{
    bool grouping = implicitGroup(xd);
    if (xd->currentMask >= 0) {
        /* If masking, draw temporary pattern */
        cairo_push_group(xd_cc);
    }
    if (grouping) {
        cairo_push_group(xd_cc);
    }
    return grouping;
}

void cairoEnd(bool grouping, pX11Desc xd)
{
    if (grouping) {
        cairo_pattern_t *source = cairo_pop_group(xd_cc);
        cairo_set_source(xd_cc, source);
        cairo_paint(xd_cc);
        cairo_pattern_destroy(source);            
    }
    if (xd->currentMask >= 0) {
        /* If masking, use temporary pattern as source and mask that */
        cairo_pattern_t *source = cairo_pop_group(xd_cc);
        cairo_pattern_t *mask = xd->masks[xd->currentMask];
        cairo_set_source(xd_cc, source);
        cairo_mask(xd_cc, mask);
        /* Release temporary pattern */
        cairo_pattern_destroy(source);
    }
}

static void cairoStroke(const pGEcontext gc, pX11Desc xd) 
{
    if (R_ALPHA(gc->col) > 0 && gc->lty != -1) {
        CairoColor(gc->col, xd);
        CairoLineType(gc, xd);
        cairo_stroke(xd_cc);
    }
}

void cairoFill(const pGEcontext gc, pX11Desc xd)
{
    /* patternFill overrides fill */
    if (gc->patternFill != R_NilValue) {
        CairoPatternFill(gc->patternFill, xd);
    } else if (R_ALPHA(gc->fill) > 0) {
        cairo_set_antialias(xd_cc, CAIRO_ANTIALIAS_NONE);
        CairoColor(gc->fill, xd);
        cairo_fill_preserve(xd_cc);
        cairo_set_antialias(xd_cc, xd->antialias);
    }
}

static SEXP Cairo_SetPattern(SEXP pattern, pDevDesc dd) 
{
    pX11Desc xd = (pX11Desc) dd->deviceSpecific;
    SEXP ref;
    PROTECT(ref = allocVector(INTSXP, 1));
    INTEGER(ref)[0] = CairoSetPattern(pattern, xd);
    UNPROTECT(1);
    return ref;
}

static void Cairo_ReleasePattern(SEXP ref, pDevDesc dd) 
{
    pX11Desc xd = (pX11Desc) dd->deviceSpecific;
    /* NULL means release all patterns */
    if (ref == R_NilValue) {
        CairoCleanPatterns(xd);
    } else {
        CairoReleasePattern(INTEGER(ref)[0], xd);
    }
}

static SEXP Cairo_SetClipPath(SEXP path, SEXP ref, pDevDesc dd) 
{
    pX11Desc xd = (pX11Desc) dd->deviceSpecific;
    return CairoSetClipPath(path, ref, xd);
}

static void Cairo_ReleaseClipPath(SEXP ref, pDevDesc dd) 
{
    pX11Desc xd = (pX11Desc) dd->deviceSpecific;
    /* NULL means release all patterns */
    if (isNull(ref)) {
        CairoCleanClipPaths(xd);
    } else {
        int i;
        for (i = 0; i < LENGTH(ref); i++) {
            CairoReleaseClipPath(INTEGER(ref)[i], xd);
        }
    }
}

static SEXP Cairo_SetMask(SEXP mask, SEXP ref, pDevDesc dd) 
{
    pX11Desc xd = (pX11Desc) dd->deviceSpecific;
    return CairoSetMask(mask, ref, xd);
}

static void Cairo_ReleaseMask(SEXP ref, pDevDesc dd) 
{
    pX11Desc xd = (pX11Desc) dd->deviceSpecific;
    /* NULL means release all patterns */
    if (isNull(ref)) {
        CairoCleanMasks(xd);
    } else {
        int i;
        for (i = 0; i < LENGTH(ref); i++) {
            CairoReleaseMask(INTEGER(ref)[i], xd);
        }
    }
}

static SEXP Cairo_DefineGroup(SEXP source, int op, SEXP destination, 
                              pDevDesc dd)
{
    pX11Desc xd = (pX11Desc) dd->deviceSpecific;
    return CairoDefineGroup(source, op, destination, xd);
}

static void Cairo_UseGroup(SEXP ref, SEXP trans, pDevDesc dd)
{
    pX11Desc xd = (pX11Desc) dd->deviceSpecific;
    CairoUseGroup(ref, trans, xd);
}

static void Cairo_ReleaseGroup(SEXP ref, pDevDesc dd)
{
    pX11Desc xd = (pX11Desc) dd->deviceSpecific;
    /* NULL means release all groups */
    if (isNull(ref)) {
        CairoCleanGroups(xd);
    } else {
        int i;
        for (i = 0; i < LENGTH(ref); i++) {
            CairoReleaseGroup(INTEGER(ref)[i], xd);
        }
    }
}

/*
 ***************************
 * Stroking and filling paths
 ***************************
 */
static void CairoStrokePath(SEXP path,
                            const pGEcontext gc, 
                            pX11Desc xd)
{
    cairo_t *cc = xd_cc;
    SEXP R_fcall;
    bool grouping = false;

    if (!xd->appending) {
        grouping = cairoBegin(xd);
    }

    /* Increment the "appending" count */
    xd->appending++;
    /* Clear the current path */
    cairo_new_path(cc);
    /* Play the path function to build the path */
    R_fcall = PROTECT(lang1(path));
    eval(R_fcall, R_GlobalEnv);
    UNPROTECT(1);
    /* Decrement the "appending" count */
    xd->appending--;
    /* Stroke the path */

    if (!xd->appending) {
        if (R_ALPHA(gc->col) > 0 && gc->lty != -1) {
            cairoStroke(gc, xd);
        }
        cairoEnd(grouping, xd);
    }
}

static void Cairo_Stroke(SEXP path, const pGEcontext gc, pDevDesc dd)
{
    pX11Desc xd = (pX11Desc) dd->deviceSpecific;
    CairoStrokePath(path, gc, xd);    
}

static void CairoFillPath(SEXP path,
                          int rule,
                          const pGEcontext gc, 
                          pX11Desc xd)
{
    cairo_t *cc = xd_cc;
    SEXP R_fcall;
    bool grouping = false;

    if (!xd->appending) {
        grouping = cairoBegin(xd);
    }

    /* Increment the "appending" count */
    xd->appending++;
    /* Clear the current path */
    cairo_new_path(cc);
    /* Play the path function to build the path */
    R_fcall = PROTECT(lang1(path));
    eval(R_fcall, R_GlobalEnv);
    UNPROTECT(1);
    /* Decrement the "appending" count */
    xd->appending--;
    /* Stroke the path */

    if (!xd->appending) {
        /* patternFill overrides fill */
        if (gc->patternFill != R_NilValue || 
            R_ALPHA(gc->fill) > 0) { 
            switch (rule) {
            case R_GE_nonZeroWindingRule: 
                cairo_set_fill_rule(xd_cc, CAIRO_FILL_RULE_WINDING); break;
            case R_GE_evenOddRule:
                cairo_set_fill_rule(xd_cc, CAIRO_FILL_RULE_EVEN_ODD); break;
            }
            cairoFill(gc, xd);
        }
        cairoEnd(grouping, xd);
        if (!grouping) xd->cb->serial++;
    }
}

static void Cairo_Fill(SEXP path, int rule, const pGEcontext gc, pDevDesc dd)
{
    pX11Desc xd = (pX11Desc) dd->deviceSpecific;
    CairoFillPath(path, rule, gc, xd);    
}

static void CairoFillStrokePath(SEXP path,
                                int rule,
                                const pGEcontext gc, 
                                pX11Desc xd)
{
    cairo_t *cc = xd_cc;
    SEXP R_fcall;
    /* Increment the "appending" count */
    xd->appending++;
    /* Clear the current path */
    cairo_new_path(cc);
    /* Play the path function to build the path */
    R_fcall = PROTECT(lang1(path));
    eval(R_fcall, R_GlobalEnv);
    UNPROTECT(1);
    /* Decrement the "appending" count */
    xd->appending--;
}

static void CairoFillStroke(SEXP path, int rule, 
                            const pGEcontext gc, pDevDesc dd, int op)
{
    pX11Desc xd = (pX11Desc) dd->deviceSpecific;

    bool grouping = cairoBegin(xd);
    CairoFillStrokePath(path, rule, gc, xd);
    if (op) { /* fill */
        cairoFill(gc, xd);
    } else {
        cairoStroke(gc, xd);
    }
    cairoEnd(grouping, xd);
    if (!grouping) xd->cb->serial++;
}

static void Cairo_FillStroke(SEXP path, int rule, 
                             const pGEcontext gc, pDevDesc dd)
{
    pX11Desc xd = (pX11Desc) dd->deviceSpecific;
    if (xd->appending) {
        CairoFillStrokePath(path, rule, gc, xd);
    } else {
        bool fill = (gc->patternFill != R_NilValue) || 
            (R_ALPHA(gc->fill) > 0);
        bool stroke = (R_ALPHA(gc->col) > 0 && gc->lty != -1);
        if (fill) {
            switch (rule) {
            case R_GE_nonZeroWindingRule: 
                cairo_set_fill_rule(xd_cc, CAIRO_FILL_RULE_WINDING); break;
            case R_GE_evenOddRule:
                cairo_set_fill_rule(xd_cc, CAIRO_FILL_RULE_EVEN_ODD); break;
            }
        }
        if (fill && stroke) {
            CairoFillStroke(path, rule, gc, dd, 1);
            CairoFillStroke(path, rule, gc, dd, 0);
        } else if (fill) {
            CairoFillStroke(path, rule, gc, dd, 1);
        } else if (stroke) {
            CairoFillStroke(path, rule, gc, dd, 0);
        }
    }
}

/*
 ***************************
 * Device capabilities
 ***************************
 */

static SEXP Cairo_Capabilities(SEXP capabilities) {
    /* NOTE: although some of the features below were added in
       GE 13 the capability defines didn't come until 15 */
#if R_GE_version >= 15
    SEXP patterns = PROTECT(allocVector(INTSXP, 3));
    INTEGER(patterns)[0] = R_GE_linearGradientPattern;
    INTEGER(patterns)[1] = R_GE_radialGradientPattern;
    INTEGER(patterns)[2] = R_GE_tilingPattern;
    SET_VECTOR_ELT(capabilities, R_GE_capability_patterns, patterns);
    UNPROTECT(1);

    SEXP clippingPaths = PROTECT(allocVector(INTSXP, 1));
    INTEGER(clippingPaths)[0] = 1;
    SET_VECTOR_ELT(capabilities, R_GE_capability_clippingPaths, clippingPaths);
    UNPROTECT(1);

    SEXP masks = PROTECT(allocVector(INTSXP, 1));
    INTEGER(masks)[0] = R_GE_alphaMask;
    SET_VECTOR_ELT(capabilities, R_GE_capability_masks, masks);
    UNPROTECT(1);

    SEXP compositing = PROTECT(allocVector(INTSXP, 25));
    INTEGER(compositing)[0] = R_GE_compositeMultiply;
    INTEGER(compositing)[1] = R_GE_compositeScreen;
    INTEGER(compositing)[2] = R_GE_compositeOverlay;
    INTEGER(compositing)[3] = R_GE_compositeDarken;
    INTEGER(compositing)[4] = R_GE_compositeLighten;
    INTEGER(compositing)[5] = R_GE_compositeColorDodge;
    INTEGER(compositing)[6] = R_GE_compositeColorBurn;
    INTEGER(compositing)[7] = R_GE_compositeHardLight;
    INTEGER(compositing)[8] = R_GE_compositeSoftLight;
    INTEGER(compositing)[9] = R_GE_compositeDifference;
    INTEGER(compositing)[10] = R_GE_compositeExclusion;
    INTEGER(compositing)[11] = R_GE_compositeClear;
    INTEGER(compositing)[12] = R_GE_compositeSource;
    INTEGER(compositing)[13] = R_GE_compositeOver;
    INTEGER(compositing)[14] = R_GE_compositeIn;
    INTEGER(compositing)[15] = R_GE_compositeOut;
    INTEGER(compositing)[16] = R_GE_compositeAtop;
    INTEGER(compositing)[17] = R_GE_compositeDest;
    INTEGER(compositing)[18] = R_GE_compositeDestOver;
    INTEGER(compositing)[19] = R_GE_compositeDestIn;
    INTEGER(compositing)[20] = R_GE_compositeDestOut;
    INTEGER(compositing)[21] = R_GE_compositeDestAtop;
    INTEGER(compositing)[22] = R_GE_compositeXor;
    INTEGER(compositing)[23] = R_GE_compositeAdd;
    INTEGER(compositing)[24] = R_GE_compositeSaturate;
    SET_VECTOR_ELT(capabilities, R_GE_capability_compositing, compositing);
    UNPROTECT(1);

    SEXP transforms = PROTECT(allocVector(INTSXP, 1));
    INTEGER(transforms)[0] = 1;
    SET_VECTOR_ELT(capabilities, R_GE_capability_transformations, transforms);
    UNPROTECT(1);

    SEXP paths = PROTECT(allocVector(INTSXP, 1));
    INTEGER(paths)[0] = 1;
    SET_VECTOR_ELT(capabilities, R_GE_capability_paths, paths);
    UNPROTECT(1);

#if R_GE_version >= 16
    SEXP glyphs = PROTECT(allocVector(INTSXP, 1));
    INTEGER(glyphs)[0] = 1;
    SET_VECTOR_ELT(capabilities, R_GE_capability_glyphs, glyphs);
    UNPROTECT(1);

#if R_GE_version >= 17
    SEXP variableFonts = PROTECT(allocVector(INTSXP, 1));
    INTEGER(variableFonts)[0] = 1;
    SET_VECTOR_ELT(capabilities, R_GE_capability_variableFonts, variableFonts);
    UNPROTECT(1);
#endif /* 17 */
#endif /* 16 */
#endif /* 15 */
    return capabilities;
}

/* added interfaces to init xd and dd */
void CairoGD_CoreUtil_Init(pX11Desc xd) {
    CairoInitPatterns(xd);
    CairoInitClipPaths(xd);
    CairoInitMasks(xd);
    CairoInitGroups(xd);
    xd->appending = 0;
    xd->lwdscale = 1.0;
    xd->antialias = CAIRO_ANTIALIAS_DEFAULT;
}

void CairoGD_CoreUtil_Destroy(pX11Desc xd) {
    CairoDestroyGroups(xd);
    CairoDestroyMasks(xd);
    CairoDestroyClipPaths(xd);
    CairoDestroyPatterns(xd);
}

void CairoGD_CoreUtil_SetupFn(pDevDesc dd) {
#if R_GE_version >= 13
    dd->setPattern = Cairo_SetPattern;
    dd->releasePattern = Cairo_ReleasePattern;
    dd->setClipPath = Cairo_SetClipPath;
    dd->releaseClipPath = Cairo_ReleaseClipPath;
    dd->setMask = Cairo_SetMask;
    dd->releaseMask = Cairo_ReleaseMask;
#endif
#if R_GE_version >= 14
    dd->defineGroup = Cairo_DefineGroup;
    dd->useGroup = Cairo_UseGroup;
    dd->releaseGroup = Cairo_ReleaseGroup;
    dd->stroke = Cairo_Stroke;
    dd->fill = Cairo_Fill;
    dd->fillStroke = Cairo_FillStroke;
    dd->capabilities = Cairo_Capabilities;
#endif
    /* NOTE: we already support 15 (glyphs) natively */
}
