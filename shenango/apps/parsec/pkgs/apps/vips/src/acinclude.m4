dnl From FIND_MOTIF and ACX_PTHREAD, without much understanding
dnl
dnl FIND_ZIP[ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]]
dnl ------------------------------------------------
dnl
dnl Find ZIP libraries and headers
dnl
dnl Put includes stuff in ZIP_INCLUDES
dnl Put link stuff in ZIP_LIBS
dnl Define HAVE_ZIP if found
dnl
AC_DEFUN([FIND_ZIP], [
AC_REQUIRE([AC_PATH_XTRA])

ZIP_INCLUDES=""
ZIP_LIBS=""

AC_ARG_WITH(zip, 
  AS_HELP_STRING([--without-zip], [build without libx (default: test)]))
# Treat --without-zip like --without-zip-includes --without-zip-libraries.
if test "$with_zip" = "no"; then
  ZIP_INCLUDES=no
  ZIP_LIBS=no
fi

AC_ARG_WITH(zip-includes,
  AS_HELP_STRING([--with-zip-includes=DIR], [libz includes are in DIR]),
  ZIP_INCLUDES="-I$withval")
AC_ARG_WITH(zip-libraries,
  AS_HELP_STRING([--with-zip-libraries=DIR], [libz libraries are in DIR]),
  ZIP_LIBS="-L$withval -lz")

AC_MSG_CHECKING(for ZIP)

# Look for zlib.h 
if test "$ZIP_INCLUDES" = ""; then
  # Check the standard search path
  AC_TRY_COMPILE([#include <zlib.h>],[int a;],[
    ZIP_INCLUDES=""
  ], [
    # zlib.h is not in the standard search path, try
    # $prefix
    zip_save_INCLUDES="$INCLUDES"

    INCLUDES="-I${prefix}/include $INCLUDES"

    AC_TRY_COMPILE([#include <zlib.h>],[int a;],[
      ZIP_INCLUDES="-I${prefix}/include"
    ], [
      ZIP_INCLUDES="no"
    ])

    INCLUDES=$zip_save_INCLUDES
  ])
fi

# Now for the libraries
if test "$ZIP_LIBS" = ""; then
  zip_save_LIBS="$LIBS"
  zip_save_INCLUDES="$INCLUDES"

  LIBS="-lz $LIBS"
  INCLUDES="$ZIP_INCLUDES $INCLUDES"

  # Try the standard search path first
  AC_TRY_LINK([#include <zlib.h>],[zlibVersion()], [
    ZIP_LIBS="-lz"
  ], [
    # libz is not in the standard search path, try $prefix

    LIBS="-L${prefix}/lib $LIBS"

    AC_TRY_LINK([#include <zlib.h>],[zlibVersion()], [
      ZIP_LIBS="-L${prefix}/lib -lz"
    ], [
      ZIP_LIBS=no
    ])
  ])

  LIBS="$zip_save_LIBS"
  INCLUDES="$zip_save_INCLUDES"
fi

AC_SUBST(ZIP_LIBS)
AC_SUBST(ZIP_INCLUDES)

# Print a helpful message
zip_libraries_result="$ZIP_LIBS"
zip_includes_result="$ZIP_INCLUDES"

if test x"$zip_libraries_result" = x""; then
  zip_libraries_result="in default path"
fi
if test x"$zip_includes_result" = x""; then
  zip_includes_result="in default path"
fi

if test "$zip_libraries_result" = "no"; then
  zip_libraries_result="(none)"
fi
if test "$zip_includes_result" = "no"; then
  zip_includes_result="(none)"
fi

AC_MSG_RESULT([libraries $zip_libraries_result, headers $zip_includes_result])

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test "$ZIP_INCLUDES" != "no" && test "$ZIP_LIBS" != "no"; then
  AC_DEFINE(HAVE_ZIP,1,[Define if you have libz libraries and header files.])
  $1
else
  ZIP_LIBS=""
  ZIP_INCLUDES=""
  $2
fi

])dnl

