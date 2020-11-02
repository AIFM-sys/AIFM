# - Try to find LibXslt
# Once done this will define
#
#  LIBXSLT_FOUND - system has LibXslt
#  LIBXSLT_INCLUDE_DIR - the LibXslt include directory
#  LIBXSLT_LIBRARIES - Link these to LibXslt
#  LIBXSLT_DEFINITIONS - Compiler switches required for using LibXslt

# Copyright (c) 2006, Alexander Neundorf, <neundorf@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


IF (LIBXSLT_INCLUDE_DIR AND LIBXSLT_LIBRARIES)
   # in cache already
   SET(LibXslt_FIND_QUIETLY TRUE)
ENDIF (LIBXSLT_INCLUDE_DIR AND LIBXSLT_LIBRARIES)

IF (NOT WIN32)
   # use pkg-config to get the directories and then use these values
   # in the FIND_PATH() and FIND_LIBRARY() calls
   INCLUDE(UsePkgConfig)
   PKGCONFIG(libxslt _LibXsltIncDir _LibXsltLinkDir _LibXsltLinkFlags _LibXsltCflags)
   SET(LIBXSLT_DEFINITIONS ${_LibXsltCflags})
ENDIF (NOT WIN32)

FIND_PATH(LIBXSLT_INCLUDE_DIR libxslt/xslt.h
    ${_LibXsltIncDir}
  )

FIND_LIBRARY(LIBXSLT_LIBRARIES NAMES xslt libxslt
    PATHS
    ${_LibXsltLinkDir}
  )

INCLUDE(FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE if 
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibXslt DEFAULT_MSG LIBXSLT_LIBRARIES LIBXSLT_INCLUDE_DIR)


MARK_AS_ADVANCED(LIBXSLT_INCLUDE_DIR LIBXSLT_LIBRARIES)

