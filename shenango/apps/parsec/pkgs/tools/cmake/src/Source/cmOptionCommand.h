/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmOptionCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmOptionCommand_h
#define cmOptionCommand_h

#include "cmCommand.h"

/** \class cmOptionCommand
 * \brief Provide an option to the user
 *
 * cmOptionCommand provides an option for the user to select
 */
class cmOptionCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmOptionCommand;
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
  virtual const char* GetName() {return "option";}
  
  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Provides an option that the user can optionally select.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  option(<option_variable> \"help string describing option\"\n"
      "         [initial value])\n"
      "Provide an option for the user to select as ON or OFF.  If no "
      "initial value is provided, OFF is used.";
    }

  /**
   * This determines if the command is invoked when in script mode.
   */
  virtual bool IsScriptable() { return true; }

  cmTypeMacro(cmOptionCommand, cmCommand);
};



#endif
