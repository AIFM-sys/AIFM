# Locate gdal
# This module defines
# GDAL_LIBRARY
# GDAL_FOUND, if false, do not try to link to gdal 
# GDAL_INCLUDE_DIR, where to find the headers
#
# $GDALDIR is an environment variable that would
# correspond to the ./configure --prefix=$GDAL_DIR
# used in building gdal.
#
# Created by Eric Wing. I'm not a gdal user, but OpenSceneGraph uses it 
# for osgTerrain so I whipped this module together for completeness.
# I actually don't know the conventions or where files are typically
# placed in distros.
# Any real gdal users are encouraged to correct this (but please don't
# break the OS X framework stuff when doing so which is what usually seems 
# to happen).

# This makes the presumption that you are include gdal.h like
# #include "gdal.h"

FIND_PATH(GDAL_INCLUDE_DIR gdal.h
  HINTS
  $ENV{GDAL_DIR}
  PATH_SUFFIXES include
  PATHS
  ~/Library/Frameworks/gdal.framework/Headers
  /Library/Frameworks/gdal.framework/Headers
  /usr/local/include/gdal
  /usr/local/include/GDAL
  /usr/local/include
  /usr/include/gdal
  /usr/include/GDAL
  /usr/include
  /sw/include/gdal 
  /sw/include/GDAL 
  /sw/include # Fink
  /opt/local/include/gdal
  /opt/local/include/GDAL
  /opt/local/include # DarwinPorts
  /opt/csw/include/gdal
  /opt/csw/include/GDAL
  /opt/csw/include # Blastwave
  /opt/include/gdal
  /opt/include/GDAL
  /opt/include
)

FIND_LIBRARY(GDAL_LIBRARY 
  NAMES gdal GDAL
  HINTS
  $ENV{GDAL_DIR}
  PATH_SUFFIXES lib64 lib
  PATHS
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local
    /usr
    /sw
    /opt/local
    /opt/csw
    /opt
    /usr/freeware
    [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment;GDAL_ROOT]/lib
)

SET(GDAL_FOUND "NO")
IF(GDAL_LIBRARY AND GDAL_INCLUDE_DIR)
  SET(GDAL_FOUND "YES")
ENDIF(GDAL_LIBRARY AND GDAL_INCLUDE_DIR)



