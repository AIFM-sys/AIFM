# try to load any previously computed information for C on this platform
INCLUDE( ${CMAKE_PLATFORM_ROOT_BIN}/CMakeCPlatform.cmake OPTIONAL)
# try to load any previously computed information for CXX on this platform
INCLUDE( ${CMAKE_PLATFORM_ROOT_BIN}/CMakeCXXPlatform.cmake OPTIONAL)

SET(WIN32 1)

INCLUDE(Platform/cl)

SET(CMAKE_CREATE_WIN32_EXE /subsystem:windows)
SET(CMAKE_CREATE_CONSOLE_EXE /subsystem:console)

IF(CMAKE_GENERATOR MATCHES "Visual Studio 6")
   SET (CMAKE_NO_BUILD_TYPE 1)
ENDIF(CMAKE_GENERATOR MATCHES "Visual Studio 6")
IF(NOT CMAKE_NO_BUILD_TYPE AND CMAKE_GENERATOR MATCHES "Visual Studio")
  SET (CMAKE_NO_BUILD_TYPE 1)
  SET (CMAKE_CONFIGURATION_TYPES "Debug;Release;MinSizeRel;RelWithDebInfo" CACHE STRING 
     "Semicolon separated list of supported configuration types, only supports Debug, Release, MinSizeRel, and RelWithDebInfo, anything else will be ignored.")
  MARK_AS_ADVANCED(CMAKE_CONFIGURATION_TYPES)
ENDIF(NOT CMAKE_NO_BUILD_TYPE AND CMAKE_GENERATOR MATCHES "Visual Studio")
# does the compiler support pdbtype and is it the newer compiler
IF(CMAKE_GENERATOR MATCHES  "Visual Studio 8")
  SET(CMAKE_COMPILER_2005 1)
ENDIF(CMAKE_GENERATOR MATCHES  "Visual Studio 8")
IF(CMAKE_GENERATOR MATCHES  "Visual Studio 9")
  SET(CMAKE_COMPILER_2005 1)
ENDIF(CMAKE_GENERATOR MATCHES  "Visual Studio 9")


# make sure to enable languages after setting configuration types
ENABLE_LANGUAGE(RC)
SET(CMAKE_COMPILE_RESOURCE "rc <FLAGS> /fo<OBJECT> <SOURCE>")

