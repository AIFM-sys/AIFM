/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmAddDefinitionsCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmAddDefinitionsCommand_h
#define cmAddDefinitionsCommand_h

#include "cmCommand.h"

/** \class cmAddDefinitionsCommand
 * \brief Specify a list of compiler defines
 *
 * cmAddDefinitionsCommand specifies a list of compiler defines. These defines
 * will be added to the compile command.
 */
class cmAddDefinitionsCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone()
    {
    return new cmAddDefinitionsCommand;
    }

  /**
   * This is called when the command is first encountered in
   * the CMakeLists.txt file.
   */
  virtual bool InitialPass(std::vector<std::string> const& args,
                           cmExecutionStatus &status);

  /**
   * The name of the command as specified in CMakeList.txt.
   */
  virtual const char* GetName() {return "add_definitions";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation()
    {
    return "Adds -D define flags to the compilation of source files.";
    }

  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  add_definitions(-DFOO -DBAR ...)\n"
      "Adds flags to the compiler command line for sources in the current "
      "directory and below.  This command can be used to add any flags, "
      "but it was originally intended to add preprocessor definitions.  "
      "Flags beginning in -D or /D that look like preprocessor definitions "
      "are automatically added to the COMPILE_DEFINITIONS property for "
      "the current directory.  Definitions with non-trival values may be "
      "left in the set of flags instead of being converted for reasons of "
      "backwards compatibility.  See documentation of the directory, "
      "target, and source file COMPILE_DEFINITIONS properties for details "
      "on adding preprocessor definitions to specific scopes and "
      "configurations."
      ;
    }

  cmTypeMacro(cmAddDefinitionsCommand, cmCommand);
private:
  bool ParseDefinition(std::string const& def);
};



#endif
