
# This file is included by cmGlobalGenerator::EnableLanguage.
# It is included after the compiler has been determined, so
# we know things like the compiler name and if the compiler is gnu.

# before cmake 2.6 these variables were set in cmMakefile.cxx. This is still
# done to keep scripts and custom language and compiler modules working.
# But they are reset here and set again in the platform files for the target
# platform, so they can be used for testing the target platform instead
# of testing the host platform.
SET(APPLE  )
SET(UNIX   )
SET(CYGWIN )
SET(WIN32  )


# include Generic system information
INCLUDE(CMakeGenericSystem)

# 2. now include SystemName.cmake file to set the system specific information
SET(CMAKE_SYSTEM_INFO_FILE Platform/${CMAKE_SYSTEM_NAME})

INCLUDE(${CMAKE_SYSTEM_INFO_FILE} OPTIONAL RESULT_VARIABLE _INCLUDED_SYSTEM_INFO_FILE)

IF(NOT _INCLUDED_SYSTEM_INFO_FILE)
  MESSAGE("System is unknown to cmake, create:\n${CMAKE_SYSTEM_INFO_FILE}"
          " to use this system, please send your config file to "
          "cmake@www.cmake.org so it can be added to cmake")
  IF(EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
    CONFIGURE_FILE(${CMAKE_BINARY_DIR}/CMakeCache.txt
                   ${CMAKE_BINARY_DIR}/CopyOfCMakeCache.txt COPYONLY)
    MESSAGE("You CMakeCache.txt file was copied to CopyOfCMakeCache.txt. " 
            "Please send that file to cmake@www.cmake.org.")
   ENDIF(EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
ENDIF(NOT _INCLUDED_SYSTEM_INFO_FILE)


# for most systems a module is the same as a shared library
# so unless the variable CMAKE_MODULE_EXISTS is set just
# copy the values from the LIBRARY variables
# this has to be done after the system information has been loaded
IF(NOT CMAKE_MODULE_EXISTS)
  SET(CMAKE_SHARED_MODULE_PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}")
  SET(CMAKE_SHARED_MODULE_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
  SET(CMAKE_SHARED_MODULE_RUNTIME_C_FLAG ${CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG})
  SET(CMAKE_SHARED_MODULE_RUNTIME_C_FLAG_SEP ${CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG_SEP})
ENDIF(NOT CMAKE_MODULE_EXISTS)


SET(CMAKE_SYSTEM_SPECIFIC_INFORMATION_LOADED 1)
