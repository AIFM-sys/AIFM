/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmMessageCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:07 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmMessageCommand_h
#define cmMessageCommand_h

#include "cmCommand.h"

/** \class cmMessageCommand
 * \brief Displays a message to the user
 *
 */
class cmMessageCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmMessageCommand;
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
  virtual const char* GetName() { return "message";}

  /**
   * This determines if the command is invoked when in script mode.
   */
  virtual bool IsScriptable() { return true; }

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Display a message to the user.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  message([SEND_ERROR | STATUS | FATAL_ERROR]\n"
      "          \"message to display\" ...)\n"
      "By default the message is displayed in a pop up window (CMakeSetup), "
      "or in the stdout of cmake, or the error section of ccmake. "
      "If the first argument is "
      "SEND_ERROR then an error is raised, and the generate phase will "
      "be skipped.  If the first argument is FATAL_ERROR, all processing "
      "is halted. If the first argument is STATUS then the message is "
      "displayed in the progress line for the GUI, or with a -- in the "
      "command line cmake.";
    }
  
  cmTypeMacro(cmMessageCommand, cmCommand);
};


#endif
