/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmGetTargetPropertyCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmGetTargetPropertyCommand_h
#define cmGetTargetPropertyCommand_h

#include "cmCommand.h"

class cmGetTargetPropertyCommand : public cmCommand
{
public:
  virtual cmCommand* Clone() 
    {
      return new cmGetTargetPropertyCommand;
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
  virtual const char* GetName() { return "get_target_property";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Get a property from a target.";
    }
  
  /**
   * Longer documentation.
   */
  virtual const char* GetFullDocumentation()
    {
      return
        "  get_target_property(VAR target property)\n"
        "Get a property from a target.   The value of the property is "
        "stored in the variable VAR.  If the property is not found, VAR "
        "will be set to \"NOTFOUND\".  Use set_target_properties to set "
        "property values.  Properties are usually used to control how "
        "a target is built.\n"
        "The read-only property \"<CONFIG>_LOCATION\" provides the full "
        "path to the file on disk that will be created for the target when "
        "building under configuration <CONFIG> "
        "(in upper-case, such as \"DEBUG_LOCATION\"). "
        "The read-only property \"LOCATION\" specifies "
        "the full path to the file on disk that will be created for the "
        "target. The path may contain a build-system-specific portion that "
        "is replaced at build time with the configuration getting built "
        "(such as \"$(ConfigurationName)\" in VS). "
        "This is very useful for executable targets to get "
        "the path to the executable file for use in a custom command.\n"
        "The read-only property \"TYPE\" returns which type the specified "
        "target has (EXECUTABLE, STATIC_LIBRARY, SHARED_LIBRARY, "
        "MODULE_LIBRARY, UTILITY, INSTALL_FILES or INSTALL_PROGRAMS). "
        "This command can get properties for any target so far created. "
        "The targets do not need to be in the current CMakeLists.txt file.";
    }
  
  cmTypeMacro(cmGetTargetPropertyCommand, cmCommand);
};



#endif
