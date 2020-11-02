/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmSetTargetPropertiesCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:07 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmSetTargetsPropertiesCommand_h
#define cmSetTargetsPropertiesCommand_h

#include "cmCommand.h"

class cmSetTargetPropertiesCommand : public cmCommand
{
public:
  virtual cmCommand* Clone() 
    {
      return new cmSetTargetPropertiesCommand;
    }

  /**
   * This is called when the command is first encountered in
   * the input file.
   */
  virtual bool InitialPass(std::vector<std::string> const& args,
                           cmExecutionStatus &status);

  /**
   * The name of the command as specified in CMakeList.txt.
   */
  virtual const char* GetName() { return "set_target_properties";}  

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Targets can have properties that affect how they are built.";
    }
  
  /**
   *  Used by this command and cmSetPropertiesCommand
   */
  static bool SetOneTarget(const char *tname, 
                           std::vector<std::string> &propertyPairs, 
                           cmMakefile *mf);

  /**
   * Longer documentation.
   */
  virtual const char* GetFullDocumentation()
    {
      return
        "  set_target_properties(target1 target2 ...\n"
        "                        PROPERTIES prop1 value1\n"
        "                        prop2 value2 ...)\n"
        "Set properties on a target. The syntax for the command is to "
        "list all the files you want "
        "to change, and then provide the values you want to set next.  "
        "You can use any prop value pair you want and "
        "extract it later with the GET_TARGET_PROPERTY command.\n"
        "Properties that affect the name of a target's output file are "
        "as follows.  "
        "The PREFIX and SUFFIX properties override the default target name "
        "prefix (such as \"lib\") and suffix (such as \".so\"). "
        "IMPORT_PREFIX and IMPORT_SUFFIX are the equivalent properties for "
        "the import library corresponding to a DLL "
        "(for SHARED library targets).  "
        "OUTPUT_NAME sets the real name of a target when it is built and "
        "can be used to help create two targets of the same name even though "
        "CMake requires unique logical target names.  There is also a "
        "<CONFIG>_OUTPUT_NAME that can set the output name on a "
        "per-configuration basis.  "
        "<CONFIG>_POSTFIX sets a postfix for the real name of the target "
        "when it is built under the configuration named by <CONFIG> "
        "(in upper-case, such as \"DEBUG_POSTFIX\").  The value of "
        "this property is initialized when the target is created to the "
        "value of the variable CMAKE_<CONFIG>_POSTFIX (except for executable "
        "targets because earlier CMake versions which did not use this "
        "variable for executables)."
        "\n"
        "The LINK_FLAGS property can be used to add extra flags to the "
        "link step of a target. LINK_FLAGS_<CONFIG> will add to the "
        "configuration <CONFIG>, "
        "for example, DEBUG, RELEASE, MINSIZEREL, RELWITHDEBINFO. "
        "DEFINE_SYMBOL sets the name of the preprocessor symbol defined when "
        "compiling sources in a shared library. "
        "If not set here then it is set to target_EXPORTS by default "
        "(with some substitutions if the target is not a valid C "
        "identifier). This is useful for headers to know whether they are "
        "being included from inside their library our outside to properly "
        "setup dllexport/dllimport decorations. "
        "The COMPILE_FLAGS property sets additional compiler flags used "
        "to build sources within the target.  It may also be used to pass "
        "additional preprocessor definitions."
        "\n"
        "The LINKER_LANGUAGE property is used to change the tool "
        "used to link an executable or shared library. The default is "
        "set the language to match the files in the library. CXX and C "
        "are common values for this property."
        "\n"
        "For shared libraries VERSION and SOVERSION can be used to specify "
        "the build version and api version respectively. When building or "
        "installing appropriate symlinks are created if the platform "
        "supports symlinks and the linker supports so-names. "
        "If only one of both is specified the missing is assumed to have "
        "the same version number. "
        "For executables VERSION can be used to specify the build version. "
        "When building or installing appropriate symlinks are created if "
        "the platform supports symlinks. "
        "For shared libraries and executables on Windows the VERSION "
        "attribute is parsed to extract a \"major.minor\" version number. "
        "These numbers are used as the image version of the binary. "
        "\n"
        "There are a few properties used to specify RPATH rules. "
        "INSTALL_RPATH is a semicolon-separated list specifying the rpath "
        "to use in installed targets (for platforms that support it). "
        "INSTALL_RPATH_USE_LINK_PATH is a boolean that if set to true will "
        "append directories in the linker search path and outside the "
        "project to the INSTALL_RPATH. "
        "SKIP_BUILD_RPATH is a boolean specifying whether to skip automatic "
        "generation of an rpath allowing the target to run from the "
        "build tree. "
        "BUILD_WITH_INSTALL_RPATH is a boolean specifying whether to link "
        "the target in the build tree with the INSTALL_RPATH.  This takes "
        "precedence over SKIP_BUILD_RPATH and avoids the need for relinking "
        "before installation.  INSTALL_NAME_DIR is a string specifying the "
        "directory portion of the \"install_name\" field of shared libraries "
        "on Mac OSX to use in the installed targets. "
        "When the target is created the values of "
        "the variables CMAKE_INSTALL_RPATH, "
        "CMAKE_INSTALL_RPATH_USE_LINK_PATH, CMAKE_SKIP_BUILD_RPATH, "
        "CMAKE_BUILD_WITH_INSTALL_RPATH, and CMAKE_INSTALL_NAME_DIR "
        "are used to initialize these properties.\n"
        "PROJECT_LABEL can be used to change the name of "
        "the target in an IDE like visual studio.  VS_KEYWORD can be set "
        "to change the visual studio keyword, for example QT integration "
        "works better if this is set to Qt4VSv1.0.\n"
        "When a library is built CMake by default generates code to remove "
        "any existing library using all possible names.  This is needed "
        "to support libraries that switch between STATIC and SHARED by "
        "a user option.  However when using OUTPUT_NAME to build a static "
        "and shared library of the same name using different logical target "
        "names the two targets will remove each other's files.  This can be "
        "prevented by setting the CLEAN_DIRECT_OUTPUT property to 1.\n"
        "The PRE_INSTALL_SCRIPT and POST_INSTALL_SCRIPT properties are the "
        "old way to specify CMake scripts to run before and after "
        "installing a target.  They are used only when the old "
        "INSTALL_TARGETS command is used to install the target.  Use the "
        "INSTALL command instead."
        "\n"
        "The EXCLUDE_FROM_DEFAULT_BUILD property is used by the visual "
        "studio generators.  If it is set to 1 the target will not be "
        "part of the default build when you select \"Build Solution\"."
        ;
    }
  
  cmTypeMacro(cmSetTargetPropertiesCommand, cmCommand);
};



#endif
