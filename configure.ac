# Process this file with autoconf to produce a configure script.
AC_INIT(Cairo, 1.2, Simon.Urbanek@r-project.org)
AC_CONFIG_SRCDIR([src/cairogd.c])
AC_CONFIG_HEADER([src/cconfig.h])

# find R home and set CC/CFLAGS
: ${R_HOME=`R RHOME`}
if test -z "${R_HOME}"; then
  echo "could not determine R_HOME"
  exit 1
fi
CC=`${R_HOME}/bin/R CMD config CC`;
CFLAGS=`${R_HOME}/bin/R CMD config CFLAGS`
AC_SUBST(R_HOME)

# platform check
#AC_CANONICAL_HOST
#case "${host_os}" in
#  darwin*)
#    AC_DEFINE(DARWIN, 1, [Set if the OS is Darwin/Mac OS X.])
#    ;;
#esac

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
#AC_HEADER_TIME

# Checks for library functions.
#AC_FUNC_MALLOC
#AC_TYPE_SIGNAL

AC_PATH_PROG([PKGCONF],[pkg-config],[],[$PATH:/usr/local/bin:ext/bin:ext])

have_cairo=no

if test "xx$CAIRO_LIBS" == "xx"; then
   if test "xx$PKGCONF" != "xx"; then
      AC_MSG_CHECKING([whether pkg-config knows about cairo])
      if ${PKGCONF} --exists cairo; then
         AC_MSG_RESULT([yes])
	 CAIRO_CFLAGS=`${PKGCONF} --cflags cairo`
	 CAIRO_LIBS=`${PKGCONF} --libs cairo`
      else
	AC_MSG_RESULT([no])
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
AC_LINK_IFELSE([
#include "cairo.h"
int main(void) {
#ifndef CAIRO_HAS_PNG_FUNCTIONS
#error no PNG support
#endif
    return 0;
}],[AC_MSG_RESULT([yes])] ,[AC_MSG_RESULT([no])
AC_MSG_ERROR([Cairo library has no PNG support.])])

uses_ats=no
AC_MSG_CHECKING([for ATS font support in Cairo])
AC_LINK_IFELSE([
#include "cairo.h"
int main(void) {
#ifndef CAIRO_HAS_ATSUI_FONT
#error no PNG support
#endif
    return 0;
}],[AC_MSG_RESULT([yes])
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

#AC_CHECK_LIB(cairo, cairo_create,,
#[AC_MSG_ERROR([Cannot link to cairo library. Please check config.log for details and set CAIRO_LIBS accordingly.])])

AC_SEARCH_LIBS(deflate, z)

AC_MSG_CHECKING([whether Cairo programs can be compiled])
AC_LINK_IFELSE([
#include "cairo.h"
int main(void) {
  cairo_surface_t *cs = 0;
  cairo_t *cr;
  cr = cairo_create(cs);
  return 0;
}
],[AC_MSG_RESULT(yes)],
	[AC_MSG_ERROR([Cannot compile a simple Cairo program. See config.log for details.])])

# --- check for JPEG support ---
AC_MSG_CHECKING([wheter libjpeg works])
zLIBS="${LIBS}"
LIBS="${LIBS} -ljpeg"
AC_LINK_IFELSE([
#include <stdio.h>
#include <jpeglib.h>
int main(void) {
  struct jpeg_compress_struct cinfo;
  jpeg_create_compress(&cinfo);
  return 0;
}
],[ AC_MSG_RESULT([yes])
    AC_DEFINE(SUPPORTS_JPEG, 1, [Define to 1 if libjpeg is present and working])],
  [ AC_MSG_RESULT(no)
    LIBS="${zLIBS}"])

# --- check for TIFF support
AC_MSG_CHECKING([wheter libtiff works])
zLIBS="${LIBS}"
LIBS="${LIBS} -ltiff"
AC_LINK_IFELSE([
#include <tiffio.h>
int main(void) {
  TIFF *out = TIFFOpen("new.tif", "w");
  TIFFSetField (out, TIFFTAG_IMAGEWIDTH, 100);
  return 0;
}
],[ AC_MSG_RESULT([yes])
    AC_DEFINE(SUPPORTS_TIFF, 1, [Define to 1 if libtiff is present and working])],
  [ AC_MSG_RESULT(no)
    LIBS="${zLIBS}"])

AC_CONFIG_FILES([src/Makevars])
AC_OUTPUT
