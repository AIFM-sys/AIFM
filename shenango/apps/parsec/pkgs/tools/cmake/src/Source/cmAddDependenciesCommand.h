/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmAddDependenciesCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmDependenciessCommand_h
#define cmDependenciessCommand_h

#include "cmCommand.h"

/** \class cmAddDependenciesCommand
 * \brief Add a dependency to a target
 *
 * cmAddDependenciesCommand adds a dependency to a target
 */
class cmAddDependenciesCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmAddDependenciesCommand;
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
  virtual const char* GetName() { return "add_dependencies";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Add a dependency between top-level targets.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  add_dependencies(target-name depend-target1\n"
      "                   depend-target2 ...)\n"
      "Make a top-level target depend on other top-level targets.  A "
      "top-level target is one created by ADD_EXECUTABLE, ADD_LIBRARY, "
      "or ADD_CUSTOM_TARGET.  Adding dependencies with this command "
      "can be used to make sure one target is built before another target.  "
      "See the DEPENDS option of ADD_CUSTOM_TARGET "
      "and ADD_CUSTOM_COMMAND for adding file-level dependencies in custom "
      "rules.  See the OBJECT_DEPENDS option in "
      "SET_SOURCE_FILES_PROPERTIES to add file-level dependencies to object "
      "files.";
    }
  
  cmTypeMacro(cmAddDependenciesCommand, cmCommand);
};


#endif
