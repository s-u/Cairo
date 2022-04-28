# Process this file with autoconf to produce a configure script.
AC_INIT([Cairo],[1.3],[Simon.Urbanek@r-project.org])
AC_CONFIG_SRCDIR([src/cairogd.c])
AC_CONFIG_HEADERS([src/cconfig.h])

# find R home and set CC/CFLAGS
: ${R_HOME=`R RHOME`}
if test -z "${R_HOME}"; then
  echo "could not determine R_HOME"
  exit 1
fi
RBIN="${R_HOME}/bin/R"
CC=`"${RBIN}" CMD config CC`;
CFLAGS=`"${RBIN}" CMD config CFLAGS`
CPPFLAGS=`"${RBIN}" CMD config CPPFLAGS`
LDFLAGS=`"${RBIN}" CMD config LDFLAGS`
CPP="${CC} -E"
AC_SUBST(R_HOME)

# Checks for programs.
AC_LANG(C)
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

AC_PATH_PROG([PKGCONF],[pkg-config],[],[$PATH:/usr/local/bin:ext/bin:ext:/sw/bin:/opt/bin])

have_cairo=no
if test "xx$CAIRO_LIBS" == "xx"; then
   if test "xx$PKGCONF" != "xx"; then
      AC_MSG_CHECKING([whether pkg-config knows about cairo])
      if "${PKGCONF}" --exists cairo; then
         AC_MSG_RESULT([yes])
	 AC_MSG_CHECKING([for configurable backends])
	 modlist="cairo"
	 for module in cairo-ft cairo-pdf cairo-png cairo-ps cairo-svn cairo-xlib cairo-xlib-xrender cairo-win32; do
	    if "${PKGCONF}" --exists ${module}; then
		modlist="${modlist} ${module}"
	    fi
	 done
	 AC_MSG_RESULT(${modlist})
	 CAIRO_CFLAGS=`"${PKGCONF}" --cflags ${modlist}`
	 CAIRO_LIBS=`"${PKGCONF}" --libs ${modlist}`
	 AC_MSG_CHECKING([whether --static is needed])
	 saveLIBS="$LIBS"
	 LIBS="$saveLIBS ${CAIRO_LIBS}"
	 AC_LINK_IFELSE([AC_LANG_FUNC_LINK_TRY(cairo_reset_clip)],
	     [AC_MSG_RESULT([no])],
	     [CAIRO_LIBS=`"${PKGCONF}" --libs --static ${modlist}`
	      LIBS="$saveLIBS ${CAIRO_LIBS}"
	      AC_LINK_IFELSE([AC_LANG_FUNC_LINK_TRY(cairo_reset_clip)],
	      [AC_MSG_RESULT([yes])
	       PKGCONF_LIB_ADD=--static
	      ],
	      [AC_MSG_RESULT([neither works, abandoning pkg-config])
	       CAIRO_LIBS=''
	      ])
	     ])
	 LIBS="$saveLIBS"
      else
	 AC_MSG_RESULT([no])
      fi
   fi
fi
if test -z "${CAIRO_LIBS}"; then
   AC_MSG_NOTICE([CAIRO_LIBS is unset, attempting to guess it.])
   cpre=''
   for pre in /usr /usr/local /usr/X11 /usr/X11R6 /opt /sw; do
      if test -e "${pre}/include/cairo/cairo.h"; then
         cpre=${pre}; break
      fi
   done
   if test -n "${cpre}"; then
      CAIRO_CFLAGS="-I${cpre}/include/cairo"
      if test "${cpre}" = /usr; then
         CAIRO_LIBS="-lcairo"
      else
         CAIRO_LIBS="-L${cpre}/lib -lcairo"
      fi
   fi
fi

AC_MSG_NOTICE([CAIRO_CFLAGS=${CAIRO_CFLAGS}])

# Check for Rconn patch
AC_MSG_CHECKING([if R was compiled with the RConn patch])
if test -f "${R_HOME}/include/R_ext/RConn.h"; then
	AC_MSG_RESULT([yes])
	HAVE_RCONN_H="-DHAVE_RCONN_H"
