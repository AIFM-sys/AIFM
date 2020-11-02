SET(APPLE 1)

# Darwin versions:
#   6.x == Mac OSX 10.2
#   7.x == Mac OSX 10.3
#   8.x == Mac OSX 10.4
#   9.x == Mac OSX 10.5
STRING(REGEX REPLACE "^([0-9]+)\\.([0-9]+).*$" "\\1" DARWIN_MAJOR_VERSION "${CMAKE_SYSTEM_VERSION}")
STRING(REGEX REPLACE "^([0-9]+)\\.([0-9]+).*$" "\\2" DARWIN_MINOR_VERSION "${CMAKE_SYSTEM_VERSION}")

# Do not use the "-Wl,-search_paths_first" flag with the OSX 10.2 compiler.
# Done this way because it is too early to do a TRY_COMPILE.
IF(NOT DEFINED HAVE_FLAG_SEARCH_PATHS_FIRST)
  SET(HAVE_FLAG_SEARCH_PATHS_FIRST 0)
  IF("${DARWIN_MAJOR_VERSION}" GREATER 6)
    SET(HAVE_FLAG_SEARCH_PATHS_FIRST 1)
  ENDIF("${DARWIN_MAJOR_VERSION}" GREATER 6)
ENDIF(NOT DEFINED HAVE_FLAG_SEARCH_PATHS_FIRST)
# More desirable, but does not work:
  #INCLUDE(CheckCXXCompilerFlag)
  #CHECK_CXX_COMPILER_FLAG("-Wl,-search_paths_first" HAVE_FLAG_SEARCH_PATHS_FIRST)

SET(CMAKE_SHARED_LIBRARY_PREFIX "lib")
SET(CMAKE_SHARED_LIBRARY_SUFFIX ".dylib")
SET(CMAKE_SHARED_MODULE_PREFIX "lib")
SET(CMAKE_SHARED_MODULE_SUFFIX ".so")
SET(CMAKE_MODULE_EXISTS 1)
SET(CMAKE_DL_LIBS "")

SET(CMAKE_C_LINK_FLAGS "-headerpad_max_install_names")
SET(CMAKE_CXX_LINK_FLAGS "-headerpad_max_install_names")

IF(HAVE_FLAG_SEARCH_PATHS_FIRST)
  SET(CMAKE_C_LINK_FLAGS "-Wl,-search_paths_first ${CMAKE_C_LINK_FLAGS}")
  SET(CMAKE_CXX_LINK_FLAGS "-Wl,-search_paths_first ${CMAKE_CXX_LINK_FLAGS}")
ENDIF(HAVE_FLAG_SEARCH_PATHS_FIRST)

SET(CMAKE_PLATFORM_HAS_INSTALLNAME 1)
SET(CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS "-dynamiclib -headerpad_max_install_names")
SET(CMAKE_SHARED_MODULE_CREATE_C_FLAGS "-bundle -headerpad_max_install_names")
SET(CMAKE_SHARED_MODULE_LOADER_C_FLAG "-Wl,-bundle_loader,")
SET(CMAKE_SHARED_MODULE_LOADER_CXX_FLAG "-Wl,-bundle_loader,")
SET(CMAKE_FIND_LIBRARY_SUFFIXES ".dylib" ".so" ".a")

# hack: if a new cmake (which uses CMAKE_INSTALL_NAME_TOOL) runs on an old build tree
# (where install_name_tool was hardcoded) and where CMAKE_INSTALL_NAME_TOOL isn't in the cache
# and still cmake didn't fail in CMakeFindBinUtils.cmake (because it isn't rerun)
# hardcode CMAKE_INSTALL_NAME_TOOL here to install_name_tool, so it behaves as it did before, Alex
IF(NOT DEFINED CMAKE_INSTALL_NAME_TOOL)
  SET(CMAKE_INSTALL_NAME_TOOL install_name_tool)
ENDIF(NOT DEFINED CMAKE_INSTALL_NAME_TOOL)
# find installed SDKs
FILE(GLOB _CMAKE_OSX_SDKS "/Developer/SDKs/*")
# setup for universal binaries if sysroot exists
IF(_CMAKE_OSX_SDKS) 
  # find the most recent sdk for the default
  LIST(SORT _CMAKE_OSX_SDKS)
  LIST(REVERSE _CMAKE_OSX_SDKS)
  LIST(GET _CMAKE_OSX_SDKS 0 _CMAKE_OSX_SDKS)
  SET(CMAKE_OSX_SYSROOT_DEFAULT "${_CMAKE_OSX_SDKS}")
  # use the environment variable CMAKE_OSX_SYSROOT if it is set
  IF(NOT "$ENV{CMAKE_OSX_SYSROOT}" STREQUAL "") 
    SET(_CMAKE_OSX_SDKS "$ENV{CMAKE_OSX_SYSROOT}")
  ENDIF(NOT "$ENV{CMAKE_OSX_SYSROOT}" STREQUAL "") 
  SET(CMAKE_OSX_SYSROOT ${_CMAKE_OSX_SDKS} CACHE STRING
    "isysroot used for universal binary support")
  # set _CMAKE_OSX_MACHINE to umame -m
  EXEC_PROGRAM(uname ARGS -m OUTPUT_VARIABLE _CMAKE_OSX_MACHINE)
  # check for Power PC and change to ppc
  IF("${_CMAKE_OSX_MACHINE}" MATCHES "Power")
    SET(_CMAKE_OSX_MACHINE ppc)
  ENDIF("${_CMAKE_OSX_MACHINE}" MATCHES "Power")
  # set the default based on uname and not the environment variable
  # as that is what is used to change it!
  SET(CMAKE_OSX_ARCHITECTURES_DEFAULT ${_CMAKE_OSX_MACHINE})
  # check for environment variable CMAKE_OSX_ARCHITECTURES
  # if it is set.
  IF(NOT "$ENV{CMAKE_OSX_ARCHITECTURES}" STREQUAL "")
    SET(_CMAKE_OSX_MACHINE "$ENV{CMAKE_OSX_ARCHITECTURES}")
  ENDIF(NOT "$ENV{CMAKE_OSX_ARCHITECTURES}" STREQUAL "")
  # now put _CMAKE_OSX_MACHINE into the cache
  SET(CMAKE_OSX_ARCHITECTURES ${_CMAKE_OSX_MACHINE}
    CACHE STRING "Build architectures for OSX")
