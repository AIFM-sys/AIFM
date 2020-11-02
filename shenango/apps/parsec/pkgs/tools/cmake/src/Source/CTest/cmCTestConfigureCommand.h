/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCTestConfigureCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmCTestConfigureCommand_h
#define cmCTestConfigureCommand_h

#include "cmCTestHandlerCommand.h"

/** \class cmCTestConfigure
 * \brief Run a ctest script
 *
 * cmCTestConfigureCommand defineds the command to configures the project.
 */
class cmCTestConfigureCommand : public cmCTestHandlerCommand
{
public:

  cmCTestConfigureCommand() {}

  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone()
    {
    cmCTestConfigureCommand* ni = new cmCTestConfigureCommand;
    ni->CTest = this->CTest;
    ni->CTestScriptHandler = this->CTestScriptHandler;
    return ni;
    }

  /**
   * The name of the command as specified in CMakeList.txt.
   */
  virtual const char* GetName() { return "ctest_configure";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation()
    {
    return "Configures the repository.";
    }

  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  ctest_configure(BUILD build_dir RETURN_VALUE res)\n"
      "Configures the given build directory and stores results in "
      "Configure.xml. The second argument is a variable that will hold "
      "return value.";
    }

  cmTypeMacro(cmCTestConfigureCommand, cmCTestHandlerCommand);

protected:
  cmCTestGenericHandler* InitializeHandler();

};


#endif
