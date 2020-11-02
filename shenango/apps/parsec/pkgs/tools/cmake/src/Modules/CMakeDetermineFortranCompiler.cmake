
# determine the compiler to use for Fortran programs
# NOTE, a generator may set CMAKE_Fortran_COMPILER before
# loading this file to force a compiler.
# use environment variable FC first if defined by user, next use 
# the cmake variable CMAKE_GENERATOR_FC which can be defined by a generator
# as a default compiler

IF(NOT CMAKE_Fortran_COMPILER)
  # prefer the environment variable CC
  IF($ENV{FC} MATCHES ".+")
    GET_FILENAME_COMPONENT(CMAKE_Fortran_COMPILER_INIT $ENV{FC} PROGRAM PROGRAM_ARGS CMAKE_Fortran_FLAGS_ENV_INIT)
    IF(CMAKE_Fortran_FLAGS_ENV_INIT)
      SET(CMAKE_Fortran_COMPILER_ARG1 "${CMAKE_Fortran_FLAGS_ENV_INIT}" CACHE STRING "First argument to Fortran compiler")
    ENDIF(CMAKE_Fortran_FLAGS_ENV_INIT)
    IF(EXISTS ${CMAKE_Fortran_COMPILER_INIT})
    ELSE(EXISTS ${CMAKE_Fortran_COMPILER_INIT})
      MESSAGE(FATAL_ERROR "Could not find compiler set in environment variable FC:\n$ENV{FC}.") 
    ENDIF(EXISTS ${CMAKE_Fortran_COMPILER_INIT})
  ENDIF($ENV{FC} MATCHES ".+")
  
  # next try prefer the compiler specified by the generator
  IF(CMAKE_GENERATOR_FC) 
    IF(NOT CMAKE_Fortran_COMPILER_INIT)
      SET(CMAKE_Fortran_COMPILER_INIT ${CMAKE_GENERATOR_FC})
    ENDIF(NOT CMAKE_Fortran_COMPILER_INIT)
  ENDIF(CMAKE_GENERATOR_FC)

  # finally list compilers to try
  IF(CMAKE_Fortran_COMPILER_INIT)
    SET(CMAKE_Fortran_COMPILER_LIST ${CMAKE_Fortran_COMPILER_INIT})
  ELSE(CMAKE_Fortran_COMPILER_INIT)
    # Known compilers:
    #  f77/f90/f95: generic compiler names
    #  g77: GNU Fortran 77 compiler
    #  gfortran: putative GNU Fortran 95+ compiler (in progress)
    #  fort77: native F77 compiler under HP-UX (and some older Crays)
    #  frt: Fujitsu F77 compiler
    #  pgf77/pgf90/pgf95: Portland Group F77/F90/F95 compilers
    #  xlf/xlf90/xlf95: IBM (AIX) F77/F90/F95 compilers
    #  lf95: Lahey-Fujitsu F95 compiler
    #  fl32: Microsoft Fortran 77 "PowerStation" compiler
    #  af77: Apogee F77 compiler for Intergraph hardware running CLIX
    #  epcf90: "Edinburgh Portable Compiler" F90
    #  fort: Compaq (now HP) Fortran 90/95 compiler for Tru64 and Linux/Alpha
    #  ifc: Intel Fortran 95 compiler for Linux/x86
    #  efc: Intel Fortran 95 compiler for IA64
    #
    #  The order is 95 or newer compilers first, then 90, 
    #  then 77 or older compilers, gnu is always last in the group,
    #  so if you paid for a compiler it is picked by default.
    # NOTE for testing purposes this list is DUPLICATED in
    # CMake/Source/CMakeLists.txt, IF YOU CHANGE THIS LIST,
    # PLEASE UPDATE THAT FILE AS WELL!
    SET(CMAKE_Fortran_COMPILER_LIST
      ifort ifc efc f95 pgf95 lf95 xlf95 fort gfortran g95 f90
      pgf90 xlf90 epcf90 fort77 frt pgf77 xlf fl32 af77 g77 f77
      )
  ENDIF(CMAKE_Fortran_COMPILER_INIT)

  # Find the compiler.
  FIND_PROGRAM(CMAKE_Fortran_COMPILER NAMES ${CMAKE_Fortran_COMPILER_LIST} DOC "Fortran compiler")
  IF(CMAKE_Fortran_COMPILER_INIT AND NOT CMAKE_Fortran_COMPILER)
    SET(CMAKE_Fortran_COMPILER "${CMAKE_Fortran_COMPILER_INIT}" CACHE FILEPATH "Fortran compiler" FORCE)
  ENDIF(CMAKE_Fortran_COMPILER_INIT AND NOT CMAKE_Fortran_COMPILER)
ENDIF(NOT CMAKE_Fortran_COMPILER)

MARK_AS_ADVANCED(CMAKE_Fortran_COMPILER)  