# for nmake we need to compute some information about the compiler 
# that is being used.
# the compiler may be free command line, 6, 7, or 71, and
# each have properties that must be determined.  
# to avoid running these tests with each cmake run, the
# test results are saved in CMakeCPlatform.cmake, a file
# that is automatically copied into try_compile directories
# by the global generator.
SET(MSVC_IDE 1)
IF(CMAKE_GENERATOR MATCHES "Makefiles")
  SET(MSVC_IDE 0)
  IF(NOT CMAKE_VC_COMPILER_TESTS_RUN)
    SET(CMAKE_VC_COMPILER_TESTS 1)
    SET(testNmakeCLVersionFile
      "${CMAKE_ROOT}/Modules/CMakeTestNMakeCLVersion.c")
    STRING(REGEX REPLACE "/" "\\\\" testNmakeCLVersionFile "${testNmakeCLVersionFile}")
    MESSAGE(STATUS "Check for CL compiler version")
    SET(CMAKE_TEST_COMPILER ${CMAKE_C_COMPILER})
    IF (NOT CMAKE_C_COMPILER)
      SET(CMAKE_TEST_COMPILER ${CMAKE_CXX_COMPILER})
    ENDIF(NOT CMAKE_C_COMPILER)
    EXEC_PROGRAM(${CMAKE_TEST_COMPILER} 
      ARGS /nologo -EP \"${testNmakeCLVersionFile}\" 
      OUTPUT_VARIABLE CMAKE_COMPILER_OUTPUT 
      RETURN_VALUE CMAKE_COMPILER_RETURN
      )
    IF(NOT CMAKE_COMPILER_RETURN)
      FILE(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log 
        "Determining the version of compiler passed with the following output:\n"
        "${CMAKE_COMPILER_OUTPUT}\n\n")
      STRING(REGEX REPLACE "\n" " " compilerVersion "${CMAKE_COMPILER_OUTPUT}")
      STRING(REGEX REPLACE ".*VERSION=(.*)" "\\1"
        compilerVersion "${compilerVersion}")
      MESSAGE(STATUS "Check for CL compiler version - ${compilerVersion}")
      SET(MSVC60)
      SET(MSVC70)
      SET(MSVC71)
      SET(MSVC80)
      SET(CMAKE_COMPILER_2005)
      IF("${compilerVersion}" LESS 1300)
        SET(MSVC60 1)
        SET(CMAKE_COMPILER_SUPPORTS_PDBTYPE 1)
      ENDIF("${compilerVersion}" LESS 1300)
      IF("${compilerVersion}" EQUAL 1300)
        SET(MSVC70 1)
        SET(CMAKE_COMPILER_SUPPORTS_PDBTYPE 0)
      ENDIF("${compilerVersion}" EQUAL 1300)
      IF("${compilerVersion}" EQUAL 1310)
        SET(MSVC71 1)
        SET(CMAKE_COMPILER_SUPPORTS_PDBTYPE 0)
      ENDIF("${compilerVersion}" EQUAL 1310)
      IF("${compilerVersion}" EQUAL 1400)
        SET(MSVC80 1)
        SET(CMAKE_COMPILER_2005 1)
      ENDIF("${compilerVersion}" EQUAL 1400)
      IF("${compilerVersion}" GREATER 1400)
        SET(MSVC80 1)
        SET(CMAKE_COMPILER_2005 1)
      ENDIF("${compilerVersion}" GREATER 1400)
      IF("${compilerVersion}" GREATER 1400)
        SET(MSVC80 1)
        SET(CMAKE_COMPILER_2005 1)
      ENDIF("${compilerVersion}" GREATER 1400)
      IF("${compilerVersion}" EQUAL 1500)
        SET(MSVC90 1)
        SET(MSVC80 0)
      ENDIF("${compilerVersion}" EQUAL 1500)
      IF("${compilerVersion}" GREATER 1500)
        SET(MSVC90 1)
        SET(MSVC80 0)
      ENDIF("${compilerVersion}" GREATER 1500)
      SET(MSVC_VERSION "${compilerVersion}")
    ELSE(NOT CMAKE_COMPILER_RETURN)
      MESSAGE(STATUS "Check for CL compiler version - failed")
      FILE(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log 
        "Determining the version of compiler failed with the following output:\n"
        "${CMAKE_COMPILER_OUTPUT}\n\n")
    ENDIF(NOT CMAKE_COMPILER_RETURN)
    # try to figure out if we are running the free command line
    # tools from Microsoft.  These tools do not provide debug libraries,
    # so the link flags used have to be different.
    MAKE_DIRECTORY("${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp2")
    SET(testForFreeVCFile
      "${CMAKE_ROOT}/Modules/CMakeTestForFreeVC.cxx")
    STRING(REGEX REPLACE "/" "\\\\" testForFreeVCFile "${testForFreeVCFile}")
    MESSAGE(STATUS "Check if this is a free VC compiler")
    EXEC_PROGRAM(${CMAKE_TEST_COMPILER} ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp2
      ARGS /nologo /MD /EHsc
      \"${testForFreeVCFile}\"
      OUTPUT_VARIABLE CMAKE_COMPILER_OUTPUT 
      RETURN_VALUE CMAKE_COMPILER_RETURN
      )
    IF(CMAKE_COMPILER_RETURN)
      FILE(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log 
        "Determining if this is a free VC compiler failed with the following output:\n"
        "${CMAKE_COMPILER_OUTPUT}\n\n")
      MESSAGE(STATUS "Check if this is a free VC compiler - yes")
      SET(CMAKE_USING_VC_FREE_TOOLS 1)
    ELSE(CMAKE_COMPILER_RETURN)
      FILE(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log 
        "Determining if this is a free VC compiler passed with the following output:\n"
        "${CMAKE_COMPILER_OUTPUT}\n\n")
      MESSAGE(STATUS "Check if this is a free VC compiler - no")
      SET(CMAKE_USING_VC_FREE_TOOLS 0)
    ENDIF(CMAKE_COMPILER_RETURN)
    MAKE_DIRECTORY("${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp3")
    MESSAGE(STATUS "Check CL platform")
    EXEC_PROGRAM(${CMAKE_TEST_COMPILER} ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp3
      ARGS /nologo
      \"${testForFreeVCFile}\"
      /link /machine:i386
      OUTPUT_VARIABLE CMAKE_COMPILER_OUTPUT 
      RETURN_VALUE CMAKE_COMPILER_RETURN
      )
    # if there was an error assume it is a 64bit system
    IF(CMAKE_COMPILER_RETURN)
      FILE(APPEND 
        ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log 
        "Determining if this is a 64 bit system passed:\n"
        "${CMAKE_COMPILER_OUTPUT}\n\n")
      MESSAGE(STATUS "Check CL platform - 64 bit")
      SET(CMAKE_CL_64 1)
    ELSE(CMAKE_COMPILER_RETURN)
      FILE(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log 
        "Determining if this is a 32 bit system passed:\n"
        "${CMAKE_COMPILER_OUTPUT}\n\n")
      MESSAGE(STATUS "Check CL platform - 32 bit")
      SET(CMAKE_CL_64 0)
    ENDIF(CMAKE_COMPILER_RETURN)
  ENDIF(NOT CMAKE_VC_COMPILER_TESTS_RUN)