else
	AC_MSG_RESULT([no])
	HAVE_RCONN_H=""
fi

CPPFLAGS="$CPPFLAGS ${CAIRO_CFLAGS} ${HAVE_RCONN_H}"

AC_CHECK_HEADER(cairo.h,,AC_MSG_ERROR([Cannot find cairo.h! Please install cairo (http://www.cairographics.org/) and/or set CAIRO_CFLAGS/LIBS correspondingly.]))

AC_MSG_CHECKING([for PNG support in Cairo])
AC_LINK_IFELSE([AC_LANG_SOURCE([
#include "cairo.h"
int main(void) {
#ifndef CAIRO_HAS_PNG_FUNCTIONS
#error no PNG support
#endif
    return 0;
}])],[AC_MSG_RESULT([yes])] ,[AC_MSG_RESULT([no])
AC_MSG_ERROR([Cairo library has no PNG support.])])

uses_ats=no
AC_MSG_CHECKING([for ATS font support in Cairo])
AC_LINK_IFELSE([AC_LANG_SOURCE([
#include "cairo.h"
int main(void) {
#ifndef CAIRO_HAS_ATSUI_FONT
#error no PNG support
#endif
    return 0;
}])],[AC_MSG_RESULT([yes])
uses_ats=yes],[AC_MSG_RESULT([no])])

if test "xx${CAIRO_LIBS}" == "xx"; then
   if test $uses_ats == "yes"; then
      CAIRO_LIBS="-lcairo -lpng -lz -framework ApplicationServices"
   else
      CAIRO_LIBs="-lcairo -lpng -lz"
   fi
fi

AC_MSG_NOTICE([CAIRO_LIBS=${CAIRO_LIBS}])

LIBS="$LIBS ${CAIRO_LIBS}"

AC_SEARCH_LIBS(deflate, z)

AC_MSG_CHECKING([whether Cairo programs can be compiled])
AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <cairo.h>
int main(void) {
  cairo_surface_t *cs = 0;
  cairo_t *cr;
  cr = cairo_create(cs);
  return 0;
}
])],[AC_MSG_RESULT(yes)],
[AC_MSG_ERROR([Cannot compile a simple Cairo program. See config.log for details.])])


dnl AC_CHECK_LIB(cairo, cairo_image_surface_get_format)
dnl if test "$ac_cv_lib_cairo_cairo_image_surface_get_format" != "yes"; then
dnl   AC_MSG_ERROR([Cairo is too old: >= 1.2 is needed.])
dnl fi
AC_CHECK_DECLS([cairo_image_surface_get_format], [#include <cairo.h>])

has_cairo_ft=no

AC_ARG_WITH([freetype],
  AS_HELP_STRING(--with-freetype,[compile support for better font handling using FreeType and Fontconfig if present and supported by cairo. (default is @<:@auto@:>@).]),
        [with_freetype=$withval], [with_freetype=auto])

AC_MSG_CHECKING([whether FreeType support is desired])
AC_MSG_RESULT(${with_freetype})

if test x${with_freetype} != xno; then ## with_freetype
AC_MSG_CHECKING([for FreeType support in cairo])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <cairo.h>
#ifndef CAIRO_HAS_FT_FONT
#error nope, no ft
#endif
])],[has_cairo_ft=yes])
AC_MSG_RESULT(${has_cairo_ft})

need_xtra_ft_flags=no
if test "${has_cairo_ft}" = yes; then
 AC_MSG_CHECKING([whether FreeType needs additional flags])
 AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <cairo.h>
#include <cairo-ft.h>
int main(void) {
   cairo_ft_font_face_create_for_pattern(0);
   return 0;
}
])],[],[need_xtra_ft_flags=yes])
 AC_MSG_RESULT(${need_xtra_ft_flags})
fi

