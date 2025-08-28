AC_INIT([servicehisilicon], [0.1], [])
AM_INIT_AUTOMAKE([subdir-objects foreign dist-bzip2 no-define tar-pax])
AC_CONFIG_MACRO_DIR([m4])
LT_INIT

AC_PROG_CC
AC_PROG_CXX
AM_PATH_PYTHON
AX_PYTHON_DEVEL
AC_LANG(C++)

PKG_CHECK_MODULES(ENIGMA2, enigma2)

AC_DEFINE([DEBUG])

AC_ARG_WITH(binarycompatibledebug,
	AC_HELP_STRING([--with-binarycompatibledebug], [use e2 independent debug output]),
	[[with_binarycompatibledebug=$withval]],
	[[with_binarycompatibledebug=no]]
)

if test x"$with_binarycompatibledebug" = xyes ; then
	AC_DEFINE([BINARY_COMPATIBLE_DEBUGLOG])
fi

AC_ARG_WITH([ffmpeg-includes],
    [AS_HELP_STRING([--with-ffmpeg-includes=DIR], [Location of FFmpeg includes])],
    [FFMPEG_CFLAGS="-I$withval"], [FFMPEG_CFLAGS=""])

AC_ARG_WITH([ffmpeg-libs],
    [AS_HELP_STRING([--with-ffmpeg-libs=DIR], [Location of FFmpeg libs])],
    [FFMPEG_LIBS="-L$withval"], [FFMPEG_LIBS=""])

AC_SUBST(FFMPEG_CFLAGS)
AC_SUBST(FFMPEG_LIBS)

AC_CONFIG_FILES([
Makefile
servicehisilicon/Makefile
plugin/Makefile
])
AC_OUTPUT
