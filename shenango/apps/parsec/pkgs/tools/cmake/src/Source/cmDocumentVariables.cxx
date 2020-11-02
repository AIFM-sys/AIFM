#include "cmDocumentVariables.h"
#include "cmake.h"

void cmDocumentVariables::DefineVariables(cmake* cm)
{ 
  // Subsection: variables defined by cmake, that give
  // information about the project, and cmake
  cm->DefineProperty
    ("CMAKE_AR", cmProperty::VARIABLE,
     "Name of archiving tool for static libraries.",
     "This specifies name of the program that creates archive "
     "or static libraries.",false,
     "Variables that Provide Information");

  cm->DefineProperty
    ("CMAKE_COMMAND", cmProperty::VARIABLE,
     "The full path to the cmake executable.",
     "This is the full path to the CMake executable cmake which is "
     "useful from custom commands that want to use the cmake -E "
     "option for portable system commands. "
     "(e.g. /usr/local/bin/cmake", false, 
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_BINARY_DIR", cmProperty::VARIABLE,
     "The path to the top level of the build tree.",
     "This is the full path to the top level of the current CMake "
     "build tree. For an in-source build, this would be the same "
     "as CMAKE_SOURCE_DIR. ", false, 
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_SOURCE_DIR", cmProperty::VARIABLE,
     "The path to the top level of the source tree.",
     "This is the full path to the top level of the current CMake "
     "source tree. For an in-source build, this would be the same "
     "as CMAKE_BINARY_DIR. ", false, 
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_CURRENT_BINARY_DIR", cmProperty::VARIABLE,
     "The path to the binary directory currently being processed.",
     "This the full path to the build directory that is currently "
     "being processed by cmake.  Each directory added by "
     "add_subdirectory will create a binary directory in the build "
     "tree, and as it is being processed this variable will be set. "
     "For in-source builds this is the current source directory "
     "being processed.", false, 
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_CURRENT_SOURCE_DIR", cmProperty::VARIABLE,
     "The path to the source directory currently being processed.",
     "This the full path to the source directory that is currently "
     "being processed by cmake.  ", false, 
     "Variables that Provide Information");

  cm->DefineProperty
    ("CMAKE_CURRENT_LIST_FILE", cmProperty::VARIABLE,
     "Full path to the listfile currently being processed.",
     "As CMake processes the listfiles in your project this "
     "variable will always be set to the one currently being "
     "processed. See also CMAKE_PARENT_LIST_FILE.",false,
     "Variables that Provide Information");

  cm->DefineProperty
    ("CMAKE_CURRENT_LIST_LINE", cmProperty::VARIABLE,
     "The line number of the current file being processed.",
     "This is the line number of the file currently being"
     " processed by cmake.", false, 
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_BUILD_TOOL", cmProperty::VARIABLE,
     "Tool used for the acutal build process.",
     "This variable is set to the program that will be"
     " needed to build the output of CMake.   If the "
     "generator selected was Visual Studio 6, the "
     "CMAKE_MAKE_PROGRAM will be set to msdev, for "
     "Unix makefiles it will be set to make or gmake, "
     "and for Visual Studio 7 it set to devenv.  For "
     "Nmake Makefiles the value is nmake. This can be "
     "useful for adding special flags and commands based"
     " on the final build environment. ", false, 
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_CROSSCOMPILING", cmProperty::VARIABLE,
     "Is CMake currently cross compiling.",
     "This variable will be set to true by CMake if CMake is cross "
     "compiling. Specifically if the build platform is different "
     "from the target platform.", false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_CACHEFILE_DIR", cmProperty::VARIABLE,
     "The directory with the CMakeCache.txt file.",
     "This is the full path to the directory that has the "
     "CMakeCache.txt file in it.  This is the same as "
     "CMAKE_BINARY_DIR.", false, 
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_CACHE_MAJOR_VERSION", cmProperty::VARIABLE,
     "Major version of CMake used to create the CMakeCache.txt file",
     "This is stores the major version of CMake used to "
     "write a CMake cache file. It is only different when "
     "a different version of CMake is run on a previously "
     "created cache file.", false, 
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_CACHE_MINOR_VERSION", cmProperty::VARIABLE,
     "Minor version of CMake used to create the CMakeCache.txt file",
     "This is stores the minor version of CMake used to "
     "write a CMake cache file. It is only different when "
     "a different version of CMake is run on a previously "
     "created cache file.", false, 
     "Variables that Provide Information");
  
  cm->DefineProperty
    ("CMAKE_CACHE_RELEASE_VERSION", cmProperty::VARIABLE,
     "Release version of CMake used to create the CMakeCache.txt file",
     "This is stores the release version of CMake used to "
     "write a CMake cache file. It is only different when "
     "a different version of CMake is run on a previously "
     "created cache file.", false, 
     "Variables that Provide Information");
  
  cm->DefineProperty
    ("CMAKE_CFG_INTDIR", cmProperty::VARIABLE,
     "Build time configuration directory for project.",
     "This is a variable that is used to provide developers"
     " access to the intermediate directory used by Visual "
     "Studio IDE projects.   For example, if building "
     "Debug all executables and libraries end up in a "
     "Debug directory.   On UNIX systems this variable "
     "is set to \".\".  However, with Visual Studio this "
     "variable is set to $(IntDir).   $(IntDir) is expanded "
     "by the IDE only.  So this variable should only be "
     "used in custom commands that will be run during "
     "the build process.   This variable should not be "
     "used directly in a CMake command.  CMake has no "
     "way of knowing if Debug or Release will be picked "
     "by the IDE for a build type. If a program needs to "
     "know the directory it was built in, it can use "
     "CMAKE_INTDIR. CMAKE_INTDIR is a C/C++ preprocessor "
     "macro that is defined on the command line of the "
     "compiler.   If it has a value, it will be the "
     "intermediate directory used to build the file.   "
     "This way an executable or a library can find files "
     "that are located in the build directory.",false,
     "Variables that Provide Information");
  
  cm->DefineProperty
    ("CMAKE_CTEST_COMMAND", cmProperty::VARIABLE,
     "Full path to ctest command installed with cmake.",
     "This is the full path to the CTest executable ctest "
     "which is useful from custom commands that want "
     " to use the cmake -E option for portable system "
     "commands.",false,
     "Variables that Provide Information");

  cm->DefineProperty
    ("CMAKE_DL_LIBS", cmProperty::VARIABLE,
     "Name of library containing dlopen and dlcose.",
     "The name of the library that has dlopen and "
     "dlclose in it, usually -ldl on most UNIX machines.",false,
     "Variables that Provide Information");

  cm->DefineProperty
    ("CMAKE_EDIT_COMMAND", cmProperty::VARIABLE,
     "Full path to CMakeSetup or ccmake.",
     "This is the full path to the CMake executable "
     "that can graphically edit the cache.  For example,"
     " CMakeSetup, ccmake, or cmake -i.",false,
     "Variables that Provide Information");

  cm->DefineProperty
    ("CMAKE_GENERATOR", cmProperty::VARIABLE,
     "The generator used to build the project.",
     "The name of the generator that is being used to generate the "
     "build files.  (e.g. \"Unix Makefiles\", "
     "\"Visual Studio 6\", etc.)",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_HOME_DIRECTORY", cmProperty::VARIABLE,
     "Path to top of source tree.",
     "This is the path to the top level of the source tree.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_LINK_LIBRARY_SUFFIX", cmProperty::VARIABLE,
     "The suffix for libraries that you link to.",
     "The suffix to use for the end of a library, .lib on Windows.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_EXECUTABLE_SUFFIX", cmProperty::VARIABLE,
     "The suffix for executables on this platform.",
     "The suffix to use for the end of an executable if any, "
     ".exe on Windows.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_MAJOR_VERSION", cmProperty::VARIABLE,
     "The Major version of cmake (i.e. the 2 in 2.X.X)",
     "This specifies the major version of the CMake executable"
     " being run.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_MAKE_PROGRAM", cmProperty::VARIABLE,
     "See CMAKE_BUILD_TOOL.",
     "This variable is around for backwards compatibility, "
     "see CMAKE_BUILD_TOOL.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_MINOR_VERSION", cmProperty::VARIABLE,
     "The Minor version of cmake (i.e. the 4 in X.4.X).",
     "This specifies the minor version of the CMake"
     " executable being run.",false,
     "Variables that Provide Information");

  cm->DefineProperty
    ("CMAKE_PARENT_LIST_FILE", cmProperty::VARIABLE,
     "Full path to the parent listfile of the one currently being processed.",
     "As CMake processes the listfiles in your project this "
     "variable will always be set to the listfile that included "
     "or somehow invoked the one currently being "
     "processed. See also CMAKE_CURRENT_LIST_FILE.",false,
     "Variables that Provide Information");

  cm->DefineProperty
    ("CMAKE_PROJECT_NAME", cmProperty::VARIABLE,
     "The name of the current project.",
     "This specifies name of the current project from"
     " the closest inherited PROJECT command.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_RANLIB", cmProperty::VARIABLE,
     "Name of randomizing tool for static libraries.",
     "This specifies name of the program that randomizes "
     "libraries on UNIX, not used on Windows, but may be present.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_ROOT", cmProperty::VARIABLE,
     "Install directory for running cmake.",
     "This is the install root for the running CMake and"
     " the Modules directory can be found here. This is"
     " commonly used in this format: ${CMAKE_ROOT}/Modules",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_SIZEOF_VOID_P", cmProperty::VARIABLE,
     "Size of a void pointer.",
     "This is set to the size of a pointer on the machine, "
     "and is determined by a try compile. If a 64 bit size "
     "is found, then the library search path is modified to "
     "look for 64 bit libraries first.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_SKIP_RPATH", cmProperty::VARIABLE,
     "If true, do not add run time path information.",
     "If this is set to TRUE, then the rpath information "
     "is not added to compiled executables.  The default"
     "is to add rpath information if the platform supports it."
     "This allows for easy running from the build tree.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_SOURCE_DIR", cmProperty::VARIABLE,
     "Source directory for project.",
     "This is the top level source directory for the project. "
     "It corresponds to the source directory given to "
     "CMakeSetup or ccmake.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_STANDARD_LIBRARIES", cmProperty::VARIABLE,
     "Libraries linked into every executable and shared library.",
     "This is the list of libraries that are linked "
     "into all executables and libraries.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_USING_VC_FREE_TOOLS", cmProperty::VARIABLE,
     "True if free visual studio tools being used.",
     "This is set to true if the compiler is Visual "
     "Studio free tools.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_VERBOSE_MAKEFILE", cmProperty::VARIABLE,
     "Create verbose makefiles if on.",
     "This variable defaults to false. You can set "
     "this variable to true to make CMake produce verbose "
     "makefiles that show each command line as it is used.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("PROJECT_BINARY_DIR", cmProperty::VARIABLE,
     "Full path to build directory for project.",
     "This is the binary directory of the most recent "
     "PROJECT command.",false,"Variables that Provide Information");
  cm->DefineProperty
    ("PROJECT_NAME", cmProperty::VARIABLE,
     "Name of the project given to the project command.",
     "This is the name given to the most "
     "recent PROJECT command. ",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("PROJECT_SOURCE_DIR", cmProperty::VARIABLE,
     "Top level source directory for the current project.",
     "This is the source directory of the most recent "
     "PROJECT command.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("[Project name]_BINARY_DIR", cmProperty::VARIABLE,
     "Top level binary directory for the named project.",
     "A variable is created with the name used in the PROJECT "
     "command, and is the binary directory for the project.  "
     " This can be useful when SUBDIR is used to connect "
     "several projects.",false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("[Project name]_SOURCE_DIR", cmProperty::VARIABLE,
     "Top level source directory for the named project.",
     "A variable is created with the name used in the PROJECT "
     "command, and is the source directory for the project."
     "   This can be useful when add_subdirectory "
     "is used to connect several projects.",false,
     "Variables that Provide Information");

  cm->DefineProperty
    ("CMAKE_IMPORT_LIBRARY_PREFIX", cmProperty::VARIABLE,
     "The prefix for import libraries that you link to.",
     "The prefix to use for the name of an import library if used "
     "on this platform.",
     false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_IMPORT_LIBRARY_SUFFIX", cmProperty::VARIABLE,
     "The suffix for import  libraries that you link to.",
     "The suffix to use for the end of an import library if used "
     "onthis platform.",
     false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_SHARED_LIBRARY_PREFIX", cmProperty::VARIABLE,
     "The prefix for shared libraries that you link to.",
     "The prefix to use for the name of a shared library, lib on UNIX.",
     false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_SHARED_LIBRARY_SUFFIX", cmProperty::VARIABLE,
     "The suffix for shared libraries that you link to.",
     "The suffix to use for the end of a shared library, .dll on Windows.",
     false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_SHARED_MODULE_PREFIX", cmProperty::VARIABLE,
     "The prefix for loadable modules that you link to.",
     "The prefix to use for the name of a loadable module on this platform.",
     false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_SHARED_MODULE_SUFFIX", cmProperty::VARIABLE,
     "The suffix for shared libraries that you link to.",
     "The suffix to use for the end of a loadable module on this platform",
     false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_STATIC_LIBRARY_PREFIX", cmProperty::VARIABLE,
     "The prefix for static libraries that you link to.",
     "The prefix to use for the name of a static library, lib on UNIX.",
     false,
     "Variables that Provide Information");
  cm->DefineProperty
    ("CMAKE_STATIC_LIBRARY_SUFFIX", cmProperty::VARIABLE,
     "The suffix for static libraries that you link to.",
     "The suffix to use for the end of a static library, .lib on Windows.",
     false,
     "Variables that Provide Information");


  // Variables defined by cmake, that change the behavior 
  // of cmake
    cm->DefineProperty
    ("CMAKE_FIND_LIBRARY_PREFIXES",  cmProperty::VARIABLE,
     "Prefixes to prepend when looking for libraries.",
     "This specifies what prefixes to add to library names when "
     "the find_library command looks for libraries. On UNIX "
     "systems this is typically lib, meaning that when trying "
     "to find the foo library it will look for libfoo.",
     false,
     "Variables That Change Behavior");

    cm->DefineProperty
    ("CMAKE_FIND_LIBRARY_SUFFIXES",  cmProperty::VARIABLE,
     "Suffixes to append when looking for libraries.",
     "This specifies what suffixes to add to library names when "
     "the find_library command looks for libraries. On Windows "
     "systems this is typically .lib and .dll, meaning that when trying "
     "to find the foo library it will look for foo.dll etc.",
     false,
     "Variables That Change Behavior");

    cm->DefineProperty
    ("CMAKE_CONFIGURATION_TYPES",  cmProperty::VARIABLE,
     "Specifies the available build types.",
     "This specifies what build types will be available such as "
     "Debug, Release, RelWithDebInfo etc. This has reasonable defaults "
     "on most platforms. But can be extended to provide other "
     "build types. See also CMAKE_BUILD_TYPE.",
     false,
     "Variables That Change Behavior");

    cm->DefineProperty
    ("CMAKE_BUILD_TYPE",  cmProperty::VARIABLE,
     "Specifies the build type for make based generators.",
     "This specifies what build type will be built in this tree. "
     " Possible values are empty, Debug, Release, RelWithDebInfo"
     " and MinSizeRel. This variable is only supported for "
     "make based generators. If this variable is supported, "
     "then CMake will also provide initial values for the "
     "variables with the name "
     " CMAKE_C_FLAGS_[Debug|Release|RelWithDebInfo|MinSizeRel]."
     " For example, if CMAKE_BUILD_TYPE is Debug, then "
     "CMAKE_C_FLAGS_DEBUG will be added to the CMAKE_C_FLAGS.",false,
     "Variables That Change Behavior");

  cm->DefineProperty
    ("CMAKE_BACKWARDS_COMPATIBILITY", cmProperty::VARIABLE,
     "Version of cmake required to build project",
     "From the point of view of backwards compatibility, this "
     "specifies what version of CMake should be supported. By "
     "default this value is the version number of CMake that "
     "you are running. You can set this to an older version of"
     " CMake to support deprecated commands of CMake in projects"
     " that were written to use older versions of CMake. This "
     "can be set by the user or set at the beginning of a "
     "CMakeLists file.",false,
     "Variables That Change Behavior");

  cm->DefineProperty
    ("CMAKE_INSTALL_PREFIX", cmProperty::VARIABLE,
     "Install directory used by install.",
     "If \"make install\" is invoked or INSTALL is built"
     ", this directory is pre-pended onto all install "
     "directories. This variable defaults to /usr/local"
     " on UNIX and c:/Program Files on Windows.",false,
     "Variables That Change Behavior");

  cm->DefineProperty
    ("CMAKE_MODULE_PATH", cmProperty::VARIABLE,
     "Path to look for cmake modules to load.",
     "Specifies a path to override the default seach path for "
     "CMake modules. For example include commands will look "
     "in this path first for modules to include.",
     false,
     "Variables That Change Behavior");

  cm->DefineProperty
    ("CMAKE_PREFIX_PATH", cmProperty::VARIABLE,
     "Path used for searching by FIND_XXX(), with appropriate suffixes added.",
     "Specifies a path which will be used by the FIND_XXX() commands. It "
     "contains the \"base\" directories, the FIND_XXX() commands append "
     "appropriate subdirectories to the base directories. So FIND_PROGRAM() "
     "adds /bin to each of the directories in the path, FIND_LIBRARY() "
     "appends /lib to each of the directories, and FIND_PATH() and "
     "FIND_FILE() append /include . By default it is empty, it is intended "
     "to be set by the project. See also CMAKE_SYSTEM_PREFIX_PATH, "
     "CMAKE_INCLUDE_PATH, CMAKE_LIBRARY_PATH, CMAKE_PROGRAM_PATH.", false,
     "Variables That Change Behavior");

  cm->DefineProperty
    ("CMAKE_INCLUDE_PATH", cmProperty::VARIABLE,
     "Path used for searching by FIND_FILE() and FIND_PATH().",
     "Specifies a path which will be used both by FIND_FILE() and "
     "FIND_PATH(). Both commands will check each of the contained directories "
     "for the existence of the file which is currently searched. By default "
     "it is empty, it is intended to be set by the project. See also "
     "CMAKE_SYSTEM_INCLUDE_PATH, CMAKE_PREFIX_PATH.", false,
     "Variables That Change Behavior");

  cm->DefineProperty
    ("CMAKE_LIBRARY_PATH", cmProperty::VARIABLE,
     "Path used for searching by FIND_LIBRARY().",
     "Specifies a path which will be used by FIND_LIBRARY(). FIND_LIBRARY() "
     "will check each of the contained directories for the existence of the "
     "library which is currently searched. By default it is empty, it is "
     "intended to be set by the project. See also CMAKE_SYSTEM_LIBRARY_PATH, "
     "CMAKE_PREFIX_PATH.", false,
     "Variables That Change Behavior");

  cm->DefineProperty
    ("CMAKE_PROGRAM_PATH", cmProperty::VARIABLE,
     "Path used for searching by FIND_PROGRAM().",
     "Specifies a path which will be used by FIND_PROGRAM(). FIND_PROGRAM() "
     "will check each of the contained directories for the existence of the "
     "program which is currently searched. By default it is empty, it is "
     "intended to be set by the project. See also CMAKE_SYSTEM_PROGRAM_PATH, "
     " CMAKE_PREFIX_PATH.", false,
     "Variables That Change Behavior");

  cm->DefineProperty
    ("CMAKE_SYSTEM_PREFIX_PATH", cmProperty::VARIABLE,
     "Path used for searching by FIND_XXX(), with appropriate suffixes added.",
     "Specifies a path which will be used by the FIND_XXX() commands. It "
     "contains the \"base\" directories, the FIND_XXX() commands append "
     "appropriate subdirectories to the base directories. So FIND_PROGRAM() "
     "adds /bin to each of the directories in the path, FIND_LIBRARY() "
     "appends /lib to each of the directories, and FIND_PATH() and "
     "FIND_FILE() append /include . By default this contains the standard "
     "directories for the current system. It is NOT intended "
     "to be modified by the project, use CMAKE_PREFIX_PATH for this. See also "
     "CMAKE_SYSTEM_INCLUDE_PATH, CMAKE_SYSTEM_LIBRARY_PATH, "
     "CMAKE_SYSTEM_PROGRAM_PATH.", false,
     "Variables That Change Behavior");

  cm->DefineProperty
    ("CMAKE_SYSTEM_INCLUDE_PATH", cmProperty::VARIABLE,
     "Path used for searching by FIND_FILE() and FIND_PATH().",
     "Specifies a path which will be used both by FIND_FILE() and "
     "FIND_PATH(). Both commands will check each of the contained directories "
     "for the existence of the file which is currently searched. By default "
     "it contains the standard directories for the current system. It is "
     "NOT intended to be modified by the project, use CMAKE_INCLUDE_PATH "
     "for this. See also CMAKE_SYSTEM_PREFIX_PATH.", false,
     "Variables That Change Behavior");

  cm->DefineProperty
    ("CMAKE_SYSTEM_LIBRARY_PATH", cmProperty::VARIABLE,
     "Path used for searching by FIND_LIBRARY().",
     "Specifies a path which will be used by FIND_LIBRARY(). FIND_LIBRARY() "
     "will check each of the contained directories for the existence of the "
     "library which is currently searched. By default it contains the "
     "standard directories for the current system. It is NOT intended to be "
     "modified by the project, use CMAKE_SYSTEM_LIBRARY_PATH for this. See "
     "also CMAKE_SYSTEM_PREFIX_PATH.", false,
     "Variables That Change Behavior");

  cm->DefineProperty
    ("CMAKE_SYSTEM_PROGRAM_PATH", cmProperty::VARIABLE,
     "Path used for searching by FIND_PROGRAM().",
     "Specifies a path which will be used by FIND_PROGRAM(). FIND_PROGRAM() "
     "will check each of the contained directories for the existence of the "
     "program which is currently searched. By default it contains the "
     "standard directories for the current system. It is NOT intended to be "
     "modified by the project, use CMAKE_PROGRAM_PATH for this. See also "
     "CMAKE_SYSTEM_PREFIX_PATH.", false,
     "Variables That Change Behavior");

  cm->DefineProperty
    ("CMAKE_USER_MAKE_RULES_OVERRIDE", cmProperty::VARIABLE,
     "Specify a file that can change the build rule variables.",
     "If this variable is set, it should to point to a "
     "CMakeLists.txt file that will be read in by CMake "
     "after all the system settings have been set, but "
     "before they have been used.  This would allow you "
     "to override any variables that need to be changed "
     "for some special project. ",false,
     "Variables That Change Behavior");
  
  cm->DefineProperty
    ("BUILD_SHARED_LIBS", cmProperty::VARIABLE,
     "Global flag to cause add_library to create shared libraries if on.",
     "If present and true, this will cause all libraries to be "
     "built shared unless the library was explicitly added as a "
     "static library.  This variable is often added to projects "
     "as an OPTION so that each user of a project can decide if "
     "they want to build the project using shared or static "
     "libraries.",false,
     "Variables That Change Behavior");
  
  cm->DefineProperty
    ("CMAKE_NOT_USING_CONFIG_FLAGS", cmProperty::VARIABLE,
     "Skip _BUILD_TYPE flags if true.",
     "This is an internal flag used by the generators in "
     "CMake to tell CMake to skip the _BUILD_TYPE flags.",false,
     "Variables That Change Behavior");
  
  cm->DefineProperty
    ("CMAKE_MFC_FLAG", cmProperty::VARIABLE,
     "Tell cmake to use MFC for an executable or dll.",
     "This can be set in a CMakeLists.txt file and will "
     "enable MFC in the application.  It should be set "
     "to 1 for static the static MFC library, and 2 for "
     "the shared MFC library.  This is used in visual "
     "studio 6 and 7 project files.   The CMakeSetup "
     "dialog uses MFC and the CMakeLists.txt looks like this:\n"
     "ADD_DEFINITIONS(-D_AFXDLL)\n"
     "set(CMAKE_MFC_FLAG 2)\n"
     "add_executable(CMakeSetup WIN32 ${SRCS})\n",false,
     "Variables That Change Behavior");

  
  // Variables defined by CMake that describe the system
  
  cm->DefineProperty
    ("CMAKE_SYSTEM", cmProperty::VARIABLE,
     "Name of system cmake is compiling for.",
     "This variable is the composite of CMAKE_SYSTEM_NAME"
     "and CMAKE_SYSTEM_VERSION, like this "
     "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_VERSION}. "
     "If CMAKE_SYSTEM_VERSION is not set, then "
     "CMAKE_SYSTEM is the same as CMAKE_SYSTEM_NAME.",false,
     "Variables That Describe the System");
  cm->DefineProperty
    ("CMAKE_SYSTEM_NAME", cmProperty::VARIABLE,
     "Name of the OS CMake is building for.",
     "This is the name of the operating system on "
     "which CMake is targeting.   On systems that "
     "have the uname command, this variable is set "
     "to the output of uname -s.  Linux, Windows, "
     " and Darwin for Mac OSX are the values found "
     " on the big three operating systems."  ,false,
     "Variables That Describe the System");
  cm->DefineProperty
    ("CMAKE_SYSTEM_PROCESSOR", cmProperty::VARIABLE,
     "The name of the CPU CMake is building for.",
     "On systems that support uname, this variable is "
     "set to the output of uname -p, on windows it is "
     "set to the value of the environment variable "
     "PROCESSOR_ARCHITECTURE",false,
     "Variables That Describe the System");
  cm->DefineProperty
    ("CMAKE_SYSTEM_VERSION", cmProperty::VARIABLE,
     "OS version CMake is building for.",
     "A numeric version string for the system, on "
     "systems that support uname, this variable is "
     "set to the output of uname -r. On other "
     "systems this is set to major-minor version numbers.",false,
     "Variables That Describe the System");

  cm->DefineProperty
    ("CMAKE_HOST_SYSTEM", cmProperty::VARIABLE,
     "Name of system cmake is being run on.",
     "The same as CMAKE_SYSTEM but for the host system instead "
     "of the target system when cross compiling.",false,
     "Variables That Describe the System");
  cm->DefineProperty
    ("CMAKE_HOST_SYSTEM_NAME", cmProperty::VARIABLE,
     "Name of the OS CMake is running on.",
     "The same as CMAKE_SYSTEM_NAME but for the host system instead "
     "of the target system when cross compiling.",false,
     "Variables That Describe the System");
  cm->DefineProperty
    ("CMAKE_HOST_SYSTEM_PROCESSOR", cmProperty::VARIABLE,
     "The name of the CPU CMake is running on.",
     "The same as CMAKE_SYSTEM_PROCESSOR but for the host system instead "
     "of the target system when cross compiling.",false,
     "Variables That Describe the System");
  cm->DefineProperty
    ("CMAKE_HOST_SYSTEM_VERSION", cmProperty::VARIABLE,
     "OS version CMake is running on.",
     "The same as CMAKE_SYSTEM_VERSION but for the host system instead "
     "of the target system when cross compiling.",false,
     "Variables That Describe the System");

  cm->DefineProperty
    ("APPLE", cmProperty::VARIABLE,
     "True if running on Mac OSX.",
     "Set to true on Mac OSX.",false,
     "Variables That Describe the System");
  
  cm->DefineProperty
    ("BORLAND", cmProperty::VARIABLE,
     "True of the borland compiler is being used.",
     "This is set to true if the Borland compiler is being used.",false,
     "Variables That Describe the System");
  
  cm->DefineProperty
    ("CYGWIN", cmProperty::VARIABLE,
     "True for cygwin.",
     "Set to true when using CYGWIN.",false,
     "Variables That Describe the System");
  
  cm->DefineProperty
    ("MSVC", cmProperty::VARIABLE,
     "True when using Microsoft Visual C",
     "Set to true when the compiler is some version of Microsoft Visual C.",
     false,
     "Variables That Describe the System");

  cm->DefineProperty
    ("MSVC80", cmProperty::VARIABLE,
     "True when using Microsoft Visual C 8.0",
     "Set to true when the compiler is version 8.0 of Microsoft Visual C.",
     false,
     "Variables That Describe the System");

  cm->DefineProperty
    ("MSVC_IDE", cmProperty::VARIABLE,
     "True when using the Microsoft Visual C IDE",
     "Set to true when the target platform is the Microsoft Visual C IDE, "
     "as opposed to the command line compiler.",
     false,
     "Variables That Describe the System");

  cm->DefineProperty
    ("MSVC_VERSION", cmProperty::VARIABLE,
     "The version of Microsoft Visual C/C++ being used if any.",
     "The version of Microsoft Visual C/C++ being used if any. "
     "For example 1300 is MSVC 6.0.",
     false,
     "Variables That Describe the System");

  cm->DefineProperty
    ("CMAKE_CL_64", cmProperty::VARIABLE,
     "Using the 64 bit compiler from Microsoft",
     "Set to true when using the 64 bit cl compiler from Microsoft.",
     false,
     "Variables That Describe the System");

  cm->DefineProperty
    ("CMAKE_COMPILER_2005", cmProperty::VARIABLE,
     "Using the Visual Studio 2005 compiler from Microsoft",
     "Set to true when using the Visual Studio 2005 compiler "
     "from Microsoft.",
     false,
     "Variables That Describe the System");

  cm->DefineProperty
    ("UNIX", cmProperty::VARIABLE,
     "True for UNIX and UNIX like operating systems.",
     "Set to true when the target system is UNIX or UNIX like "
     "(i.e. APPLE and CYGWIN).",false,
     "Variables That Describe the System");
  
  cm->DefineProperty
    ("WIN32", cmProperty::VARIABLE,
     "True on windows systems, including win64.",
     "Set to true when the target system is Windows and on cygwin.",false,
     "Variables That Describe the System");

  cm->DefineProperty
    ("CMAKE_HOST_APPLE", cmProperty::VARIABLE,
     "True for Apple OSXoperating systems.",
     "Set to true when the host system is Apple OSX.",
     false,
     "Variables That Describe the System");

  cm->DefineProperty
    ("CMAKE_HOST_UNIX", cmProperty::VARIABLE,
     "True for UNIX and UNIX like operating systems.",
     "Set to true when the host system is UNIX or UNIX like "
     "(i.e. APPLE and CYGWIN).",false,
     "Variables That Describe the System");
  
  cm->DefineProperty
    ("CMAKE_HOST_WIN32", cmProperty::VARIABLE,
     "True on windows systems, including win64.",
     "Set to true when the host system is Windows and on cygwin.",false,
     "Variables That Describe the System");

  // Variables that affect the building of object files and 
  // targets.
  //
  cm->DefineProperty
    ("CMAKE_INSTALL_RPATH", cmProperty::VARIABLE,
     "The rpath to use for installed targets.",
     "A semicolon-separated list specifying the rpath "
     "to use in installed targets (for platforms that support it). "
     "This is used to initialize the target property "
     "INSTALL_RPATH for all targets.",
     false,
     "Variables that Control the Build");

  cm->DefineProperty
    ("CMAKE_INSTALL_RPATH_USE_LINK_PATH", cmProperty::VARIABLE,
     "Add paths to linker search and installed rpath.",
     "CMAKE_INSTALL_RPATH_USE_LINK_PATH is a boolean that if set to true "
     "will append directories in the linker search path and outside the "
     "project to the INSTALL_RPATH. "
     "This is used to initialize the target property "
     "INSTALL_RPATH_USE_LINK_PATH for all targets.",
     false,
     "Variables that Control the Build");

  cm->DefineProperty
    ("CMAKE_INSTALL_NAME_DIR", cmProperty::VARIABLE,
     "Mac OSX directory name for installed targets.",
     "CMAKE_INSTALL_NAME_DIR is used to initialize the "
     "INSTALL_NAME_DIR property on all targets. See that target "
     "property for more information.",
     false,
     "Variables that Control the Build");

  cm->DefineProperty
    ("CMAKE_Fortran_MODULE_DIRECTORY", cmProperty::VARIABLE,
     "Fortran module output directory.",
     "This variable is used to initialize the "
     "Fortran_MODULE_DIRECTORY property on all the targets. "
     "See that target property for additional information.",
     false,
     "Variables that Control the Build");

  cm->DefineProperty
    ("CMAKE_LIBRARY_OUTPUT_DIRECTORY", cmProperty::VARIABLE,
     "Where to put all the LIBRARY targets when built.",
     "This variable is used to initialize the "
     "LIBRARY_OUTPUT_DIRECTORY property on all the targets. "
     "See that target property for additional information.",
     false,
     "Variables that Control the Build");

  cm->DefineProperty
    ("CMAKE_ARCHIVE_OUTPUT_DIRECTORY", cmProperty::VARIABLE,
     "Where to put all the ARCHIVE targets when built.",
     "This variable is used to initialize the "
     "ARCHIVE_OUTPUT_DIRECTORY property on all the targets. "
     "See that target property for additional information.",
     false,
     "Variables that Control the Build");

  cm->DefineProperty
    ("CMAKE_RUNTIME_OUTPUT_DIRECTORY", cmProperty::VARIABLE,
     "Where to put all the RUNTIME targets when built.",
     "This variable is used to initialize the "
     "RUNTIME_OUTPUT_DIRECTORY property on all the targets. "
     "See that target property for additional information.",
     false,
     "Variables that Control the Build");
  

  cm->DefineProperty
    ("CMAKE_DEBUG_POSTFIX", cmProperty::VARIABLE,
     "A postfix to add to targets when build as debug.",
     "This variable is used to initialize the DEBUG_POSTFIX "
     "property on all the targets. If set the postfix will be "
     "appended to any targets built when the configuration is "
     "Debug.",
     false,
     "Variables that Control the Build");
  
  cm->DefineProperty
    ("CMAKE_BUILD_WITH_INSTALL_RPATH", cmProperty::VARIABLE,
     "Use the install path for the RPATH",
     "Normally CMake uses the build tree for the RPATH when building "
     "executables etc on systems that use RPATH. When the software "
     "is installed the executables etc are relinked by CMake to have "
     "the install RPATH. If this variable is set to true then the software "
     "is always built with the install path for the RPATH and does not "
     "need to be relinked when installed.",false,
     "Variables that Control the Build");

  cm->DefineProperty
    ("CMAKE_SKIP_BUILD_RPATH", cmProperty::VARIABLE,
     "Do not include RPATHs in the build tree.",
     "Normally CMake uses the build tree for the RPATH when building "
     "executables etc on systems that use RPATH. When the software "
     "is installed the executables etc are relinked by CMake to have "
     "the install RPATH. If this variable is set to true then the software "
     "is always built with no RPATH.",false,
     "Variables that Control the Build");

  cm->DefineProperty
    ("CMAKE_EXE_LINKER_FLAGS", cmProperty::VARIABLE,
     "Linker flags used to create executables.",
     "Flags used by the linker when creating an executable.",false,
     "Variables that Control the Build");

  cm->DefineProperty
    ("CMAKE_EXE_LINKER_FLAGS_[CMAKE_BUILD_TYPE]", cmProperty::VARIABLE,
     "Flag used when linking an executable.",
     "Same as CMAKE_C_FLAGS_* but used by the linker "
     "when creating executables.",false,
     "Variables that Control the Build");
  cm->DefineProperty
    ("CMAKE_LIBRARY_PATH_FLAG", cmProperty::VARIABLE,
     "The flag used to add a library search path to a compiler.",
     "The flag used to specify a library directory to the compiler. "
     "On most compilers this is \"-L\".",false,
     "Variables that Control the Build");
  cm->DefineProperty
    ("CMAKE_LINK_DEF_FILE_FLAG  ", cmProperty::VARIABLE,
     "Linker flag used to specify a .def file for dll creation.",
     "The flag used to add a .def file when creating "
     "a dll on Windows, this is only defined on Windows.",false,
     "Variables that Control the Build");
  cm->DefineProperty
    ("CMAKE_LINK_LIBRARY_FLAG", cmProperty::VARIABLE,
     "Flag used to link a library into an executable.",
     "The flag used to specify a library to link to an executable.  "
     "On most compilers this is \"-l\".",false,
     "Variables that Control the Build");
  cm->DefineProperty
    ("CMAKE_LINK_LIBRARY_FILE_FLAG", cmProperty::VARIABLE,
     "Flag used to link a library specified by a path to its file.",
     "The flag used before a library file path is given to the linker.  "
     "This is needed only on very few platforms.", false,
     "Variables that Control the Build");
  cm->DefineProperty
    ("CMAKE_USE_RELATIVE_PATHS", cmProperty::VARIABLE,
     "Use relative paths (May not work!).",
     "If this is set to TRUE, then the CMake will use "
     "relative paths between the source and binary tree. "
     "This option does not work for more complicated "
     "projects, and relative paths are used when possible.  "
     "In general, it is not possible to move CMake generated"
     " makefiles to a different location regardless "
     "of the value of this variable.",false,
     "Variables that Control the Build");
  cm->DefineProperty
    ("EXECUTABLE_OUTPUT_PATH", cmProperty::VARIABLE,
     "Old executable location variable.",
     "This variable should no longer be used as of CMake 2.6.  "
     "Use the RUNTIME_OUTPUT_DIRECTORY target property instead.  "
     "It will override this variable if it is set.\n"
     "If set, this is the directory where all executables "
     "built during the build process will be placed.",false,
     "Variables that Control the Build");
  cm->DefineProperty
    ("LIBRARY_OUTPUT_PATH", cmProperty::VARIABLE,
     "Old library location variable.",
     "This variable should no longer be used as of CMake 2.6.  "
     "Use the ARCHIVE_OUTPUT_DIRECTORY, LIBRARY_OUTPUT_DIRECTORY, and "
     "RUNTIME_OUTPUT_DIRECTORY target properties instead.  "
     "They will override this variable if they are set.\n"
     "If set, this is the directory where all the libraries "
     "built during the build process will be placed.",false,
     "Variables that Control the Build");


//   Variables defined when the a language is enabled These variables will
// also be defined whenever CMake has loaded its support for compiling (LANG)
// programs. This support will be loaded whenever CMake is used to compile
// (LANG) files. C and CXX are examples of the most common values for (LANG).

  cm->DefineProperty
    ("CMAKE_USER_MAKE_RULES_OVERRIDE_<LANG>", cmProperty::VARIABLE,
     "Specify a file that can change the build rule variables.",
     "If this variable is set, it should to point to a "
     "CMakeLists.txt file that will be read in by CMake "
     "after all the system settings have been set, but "
     "before they have been used.  This would allow you "
     "to override any variables that need to be changed "
     "for some language. ",false,
     "Variables for Languages");

  cm->DefineProperty
    ("CMAKE_<LANG>_COMPILER", cmProperty::VARIABLE,
     "The full path to the compiler for LANG.",
     "This is the command that will be used as the <LANG> compiler. "
     "Once set, you can not change this variable.",false,
     "Variables for Languages");
  
  cm->DefineProperty
    ("CMAKE_<LANG>_COMPILER_ID", cmProperty::VARIABLE,
     "An internal variable subject to change.",
     "This is used in determining the compiler and is subject to change.",
     false,
     "Variables for Languages");

  cm->DefineProperty
    ("CMAKE_<LANG>_PLATFORM_ID", cmProperty::VARIABLE,
     "An internal variable subject to change.",
     "This is used in determining the platform and is subject to change.",
     false,
     "Variables for Languages");

  cm->DefineProperty
    ("CMAKE_<LANG>_COMPILER_ABI", cmProperty::VARIABLE,
     "An internal variable subject to change.",
     "This is used in determining the compiler ABI and is subject to change.",
     false,
     "Variables for Languages");

  cm->DefineProperty
    ("CMAKE_INTERNAL_PLATFORM_ABI", cmProperty::VARIABLE,
     "An internal variable subject to change.",
     "This is used in determining the compiler ABI and is subject to change.",
     false,
     "Variables for Languages");

  cm->DefineProperty
    ("CMAKE_<LANG>_SIZEOF_DATA_PTR", cmProperty::VARIABLE,
     "An internal variable subject to change.",
     "This is used in determining the architecture and is subject to change.",
     false,
     "Variables for Languages");

  cm->DefineProperty
    ("CMAKE_COMPILER_IS_GNU<LANG>", cmProperty::VARIABLE,
     "True if the compiler is GNU.",
     "If the selected <LANG> compiler is the GNU "
     "compiler then this is TRUE, if not it is FALSE.",false,
     "Variables for Languages");
  
  cm->DefineProperty
    ("CMAKE_<LANG>_FLAGS_DEBUG", cmProperty::VARIABLE,
     "Flags for Debug build type or configuration.",
     "<LANG> flags used when CMAKE_BUILD_TYPE is Debug.",false,
     "Variables for Languages");
  
  cm->DefineProperty
    ("CMAKE_<LANG>_FLAGS_MINSIZEREL", cmProperty::VARIABLE,
     "Flags for MinSizeRel build type or configuration.",
     "<LANG> flags used when CMAKE_BUILD_TYPE is MinSizeRel."
     "Short for minimum size release.",false,
     "Variables for Languages");
  
  cm->DefineProperty
    ("CMAKE_<LANG>_FLAGS_RELEASE", cmProperty::VARIABLE,
     "Flags for Release build type or configuration.",
     "<LANG> flags used when CMAKE_BUILD_TYPE is Release",false,
     "Variables for Languages");
  
  cm->DefineProperty
    ("CMAKE_<LANG>_FLAGS_RELWITHDEBINFO", cmProperty::VARIABLE,
     "Flags for RelWithDebInfo type or configuration.",
     "<LANG> flags used when CMAKE_BUILD_TYPE is RelWithDebInfo. "
     "Short for Release With Debug Information.",false,
     "Variables for Languages");
  
  cm->DefineProperty
    ("CMAKE_<LANG>_COMPILE_OBJECT", cmProperty::VARIABLE,
     "Rule variable to compile a single object file.",
     "This is a rule variable that tells CMake how to "
     "compile a single object file for for the language <LANG>.",false,
     "Variables for Languages");
  
  cm->DefineProperty
    ("CMAKE_<LANG>_CREATE_SHARED_LIBRARY", cmProperty::VARIABLE,
     "Rule variable to create a shared library.",
     "This is a rule variable that tells CMake how to "
     "create a shared library for the language <LANG>.",false,
     "Variables for Languages");
  
  cm->DefineProperty
    ("CMAKE_<LANG>_CREATE_SHARED_MODULE", cmProperty::VARIABLE,
     "Rule variable to create a shared module.",
     "This is a rule variable that tells CMake how to "
     "create a shared library for the language <LANG>.",false,
     "Variables for Languages");
  
  cm->DefineProperty
    ("CMAKE_<LANG>_CREATE_STATIC_LIBRARY", cmProperty::VARIABLE,
     "Rule variable to create a static library.",
     "This is a rule variable that tells CMake how "
     "to create a static library for the language <LANG>.",false,
     "Variables for Languages");

  cm->DefineProperty
    ("CMAKE_<LANG>_ARCHIVE_CREATE", cmProperty::VARIABLE,
     "Rule variable to create a new static archive.",
     "This is a rule variable that tells CMake how to create a static "
     "archive.  It is used in place of CMAKE_<LANG>_CREATE_STATIC_LIBRARY "
     "on some platforms in order to support large object counts.  "
     "See also CMAKE_<LANG>_ARCHIVE_APPEND and CMAKE_<LANG>_ARCHIVE_FINISH.",
     false, "Variables for Languages");

  cm->DefineProperty
    ("CMAKE_<LANG>_ARCHIVE_APPEND", cmProperty::VARIABLE,
     "Rule variable to append to a static archive.",
     "This is a rule variable that tells CMake how to append to a static "
     "archive.  It is used in place of CMAKE_<LANG>_CREATE_STATIC_LIBRARY "
     "on some platforms in order to support large object counts.  "
     "See also CMAKE_<LANG>_ARCHIVE_CREATE and CMAKE_<LANG>_ARCHIVE_FINISH.",
     false, "Variables for Languages");

  cm->DefineProperty
    ("CMAKE_<LANG>_ARCHIVE_FINISH", cmProperty::VARIABLE,
     "Rule variable to finish an existing static archive.",
     "This is a rule variable that tells CMake how to finish a static "
     "archive.  It is used in place of CMAKE_<LANG>_CREATE_STATIC_LIBRARY "
     "on some platforms in order to support large object counts.  "
     "See also CMAKE_<LANG>_ARCHIVE_CREATE and CMAKE_<LANG>_ARCHIVE_APPEND.",
     false, "Variables for Languages");

  cm->DefineProperty
    ("CMAKE_<LANG>_IGNORE_EXTENSIONS", cmProperty::VARIABLE,
     "File extensions that should be ignored by the build.",
     "This is a list of file extensions that may be "
     "part of a project for a given language but are not compiled. ",false,
     "Variables for Languages");
  
  cm->DefineProperty
    ("CMAKE_<LANG>_LINKER_PREFERENCE", cmProperty::VARIABLE,
     "Determine if a language should be used for linking.",
     "If this is \"Preferred\" then if there is a mixed "
     "language shared library or executable, then this "
     "languages linker command will be used.",false,
     "Variables for Languages");
  
  cm->DefineProperty
    ("CMAKE_<LANG>_LINK_EXECUTABLE ", cmProperty::VARIABLE,
     "Rule variable to link and executable.",
     "Rule variable to link and executable for the given language.",false,
     "Variables for Languages");
  
  cm->DefineProperty
    ("CMAKE_<LANG>_OUTPUT_EXTENSION", cmProperty::VARIABLE,
     "Extension for the output of a compile for a single file.",
     "This is the extension for an object file for "
     "the given <LANG>. For example .obj for C on Windows.",false,
     "Variables for Languages");
  
  cm->DefineProperty
    ("CMAKE_<LANG>_SOURCE_FILE_EXTENSIONS", cmProperty::VARIABLE,
     "Extensions of source files for the given language.",
     "This is the list of extensions for a "
     "given languages source files.",false,"Variables for Languages");

  // variables that are used by cmake but not to be documented
  cm->DefineProperty("CMAKE_MATCH_0", cmProperty::VARIABLE,0,0);  
  cm->DefineProperty("CMAKE_MATCH_1", cmProperty::VARIABLE,0,0);  
  cm->DefineProperty("CMAKE_MATCH_2", cmProperty::VARIABLE,0,0);  
  cm->DefineProperty("CMAKE_MATCH_3", cmProperty::VARIABLE,0,0);  
  cm->DefineProperty("CMAKE_MATCH_4", cmProperty::VARIABLE,0,0);  
  cm->DefineProperty("CMAKE_MATCH_5", cmProperty::VARIABLE,0,0);  
  cm->DefineProperty("CMAKE_MATCH_6", cmProperty::VARIABLE,0,0);  
  cm->DefineProperty("CMAKE_MATCH_7", cmProperty::VARIABLE,0,0);  
  cm->DefineProperty("CMAKE_MATCH_8", cmProperty::VARIABLE,0,0);  
  cm->DefineProperty("CMAKE_MATCH_9", cmProperty::VARIABLE,0,0);  

  cm->DefineProperty("CMAKE_<LANG>_COMPILER_ARG1",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_COMPILER_ENV_VAR",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_COMPILER_ID_RUN",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_COMPILER_LOADED",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_CREATE_ASSEMBLY_SOURCE",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_CREATE_PREPROCESSED_SOURCE",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_FLAGS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_FLAGS_DEBUG_INIT",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_FLAGS_INIT",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_FLAGS_MINSIZEREL_INIT",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_FLAGS_RELEASE_INIT",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_FLAGS_RELWITHDEBINFO_INIT",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_INFORMATION_LOADED",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_LINK_EXECUTABLE",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_LINK_FLAGS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_STANDARD_LIBRARIES",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_STANDARD_LIBRARIES_INIT",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_<LANG>_USE_RESPONSE_FILE_FOR_OBJECTS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_EXECUTABLE_SUFFIX_<LANG>",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_EXE_LINK_DYNAMIC_<LANG>_FLAGS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_EXE_LINK_STATIC_<LANG>_FLAGS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_GENERATOR_<LANG>",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_IMPORT_LIBRARY_PREFIX_<LANG>",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_IMPORT_LIBRARY_SUFFIX_<LANG>",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_INCLUDE_FLAG_<LANG>",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_INCLUDE_FLAG_SEP_<LANG>",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_INCLUDE_SYSTEM_FLAG_<LANG>",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_NEEDS_REQUIRES_STEP_<LANG>_FLAG",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_LIBRARY_CREATE_<LANG>_FLAGS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_LIBRARY_<LANG>_FLAGS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_LIBRARY_LINK_<LANG>_FLAGS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_LIBRARY_LINK_DYNAMIC_<LANG>_FLAGS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_LIBRARY_LINK_STATIC_<LANG>_FLAGS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_LIBRARY_RUNTIME_<LANG>_FLAG",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_LIBRARY_RUNTIME_<LANG>_FLAG_SEP",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_LIBRARY_RPATH_LINK_<LANG>_FLAG",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_EXECUTABLE_RUNTIME_<LANG>_FLAG",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_EXECUTABLE_RUNTIME_<LANG>_FLAG_SEP",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_EXECUTABLE_RPATH_LINK_<LANG>_FLAG",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_PLATFORM_REQUIRED_RUNTIME_PATH",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_MODULE_CREATE_<LANG>_FLAGS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_MODULE_<LANG>_FLAGS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_MODULE_LINK_DYNAMIC_<LANG>_FLAGS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_MODULE_LINK_STATIC_<LANG>_FLAGS",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_MODULE_RUNTIME_<LANG>_FLAG",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_SHARED_MODULE_RUNTIME_<LANG>_FLAG_SEP",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_LINK_DEPENDENT_LIBRARY_FILES",
                     cmProperty::VARIABLE,0,0);
  cm->DefineProperty("CMAKE_LINK_DEPENDENT_LIBRARY_DIRS",
                     cmProperty::VARIABLE,0,0);
}