if test "${need_xtra_ft_flags}" = yes; then
 if test -n "${PKGCONF}"; then
   AC_MSG_CHECKING([whether pkg-config knows about fontconfig or freetype2])
   ftmod=''
   for module in fontconfig freetype2; do
      if "${PKGCONF}" --exists ${module}; then ftmod="${ftmod} ${module}"; fi
   done
   if test -n "${ftmod}"; then
      AC_MSG_RESULT(yes)
      FT_CPPFLAGS=`"${PKGCONF}" --cflags ${ftmod}`
      FT_LIBS=`"${PKGCONF}" --libs ${PKGCONF_LIB_ADD} ${ftmod}`
   else
      AC_MSG_RESULT(no)
   fi
 fi
 if test -z "${FT_LIBS}"; then
   AC_MSG_CHECKING([whether fontconfig/freetype2 location can be guessed])
   FCI=''; FTI=''; FCL=''; FTL=''
   for pre in /usr /usr/X11 /usr/X11R6 /usr/local /opt /sw; do
     if test -z "${FCI}" -a -e "${pre}/include/fontconfig/fontconfig.h"; then
        FCI="-I${pre}/include"
	FCL="-L${pre}/lib"
     fi
     if test -z "${FTI}" -a -e "${pre}/include/freetype2/freetype/freetype.h"; then
        FTI="-I${pre}/include/freetype2"
	FTL="-L${pre}/lib"
     fi
   done
   if test "${FCI}" = -I/usr/include; then FCI=''; fi
   if test "${FCL}" = "-L/usr/lib"; then FCL=''; fi
   if test "${FTL}" = "-L/usr/lib"; then FTL=''; fi
   if test "${FTL}" = "${FCL}"; then FTL=''; fi
   if test -z "${FCI}" -a -z "${FTI}"; then
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([Cannot find fontconfig/freetype2 although cairo claims to support it. Please check your cairo installation and/or update cairo if necessary or set CAIRO_CFLAGS/CAIRO_LIBS accordingly.])
   fi
   FT_CPPFLAGS="${FCI} ${FTI}"
   FT_LIBS="${FCL} ${FTL} -lfontconfig -lfreetype -lz"
   AC_MSG_RESULT([possibly])
 fi
 zLIBS="${LIBS}"
 LIBS="${LIBS} ${FT_LIBS}"
 zCPPFLAGS="${CPPFLAGS}"
 CPPFLAGS="${CPPFLAGS} ${FT_CPPFLAGS}"
 AC_MSG_CHECKING([whether additional flags work])
 AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <cairo.h>
#include <cairo-ft.h>
int main(void) {
   cairo_ft_font_face_create_for_pattern(0);
   return 0;
}
 ])],[AC_MSG_RESULT(yes)
 CAIRO_LIBS="${CAIRO_LIBS} ${FT_LIBS}"
 CAIRO_CFLAGS="${CAIRO_CFLAGS} ${FT_CPPFLAGS}"
 ],[
 AC_MSG_RESULT(no)
 AC_MSG_ERROR([Cannot use cairo-ft backend, although cairo claims it is working. Please check your caito installation and/or update cairo if necessary or set CAIRO_CFLAGS/CAIRO_LIBS accordingly.])
 ])
fi
fi ## with_freetype

# --- check for JPEG support ---
AC_MSG_CHECKING([whether libjpeg works])
zLIBS="${LIBS}"
LIBS="${LIBS} -ljpeg"
AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <stdio.h>
#include <jpeglib.h>
int main(void) {
  struct jpeg_compress_struct cinfo;
  jpeg_create_compress(&cinfo);
  return 0;
}
])],[ AC_MSG_RESULT([yes])
    AC_DEFINE(SUPPORTS_JPEG, 1, [Define to 1 if libjpeg is present and working])],
  [ AC_MSG_RESULT(no)
    LIBS="${zLIBS}"])

# --- check for Harfbuzz/ICU support ---

AC_ARG_WITH([harfbuzz],
  AS_HELP_STRING(--with-harfbuzz,[compile support for bi-directional Unicode and advanced text layout (like ligature support) using Harfbuzz and ICU libraries. (default is @<:@auto@:>@).]),
        [with_harfbuzz=$withval], [with_harfbuzz=auto])