dnl From FIND_MOTIF and ACX_PTHREAD, without much understanding
dnl
dnl FIND_TIFF[ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]]
dnl ------------------------------------------------
dnl
dnl Find TIFF libraries and headers
dnl
dnl Put compile stuff in TIFF_INCLUDES
dnl Put link stuff in TIFF_LIBS
dnl Define HAVE_TIFF if found
dnl
AC_DEFUN([FIND_TIFF], [
AC_REQUIRE([AC_PATH_XTRA])

TIFF_INCLUDES=""
TIFF_LIBS=""

AC_ARG_WITH(tiff, 
  AS_HELP_STRING([--without-tiff], [build without libtiff (default: test)]))
# Treat --without-tiff like --without-tiff-includes --without-tiff-libraries.
if test "$with_tiff" = "no"; then
  TIFF_INCLUDES=no
  TIFF_LIBS=no
fi

AC_ARG_WITH(tiff-includes,
  AS_HELP_STRING([--with-tiff-includes=DIR], [libtiff includes are in DIR]),
  TIFF_INCLUDES="-I$withval")
AC_ARG_WITH(tiff-libraries,
  AS_HELP_STRING([--with-tiff-libraries=DIR], [libtiff libraries are in DIR]),
  TIFF_LIBS="-L$withval -ltiff")

AC_MSG_CHECKING(for TIFF)

# Look for tiff.h 
if test "$TIFF_INCLUDES" = ""; then
  # Check the standard search path
  AC_TRY_COMPILE([#include <tiff.h>],[int a;],[
    TIFF_INCLUDES=""
  ], [
    # tiff.h is not in the standard search path, try
    # $prefix
    tiff_save_INCLUDES="$INCLUDES"

    INCLUDES="-I${prefix}/include $INCLUDES"

    AC_TRY_COMPILE([#include <tiff.h>],[int a;],[
      TIFF_INCLUDES="-I${prefix}/include"
    ], [
      TIFF_INCLUDES="no"
    ])

    INCLUDES=$tiff_save_INCLUDES
  ])
fi

# Now for the libraries
if test "$TIFF_LIBS" = ""; then
  tiff_save_LIBS="$LIBS"
  tiff_save_INCLUDES="$INCLUDES"

  LIBS="-ltiff -lm $LIBS"
  INCLUDES="$TIFF_INCLUDES $INCLUDES"

  # Try the standard search path first
  AC_TRY_LINK([#include <tiff.h>],[TIFFGetVersion()], [
    TIFF_LIBS="-ltiff"
  ], [
    # libtiff is not in the standard search path, try $prefix

    LIBS="-L${prefix}/lib $LIBS"

    AC_TRY_LINK([#include <tiff.h>],[TIFFGetVersion()], [
      TIFF_LIBS="-L${prefix}/lib -ltiff"
    ], [
      TIFF_LIBS=no
    ])
  ])

  LIBS="$tiff_save_LIBS"
  INCLUDES="$tiff_save_INCLUDES"
fi

AC_SUBST(TIFF_LIBS)
AC_SUBST(TIFF_INCLUDES)

# Print a helpful message
tiff_libraries_result="$TIFF_LIBS"
tiff_includes_result="$TIFF_INCLUDES"

if test x"$tiff_libraries_result" = x""; then
  tiff_libraries_result="in default path"
fi
if test x"$tiff_includes_result" = x""; then
  tiff_includes_result="in default path"
fi

if test "$tiff_libraries_result" = "no"; then
  tiff_libraries_result="(none)"
fi
if test "$tiff_includes_result" = "no"; then
  tiff_includes_result="(none)"
fi

AC_MSG_RESULT([libraries $tiff_libraries_result, headers $tiff_includes_result])

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test "$TIFF_INCLUDES" != "no" && test "$TIFF_LIBS" != "no"; then
  AC_DEFINE(HAVE_TIFF,1,[Define if you have tiff libraries and header files.])
  $1
else
  TIFF_INCLUDES=""
  TIFF_LIBS=""
  $2
fi

])dnl

