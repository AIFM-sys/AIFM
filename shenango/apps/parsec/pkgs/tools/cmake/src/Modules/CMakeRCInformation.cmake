
# This file sets the basic flags for the Fortran language in CMake.
# It also loads the available platform file for the system-compiler
# if it exists.

GET_FILENAME_COMPONENT(CMAKE_BASE_NAME ${CMAKE_RC_COMPILER} NAME_WE)
SET(CMAKE_SYSTEM_AND_RC_COMPILER_INFO_FILE
  ${CMAKE_ROOT}/Modules/Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_BASE_NAME}.cmake)
INCLUDE(Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_BASE_NAME} OPTIONAL)


SET (CMAKE_RC_FLAGS "$ENV{RCFLAGS} ${CMAKE_RC_FLAGS_INIT}" CACHE STRING
     "Flags for Fortran compiler.")

# These are the only types of flags that should be passed to the rc
# command, if COMPILE_FLAGS is used on a target this will be used
# to filter out any other flags
SET(CMAKE_RC_FLAG_REGEX "^[-/](D|I)")

# now define the following rule variables
# CMAKE_RC_COMPILE_OBJECT
SET(CMAKE_INCLUDE_FLAG_RC "-I")
# compile a Resource file into an object file
IF(NOT CMAKE_RC_COMPILE_OBJECT)
  SET(CMAKE_RC_COMPILE_OBJECT
    "<CMAKE_RC_COMPILER> <FLAGS>  /fo<OBJECT> <SOURCE>")
ENDIF(NOT CMAKE_RC_COMPILE_OBJECT)

MARK_AS_ADVANCED(
CMAKE_RC_FLAGS
)
# set this variable so we can avoid loading this more than once.
SET(CMAKE_RC_INFORMATION_LOADED 1)