AC_MSG_CHECKING([whether Harfbuzz layout is desired])
AC_MSG_RESULT(${with_harfbuzz})
if test x${with_harfbuzz} != xno; then
if test x${has_cairo_ft} != xyes; then
  if test x${with_harfbuzz} = xyes; then
    AC_MSG_ERROR([ERROR: Harfbuzz support is requested, but FreeType is not available.
    Harfbuzz requires cairo with FreeType support to be present.])
  else
    AC_MSG_NOTICE([NOTE: Harfbuzz requires cairo with FreeType support which is not available.])
  fi
  with_harfbuzz=no
fi
fi
if test x${with_harfbuzz} != xno; then
AC_MSG_CHECKING([whether Harfbuzz + ICU works])
zLIBS="${LIBS}"
zCPPFLAGS="${CPPFLAGS}"
if test -n "$PKGCONF"; then
: ${HB_CPPFLAGS=`"${PKGCONF}" --cflags harfbuzz-icu icu-uc`}
: ${HB_LIBS=`"${PKGCONF}" --libs ${PKGCONF_LIB_ADD} harfbuzz-icu icu-uc`}
fi
if test -z "${HB_LIBS}"; then
  HB_LIBS='-lharfbuzz-icu -licuuc -licudata -lharfbuzz'
fi
## ICU uses C++ (sadly) so requires stdc++ - this may not be very portable ...
LIBS="${LIBS} ${HB_LIBS} -lstdc++"
CPPFLAGS="${CPPFLAGS} ${HB_CPPFLAGS}"

AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <unicode/ubidi.h>
#include <unicode/uchar.h>

#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb-icu.h>
int main(void) {
  UBiDi *bidi = 0;
  UChar *text = 0;
  bidi = ubidi_open();
  hb_buffer_t *buf = hb_buffer_create();
  hb_buffer_set_unicode_funcs(buf, hb_icu_get_unicode_funcs());
  return 0;
}
])],[ AC_MSG_RESULT([yes])
    AC_DEFINE(HAVE_HARFBUZZ, 1, [Define to 1 if Harfbuzz and ICU are present and working])],
  [ AC_MSG_RESULT(no)
    LIBS="${zLIBS}"
    CPPFLAGS="${zCPPFLAGS}"
    HB_CPPFLAGS=''
    ])
fi

# --- check for TIFF support
AC_MSG_CHECKING([whether libtiff works])
zLIBS="${LIBS}"
if test -n "$PKGCONF"; then
  ## FIXME: --cflags?
  TIFF_LIBS=`"${PKGCONF}" --libs ${PKGCONF_LIB_ADD} libtiff-4`
fi
if test -z "${TIFF_LIBS}"; then
  TIFF_LIBS=-ltiff
fi
LIBS="${LIBS} ${TIFF_LIBS}"

AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <tiffio.h>
int main(void) {
  TIFF *out = TIFFOpen("new.tif", "w");
  TIFFSetField (out, TIFFTAG_IMAGEWIDTH, 100);
  return 0;
}
])],[ AC_MSG_RESULT([yes])
    AC_DEFINE(SUPPORTS_TIFF, 1, [Define to 1 if libtiff is present and working])],
  [ AC_MSG_RESULT(no)
    LIBS="${zLIBS}"])

## our CPPFLAGS included R's CPPFLAGS which we don't need to
## set so only include those we detected in addition to the
## R CMD config CPPFLAGS
CPPFLAGS="${CAIRO_CFLAGS} ${HB_CPPFLAGS} ${HAVE_RCONN_H}"

AC_ARG_ENABLE([debug],
[AS_HELP_STRING([--enable-debug],[enable debug flags and output. @<:@no@:>@])],
[want_debug="${enableval}"],
[want_debug=no])
if test ${want_debug} = yes; then
CPPFLAGS="-DJGD_DEBUG=1 ${CPPFLAGS}"
fi

AC_CONFIG_FILES([src/Makevars])
AC_OUTPUT
