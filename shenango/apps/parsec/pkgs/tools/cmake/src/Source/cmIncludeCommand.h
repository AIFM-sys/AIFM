/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmIncludeCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:07 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmIncludeCommand_h
#define cmIncludeCommand_h

#include "cmCommand.h"

/** \class cmIncludeCommand
 * \brief 
 *
 *  cmIncludeCommand defines a list of distant
 *  files that can be "included" in the current list file.
 *  In almost every sense, this is identical to a C/C++
 *  #include command.  Arguments are first expended as usual.
 */
class cmIncludeCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmIncludeCommand;
    }

  /**
   * This is called when the command is first encountered in
   * the CMakeLists.txt file.
   */
  virtual bool InitialPass(std::vector<std::string> const& args,
                           cmExecutionStatus &status);

  /**
   * This determines if the command is invoked when in script mode.
   */
  virtual bool IsScriptable() { return true; }

  /**
   * The name of the command as specified in CMakeList.txt.
   */
  virtual const char* GetName() {return "include";}
  
  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Read CMake listfile code from the given file.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  include(file1 [OPTIONAL] [RESULT_VARIABLE <VAR>])\n"
      "  include(module [OPTIONAL] [RESULT_VARIABLE <VAR>])\n"
      "Reads CMake listfile code from the given file.  Commands in the file "
      "are processed immediately as if they were written in place of the "
      "include command.  If OPTIONAL is present, then no error "
      "is raised if the file does not exist.  If RESULT_VARIABLE is given "
      "the variable will be set to the full filename which "
      "has been included or NOTFOUND if it failed.\n"
      "If a module is specified instead of a file, the file with name "
      "<modulename>.cmake is searched in the CMAKE_MODULE_PATH.";
    }
  
  cmTypeMacro(cmIncludeCommand, cmCommand);
};



#endif