dnl From FIND_MOTIF and ACX_PTHREAD, without much understanding
dnl
dnl FIND_JPEG[ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]]
dnl ------------------------------------------------
dnl
dnl Find JPEG libraries and headers
dnl
dnl Put compile stuff in JPEG_INCLUDES
dnl Put link stuff in JPEG_LIBS
dnl Define HAVE_JPEG if found
dnl
AC_DEFUN([FIND_JPEG], [
AC_REQUIRE([AC_PATH_XTRA])

JPEG_INCLUDES=""
JPEG_LIBS=""

AC_ARG_WITH(jpeg, 
  AS_HELP_STRING([--without-jpeg], [build without libjpeg (default: test)]))
# Treat --without-jpeg like --without-jpeg-includes --without-jpeg-libraries.
if test "$with_jpeg" = "no"; then
  JPEG_INCLUDES=no
  JPEG_LIBS=no
fi

AC_ARG_WITH(jpeg-includes,
  AS_HELP_STRING([--with-jpeg-includes=DIR], [libjpeg includes are in DIR]),
  JPEG_INCLUDES="-I$withval")
AC_ARG_WITH(jpeg-libraries,
  AS_HELP_STRING([--with-jpeg-libraries=DIR], [libjpeg libraries are in DIR]),
  JPEG_LIBS="-L$withval -ljpeg")

AC_MSG_CHECKING(for JPEG)

# Look for jpeglib.h 
if test "$JPEG_INCLUDES" = ""; then
  # Check the standard search path
  AC_TRY_COMPILE([#include <stdio.h>
    #include <jpeglib.h>],[int a;],[
    JPEG_INCLUDES=""
  ], [
    # jpeglib.h is not in the standard search path, try
    # $prefix
    jpeg_save_INCLUDES="$INCLUDES"

    INCLUDES="-I${prefix}/include $INCLUDES"

    AC_TRY_COMPILE([#include <stdio.h>
      #include <jpeglib.h>],[int a;],[
      JPEG_INCLUDES="-I${prefix}/include"
    ], [
      JPEG_INCLUDES="no"
    ])

    INCLUDES=$jpeg_save_INCLUDES
  ])
fi

# Now for the libraries
if test "$JPEG_LIBS" = ""; then
  jpeg_save_LIBS="$LIBS"
  jpeg_save_INCLUDES="$INCLUDES"

  LIBS="-ljpeg $LIBS"
  INCLUDES="$JPEG_INCLUDES $INCLUDES"

  # Try the standard search path first
  AC_TRY_LINK([#include <stdio.h>
    #include <jpeglib.h>
  ],[jpeg_abort((void*)0)], [
    JPEG_LIBS="-ljpeg"
  ], [
    # libjpeg is not in the standard search path, try $prefix

    LIBS="-L${prefix}/lib $LIBS"

    AC_TRY_LINK([#include <stdio.h>
      #include <jpeg.h>
    ],[jpeg_abort((void*)0)], [
      JPEG_LIBS="-L${prefix}/lib -ljpeg"
    ], [
      JPEG_LIBS=no
    ])
  ])

  LIBS="$jpeg_save_LIBS"
  INCLUDES="$jpeg_save_INCLUDES"
fi

AC_SUBST(JPEG_LIBS)
AC_SUBST(JPEG_INCLUDES)

# Print a helpful message
jpeg_libraries_result="$JPEG_LIBS"
jpeg_includes_result="$JPEG_INCLUDES"

if test x"$jpeg_libraries_result" = x""; then
  jpeg_libraries_result="in default path"
fi
if test x"$jpeg_includes_result" = x""; then
  jpeg_includes_result="in default path"
fi

if test "$jpeg_libraries_result" = "no"; then
  jpeg_libraries_result="(none)"
fi
if test "$jpeg_includes_result" = "no"; then
  jpeg_includes_result="(none)"
fi

AC_MSG_RESULT([libraries $jpeg_libraries_result, headers $jpeg_includes_result])

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test "$JPEG_INCLUDES" != "no" && test "$JPEG_LIBS" != "no"; then
  AC_DEFINE(HAVE_JPEG,1,[Define if you have jpeg libraries and header files.])
  $1
else
  JPEG_INCLUDES=""
  JPEG_LIBS=""
  $2
fi

])dnl