# Build a small source file to identify the compiler.
IF(${CMAKE_GENERATOR} MATCHES "Visual Studio")
  SET(CMAKE_Fortran_COMPILER_ID_RUN 1)
  SET(CMAKE_Fortran_PLATFORM_ID "Windows")

  # TODO: Set the compiler id.  It is probably MSVC but
  # the user may be using an integrated Intel compiler.
  # SET(CMAKE_Fortran_COMPILER_ID "MSVC")
ENDIF(${CMAKE_GENERATOR} MATCHES "Visual Studio")

IF(NOT CMAKE_Fortran_COMPILER_ID_RUN)
  SET(CMAKE_Fortran_COMPILER_ID_RUN 1)

  # Each entry in this list is a set of extra flags to try
  # adding to the compile line to see if it helps produce
  # a valid identification executable.
  SET(CMAKE_Fortran_COMPILER_ID_TEST_FLAGS
    # Try compiling to an object file only.
    "-c"

    # Intel on windows does not preprocess by default.
    "-fpp"
    )

  # Try to identify the compiler.
  SET(CMAKE_Fortran_COMPILER_ID)
  INCLUDE(${CMAKE_ROOT}/Modules/CMakeDetermineCompilerId.cmake)
  CMAKE_DETERMINE_COMPILER_ID(Fortran FFLAGS CMakeFortranCompilerId.F90)

  # Fall back to old is-GNU test.
  IF(NOT CMAKE_Fortran_COMPILER_ID)
    EXEC_PROGRAM(${CMAKE_Fortran_COMPILER}
      ARGS ${CMAKE_Fortran_COMPILER_ID_FLAGS_LIST} -E "\"${CMAKE_ROOT}/Modules/CMakeTestGNU.c\""
      OUTPUT_VARIABLE CMAKE_COMPILER_OUTPUT RETURN_VALUE CMAKE_COMPILER_RETURN)
    IF(NOT CMAKE_COMPILER_RETURN)
      IF("${CMAKE_COMPILER_OUTPUT}" MATCHES ".*THIS_IS_GNU.*" )
        SET(CMAKE_Fortran_COMPILER_ID "GNU")
        FILE(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
          "Determining if the Fortran compiler is GNU succeeded with "
          "the following output:\n${CMAKE_COMPILER_OUTPUT}\n\n")
      ELSE("${CMAKE_COMPILER_OUTPUT}" MATCHES ".*THIS_IS_GNU.*" )
        FILE(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
          "Determining if the Fortran compiler is GNU failed with "
          "the following output:\n${CMAKE_COMPILER_OUTPUT}\n\n")
      ENDIF("${CMAKE_COMPILER_OUTPUT}" MATCHES ".*THIS_IS_GNU.*" )
      IF(NOT CMAKE_Fortran_PLATFORM_ID)
        IF("${CMAKE_COMPILER_OUTPUT}" MATCHES ".*THIS_IS_MINGW.*" )
          SET(CMAKE_Fortran_PLATFORM_ID "MinGW")
        ENDIF("${CMAKE_COMPILER_OUTPUT}" MATCHES ".*THIS_IS_MINGW.*" )
        IF("${CMAKE_COMPILER_OUTPUT}" MATCHES ".*THIS_IS_CYGWIN.*" )
          SET(CMAKE_Fortran_PLATFORM_ID "Cygwin")
        ENDIF("${CMAKE_COMPILER_OUTPUT}" MATCHES ".*THIS_IS_CYGWIN.*" )
      ENDIF(NOT CMAKE_Fortran_PLATFORM_ID)
    ENDIF(NOT CMAKE_COMPILER_RETURN)
  ENDIF(NOT CMAKE_Fortran_COMPILER_ID)

  # Set old compiler and platform id variables.
  IF("${CMAKE_Fortran_COMPILER_ID}" MATCHES "GNU")
    SET(CMAKE_COMPILER_IS_GNUG77 1)
  ENDIF("${CMAKE_Fortran_COMPILER_ID}" MATCHES "GNU")
  IF("${CMAKE_Fortran_PLATFORM_ID}" MATCHES "MinGW")
    SET(CMAKE_COMPILER_IS_MINGW 1)
  ELSEIF("${CMAKE_Fortran_PLATFORM_ID}" MATCHES "Cygwin")
    SET(CMAKE_COMPILER_IS_CYGWIN 1)
  ENDIF("${CMAKE_Fortran_PLATFORM_ID}" MATCHES "MinGW")
ENDIF(NOT CMAKE_Fortran_COMPILER_ID_RUN)

INCLUDE(CMakeFindBinUtils)

# configure variables set in this file for fast reload later on
CONFIGURE_FILE(${CMAKE_ROOT}/Modules/CMakeFortranCompiler.cmake.in
  ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeFortranCompiler.cmake
  @ONLY IMMEDIATE # IMMEDIATE must be here for compatibility mode <= 2.0
  )
SET(CMAKE_Fortran_COMPILER_ENV_VAR "FC")