ENDIF(_CMAKE_OSX_SDKS)


IF("${CMAKE_BACKWARDS_COMPATIBILITY}" MATCHES "^1\\.[0-6]$")
  SET(CMAKE_SHARED_MODULE_CREATE_C_FLAGS
    "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} -flat_namespace -undefined suppress")
ENDIF("${CMAKE_BACKWARDS_COMPATIBILITY}" MATCHES "^1\\.[0-6]$")

IF(NOT XCODE)
  # Enable shared library versioning.  This flag is not actually referenced
  # but the fact that the setting exists will cause the generators to support
  # soname computation.
  SET(CMAKE_SHARED_LIBRARY_SONAME_C_FLAG "-install_name")
  SET(CMAKE_SHARED_LIBRARY_SONAME_CXX_FLAG "-install_name")
  SET(CMAKE_SHARED_LIBRARY_SONAME_Fortran_FLAG "-install_name")
ENDIF(NOT XCODE)

# Xcode does not support -isystem yet.
IF(XCODE)
  SET(CMAKE_INCLUDE_SYSTEM_FLAG_C)
  SET(CMAKE_INCLUDE_SYSTEM_FLAG_CXX)
ENDIF(XCODE)

# Need to list dependent shared libraries on link line.  When building
# with -isysroot (for universal binaries), the linker always looks for
# dependent libraries under the sysroot.  Listing them on the link
# line works around the problem.
SET(CMAKE_LINK_DEPENDENT_LIBRARY_FILES 1)

SET(CMAKE_C_CREATE_SHARED_LIBRARY_FORBIDDEN_FLAGS -w)
SET(CMAKE_CXX_CREATE_SHARED_LIBRARY_FORBIDDEN_FLAGS -w)
SET(CMAKE_C_CREATE_SHARED_LIBRARY
  "<CMAKE_C_COMPILER> <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS> <LINK_FLAGS> -o <TARGET> -install_name <TARGET_INSTALLNAME_DIR><TARGET_SONAME> <OBJECTS> <LINK_LIBRARIES>")
SET(CMAKE_CXX_CREATE_SHARED_LIBRARY
  "<CMAKE_CXX_COMPILER> <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> <LINK_FLAGS> -o <TARGET> -install_name <TARGET_INSTALLNAME_DIR><TARGET_SONAME> <OBJECTS> <LINK_LIBRARIES>")
SET(CMAKE_Fortran_CREATE_SHARED_LIBRARY
  "<CMAKE_Fortran_COMPILER> <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_Fortran_FLAGS> <LINK_FLAGS> -o <TARGET> -install_name <TARGET_INSTALLNAME_DIR><TARGET_SONAME> <OBJECTS> <LINK_LIBRARIES>")

SET(CMAKE_CXX_CREATE_SHARED_MODULE
      "<CMAKE_CXX_COMPILER> <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS> <LINK_FLAGS> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")

SET(CMAKE_C_CREATE_SHARED_MODULE
      "<CMAKE_C_COMPILER>  <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_MODULE_CREATE_C_FLAGS> <LINK_FLAGS> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")

SET(CMAKE_Fortran_CREATE_SHARED_MODULE
      "<CMAKE_Fortran_COMPILER>  <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_MODULE_CREATE_Fortran_FLAGS> <LINK_FLAGS> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")

SET(CMAKE_C_CREATE_MACOSX_FRAMEWORK
      "<CMAKE_C_COMPILER> <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS> <LINK_FLAGS> -o <TARGET> -install_name <TARGET_INSTALLNAME_DIR><TARGET_SONAME> <OBJECTS> <LINK_LIBRARIES>")
SET(CMAKE_CXX_CREATE_MACOSX_FRAMEWORK
      "<CMAKE_CXX_COMPILER> <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> <LINK_FLAGS> -o <TARGET> -install_name <TARGET_INSTALLNAME_DIR><TARGET_SONAME> <OBJECTS> <LINK_LIBRARIES>")


 
SET(CMAKE_PLATFORM_IMPLICIT_INCLUDE_DIRECTORIES /usr/local/include)
# default to searching for frameworks first
SET(CMAKE_FIND_FRAMEWORK FIRST)
# set up the default search directories for frameworks
SET(CMAKE_SYSTEM_FRAMEWORK_PATH
  ~/Library/Frameworks
  /Library/Frameworks
  /Network/Library/Frameworks
  /System/Library/Frameworks)

# default to searching for application bundles first
SET(CMAKE_FIND_APPBUNDLE FIRST)
# set up the default search directories for application bundles
SET(CMAKE_SYSTEM_APPBUNDLE_PATH
  ~/Applications
  /Applications
  /Developer/Applications)

INCLUDE(Platform/UnixPaths)
LIST(APPEND CMAKE_SYSTEM_PREFIX_PATH /sw)