ENDIF(CMAKE_GENERATOR MATCHES "Makefiles")

IF(CMAKE_FORCE_WIN64)
  SET(CMAKE_CL_64 1)
ENDIF(CMAKE_FORCE_WIN64)


# default to Debug builds
IF(MSVC_VERSION GREATER 1310)
  # for 2005 make sure the manifest is put in the dll with mt
  SET(CMAKE_CXX_CREATE_SHARED_LIBRARY "<CMAKE_COMMAND> -E vs_link_dll ${CMAKE_CXX_CREATE_SHARED_LIBRARY}")
  SET(CMAKE_CXX_CREATE_SHARED_MODULE "<CMAKE_COMMAND> -E vs_link_dll ${CMAKE_CXX_CREATE_SHARED_MODULE}")
  # create a C shared library
  SET(CMAKE_C_CREATE_SHARED_LIBRARY "${CMAKE_CXX_CREATE_SHARED_LIBRARY}")
  # create a C shared module just copy the shared library rule
  SET(CMAKE_C_CREATE_SHARED_MODULE "${CMAKE_CXX_CREATE_SHARED_MODULE}")
  SET(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_COMMAND> -E vs_link_exe ${CMAKE_CXX_LINK_EXECUTABLE}")
  SET(CMAKE_C_LINK_EXECUTABLE "<CMAKE_COMMAND> -E vs_link_exe ${CMAKE_C_LINK_EXECUTABLE}")

  SET(CMAKE_BUILD_TYPE_INIT Debug)
  SET (CMAKE_CXX_FLAGS_INIT "/DWIN32 /D_WINDOWS /W3 /Zm1000 /EHsc /GR")
  SET (CMAKE_CXX_FLAGS_DEBUG_INIT "/D_DEBUG /MDd /Zi /Ob0 /Od /RTC1")
  SET (CMAKE_CXX_FLAGS_MINSIZEREL_INIT "/MD /O1 /Ob1 /D NDEBUG")
  SET (CMAKE_CXX_FLAGS_RELEASE_INIT "/MD /O2 /Ob2 /D NDEBUG")
  SET (CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "/MD /Zi /O2 /Ob1 /D NDEBUG")
  SET (CMAKE_C_FLAGS_INIT "/DWIN32 /D_WINDOWS /W3 /Zm1000")
  SET (CMAKE_C_FLAGS_DEBUG_INIT "/D_DEBUG /MDd /Zi  /Ob0 /Od /RTC1")
  SET (CMAKE_C_FLAGS_MINSIZEREL_INIT "/MD /O1 /Ob1 /D NDEBUG")
  SET (CMAKE_C_FLAGS_RELEASE_INIT "/MD /O2 /Ob2 /D NDEBUG")
  SET (CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "/MD /Zi /O2 /Ob1 /D NDEBUG")
  SET (CMAKE_C_STANDARD_LIBRARIES_INIT "kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib ")
  SET (CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} /MANIFEST")
ELSE(MSVC_VERSION GREATER 1310)
  IF(CMAKE_USING_VC_FREE_TOOLS)
    MESSAGE(STATUS "Using FREE VC TOOLS, NO DEBUG available")
    SET(CMAKE_BUILD_TYPE_INIT Release)
    SET (CMAKE_CXX_FLAGS_INIT "/DWIN32 /D_WINDOWS /W3 /Zm1000 /GX /GR")
    SET (CMAKE_CXX_FLAGS_DEBUG_INIT "/D_DEBUG /MTd /Zi  /Ob0 /Od /GZ")
    SET (CMAKE_CXX_FLAGS_MINSIZEREL_INIT "/MT /O1 /Ob1 /D NDEBUG")
    SET (CMAKE_CXX_FLAGS_RELEASE_INIT "/MT /O2 /Ob2 /D NDEBUG")
    SET (CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "/MT /Zi /O2 /Ob1 /D NDEBUG")
    SET (CMAKE_C_FLAGS_INIT "/DWIN32 /D_WINDOWS /W3 /Zm1000")
    SET (CMAKE_C_FLAGS_DEBUG_INIT "/D_DEBUG /MTd /Zi  /Ob0 /Od /GZ")
    SET (CMAKE_C_FLAGS_MINSIZEREL_INIT "/MT /O1 /Ob1 /D NDEBUG")
    SET (CMAKE_C_FLAGS_RELEASE_INIT "/MT /O2 /Ob2 /D NDEBUG")
    SET (CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "/MT /Zi /O2 /Ob1 /D NDEBUG")
  ELSE(CMAKE_USING_VC_FREE_TOOLS)
    SET(CMAKE_BUILD_TYPE_INIT Debug)
    SET (CMAKE_CXX_FLAGS_INIT "/DWIN32 /D_WINDOWS /W3 /Zm1000 /GX /GR")
    SET (CMAKE_CXX_FLAGS_DEBUG_INIT "/D_DEBUG /MDd /Zi  /Ob0 /Od /GZ")
    SET (CMAKE_CXX_FLAGS_MINSIZEREL_INIT "/MD /O1 /Ob1 /D NDEBUG")
    SET (CMAKE_CXX_FLAGS_RELEASE_INIT "/MD /O2 /Ob2 /D NDEBUG")
    SET (CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "/MD /Zi /O2 /Ob1 /D NDEBUG")
    SET (CMAKE_C_FLAGS_INIT "/DWIN32 /D_WINDOWS /W3 /Zm1000")
    SET (CMAKE_C_FLAGS_DEBUG_INIT "/D_DEBUG /MDd /Zi /Ob0 /Od /GZ")
    SET (CMAKE_C_FLAGS_MINSIZEREL_INIT "/MD /O1 /Ob1 /D NDEBUG")
    SET (CMAKE_C_FLAGS_RELEASE_INIT "/MD /O2 /Ob2 /D NDEBUG")
    SET (CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "/MD /Zi /O2 /Ob1 /D NDEBUG")
  ENDIF(CMAKE_USING_VC_FREE_TOOLS)
  SET (CMAKE_C_STANDARD_LIBRARIES_INIT "kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib")
ENDIF(MSVC_VERSION GREATER 1310)

SET(CMAKE_CXX_STANDARD_LIBRARIES_INIT "${CMAKE_C_STANDARD_LIBRARIES_INIT}")

# executable linker flags
SET (CMAKE_LINK_DEF_FILE_FLAG "/DEF:")
# set the stack size and the machine type
IF(CMAKE_CL_64)
  SET (CMAKE_EXE_LINKER_FLAGS_INIT
    "${CMAKE_EXE_LINKER_FLAGS_INIT} /STACK:10000000 /machine:x64")
ELSE(CMAKE_CL_64)
  SET (CMAKE_EXE_LINKER_FLAGS_INIT
    "${CMAKE_EXE_LINKER_FLAGS_INIT} /STACK:10000000 /machine:I386")
ENDIF(CMAKE_CL_64)

# add /debug and /INCREMENTAL:YES to DEBUG and RELWITHDEBINFO also add pdbtyp
# on versions that support it
IF (CMAKE_COMPILER_SUPPORTS_PDBTYPE)
  SET (CMAKE_EXE_LINKER_FLAGS_DEBUG_INIT "/debug /pdbtype:sept /INCREMENTAL:YES")
  SET (CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO_INIT "/debug /pdbtype:sept /INCREMENTAL:YES")
ELSE (CMAKE_COMPILER_SUPPORTS_PDBTYPE)
  SET (CMAKE_EXE_LINKER_FLAGS_DEBUG_INIT "/debug /INCREMENTAL:YES")
  SET (CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO_INIT "/debug /INCREMENTAL:YES")
ENDIF (CMAKE_COMPILER_SUPPORTS_PDBTYPE)
# for release and minsize release default to no incremental linking
SET(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL_INIT "/INCREMENTAL:NO")
SET(CMAKE_EXE_LINKER_FLAGS_RELEASE_INIT "/INCREMENTAL:NO")

# copy the EXE_LINKER flags to SHARED and MODULE linker flags
# shared linker flags
SET (CMAKE_SHARED_LINKER_FLAGS_INIT ${CMAKE_EXE_LINKER_FLAGS_INIT})
SET (CMAKE_SHARED_LINKER_FLAGS_DEBUG_INIT ${CMAKE_EXE_LINKER_FLAGS_DEBUG_INIT})
SET (CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO_INIT ${CMAKE_EXE_LINKER_FLAGS_DEBUG_INIT})
SET (CMAKE_SHARED_LINKER_FLAGS_RELEASE_INIT ${CMAKE_EXE_LINKER_FLAGS_RELEASE_INIT})
SET (CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL_INIT ${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL_INIT})
# module linker flags
SET (CMAKE_MODULE_LINKER_FLAGS_INIT ${CMAKE_SHARED_LINKER_FLAGS_INIT})
SET (CMAKE_MODULE_LINKER_FLAGS_DEBUG_INIT ${CMAKE_SHARED_LINKER_FLAGS_DEBUG_INIT})
SET (CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO_INIT ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO_INIT})
SET (CMAKE_MODULE_LINKER_FLAGS_RELEASE_INIT ${CMAKE_EXE_LINKER_FLAGS_RELEASE_INIT})
SET (CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL_INIT ${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL_INIT})


# save computed information for this platform
IF(NOT EXISTS "${CMAKE_PLATFORM_ROOT_BIN}/CMakeCPlatform.cmake")
  CONFIGURE_FILE(${CMAKE_ROOT}/Modules/Platform/Windows-cl.cmake.in 
    ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeCPlatform.cmake IMMEDIATE)
ENDIF(NOT EXISTS "${CMAKE_PLATFORM_ROOT_BIN}/CMakeCPlatform.cmake")

IF(NOT EXISTS "${CMAKE_PLATFORM_ROOT_BIN}/CMakeCXXPlatform.cmake")
  CONFIGURE_FILE(${CMAKE_ROOT}/Modules/Platform/Windows-cl.cmake.in 
               ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeCXXPlatform.cmake IMMEDIATE)
ENDIF(NOT EXISTS "${CMAKE_PLATFORM_ROOT_BIN}/CMakeCXXPlatform.cmake")