dnl From FIND_MOTIF and ACX_PTHREAD, without much understanding
dnl
dnl FIND_PNG[ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]]
dnl ------------------------------------------------
dnl
dnl Find PNG libraries and headers
dnl
dnl Put compile stuff in PNG_INCLUDES
dnl Put link stuff in PNG_LIBS
dnl Define HAVE_PNG if found.
dnl
AC_DEFUN([FIND_PNG], [
AC_REQUIRE([AC_PATH_XTRA])

PNG_INCLUDES=""
PNG_LIBS=""

AC_ARG_WITH(png, 
  AS_HELP_STRING([--without-png], [build without libpng (default: test)]))
# Treat --without-png like --without-png-includes --without-png-libraries.
if test "$with_png" = "no"; then
  PNG_INCLUDES=no
  PNG_LIBS=no
fi

AC_ARG_WITH(png-includes,
  AS_HELP_STRING([--with-png-includes=DIR], [libpng includes are in DIR]),
  PNG_INCLUDES="-I$withval")
AC_ARG_WITH(png-libraries,
  AS_HELP_STRING([--with-png-libraries=DIR], [libpng libraries are in DIR]),
  PNG_LIBS="-L$withval -lpng")

AC_MSG_CHECKING(for libpng)

# Look for png.h 
if test "$PNG_INCLUDES" = ""; then
  # Check the standard search path
  AC_TRY_COMPILE([#include <png.h>],[int a;],[
    PNG_INCLUDES=""
  ], [
    # png.h is not in the standard search path, try
    # $prefix
    png_save_INCLUDES="$INCLUDES"

    INCLUDES="-I${prefix}/include $INCLUDES"

    AC_TRY_COMPILE([#include <png.h>],[int a;],[
      PNG_INCLUDES="-I${prefix}/include"
    ], [
      PNG_INCLUDES="no"
    ])

    INCLUDES=$png_save_INCLUDES
  ])
fi

# Now for the libraries
if test "$PNG_LIBS" = ""; then
  png_save_LIBS="$LIBS"
  png_save_INCLUDES="$INCLUDES"

  LIBS="-lpng $LIBS"
  INCLUDES="$PNG_INCLUDES $INCLUDES"

  # Try the standard search path first
  AC_TRY_LINK([#include <png.h>],[png_access_version_number()], [
    PNG_LIBS="-lpng"
  ], [
    # libpng is not in the standard search path, try $prefix

    LIBS="-L${prefix}/lib $LIBS"

    AC_TRY_LINK([#include <png.h>],[png_access_version_number()], [
      PNG_LIBS="-L${prefix}/lib -lpng"
    ], [
      PNG_LIBS=no
    ])
  ])

  LIBS="$png_save_LIBS"
  INCLUDES="$png_save_INCLUDES"
fi

AC_SUBST(PNG_LIBS)
AC_SUBST(PNG_INCLUDES)

# Print a helpful message
png_libraries_result="$PNG_LIBS"
png_includes_result="$PNG_INCLUDES"

if test x"$png_libraries_result" = x""; then
  png_libraries_result="in default path"
fi
if test x"$png_includes_result" = x""; then
  png_includes_result="in default path"
fi

if test "$png_libraries_result" = "no"; then
  png_libraries_result="(none)"
fi
if test "$png_includes_result" = "no"; then
  png_includes_result="(none)"
fi

AC_MSG_RESULT([libraries $png_libraries_result, headers $png_includes_result])

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test "$PNG_INCLUDES" != "no" && test "$PNG_LIBS" != "no"; then
  AC_DEFINE(HAVE_PNG,1,[Define if you have png libraries and header files.])
  $1
else
  PNG_INCLUDES=""
  PNG_LIBS=""
  $2
fi

])dnl

dnl a macro to check for ability to create python extensions
dnl  AM_CHECK_PYTHON_HEADERS([ACTION-IF-POSSIBLE], [ACTION-IF-NOT-POSSIBLE])
dnl function also defines PYTHON_INCLUDES
AC_DEFUN([AM_CHECK_PYTHON_HEADERS],
[AC_REQUIRE([AM_PATH_PYTHON])
AC_MSG_CHECKING(for headers required to compile python extensions)
dnl deduce PYTHON_INCLUDES
py_prefix=`$PYTHON -c "import sys; print sys.prefix"`
py_exec_prefix=`$PYTHON -c "import sys; print sys.exec_prefix"`
PYTHON_INCLUDES="-I${py_prefix}/include/python${PYTHON_VERSION}"
if test "$py_prefix" != "$py_exec_prefix"; then
  PYTHON_INCLUDES="$PYTHON_INCLUDES -I${py_exec_prefix}/include/python${PYTHON_VERSION}"
fi
AC_SUBST(PYTHON_INCLUDES)
dnl check if the headers exist:
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $PYTHON_INCLUDES"
AC_TRY_CPP([#include <Python.h>],dnl
[AC_MSG_RESULT(found)
$1],dnl
[AC_MSG_RESULT(not found)
$2])
CPPFLAGS="$save_CPPFLAGS"
])

