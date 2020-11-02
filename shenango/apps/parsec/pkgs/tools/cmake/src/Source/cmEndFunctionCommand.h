/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmEndFunctionCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmEndFunctionCommand_h
#define cmEndFunctionCommand_h

#include "cmCommand.h"

/** \class cmEndFunctionCommand
 * \brief ends an if block
 *
 * cmEndFunctionCommand ends an if block
 */
class cmEndFunctionCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmEndFunctionCommand;
    }

  /**
   * Override cmCommand::InvokeInitialPass to get arguments before
   * expansion.
   */
  virtual bool InvokeInitialPass(std::vector<cmListFileArgument> const&,
                                 cmExecutionStatus &);
  
  /**
   * This is called when the command is first encountered in
   * the CMakeLists.txt file.
   */
  virtual bool InitialPass(std::vector<std::string> const&,
                           cmExecutionStatus &) {return false;}

  /**
   * This determines if the command is invoked when in script mode.
   */
  virtual bool IsScriptable() { return true; }

  /**
   * The name of the command as specified in CMakeList.txt.
   */
  virtual const char* GetName() { return "endfunction";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Ends a list of commands in a function block.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  endfunction(expression)\n"
      "See the function command.";
    }
  
  cmTypeMacro(cmEndFunctionCommand, cmCommand);
};


#endif
