/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmRemoveDefinitionsCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmRemoveDefinitionsCommand_h
#define cmRemoveDefinitionsCommand_h

#include "cmCommand.h"

/** \class cmRemoveDefinitionsCommand
 * \brief Specify a list of compiler defines
 *
 * cmRemoveDefinitionsCommand specifies a list of compiler defines. 
 * These defines will
 * be removed from the compile command.  
 */
class cmRemoveDefinitionsCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmRemoveDefinitionsCommand;
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
  virtual const char* GetName() {return "remove_definitions";}
  
  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Removes -D define flags added by add_definitions.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  remove_definitions(-DFOO -DBAR ...)\n"
      "Removes flags (added by add_definitions) from the compiler command "
      "line for sources in the current directory and below.";
    }
  
  cmTypeMacro(cmRemoveDefinitionsCommand, cmCommand);
};



#endif
