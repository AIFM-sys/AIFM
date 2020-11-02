/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCTestUpdateCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmCTestUpdateCommand_h
#define cmCTestUpdateCommand_h

#include "cmCTestHandlerCommand.h"

/** \class cmCTestUpdate
 * \brief Run a ctest script
 *
 * cmCTestUpdateCommand defineds the command to updates the repository.
 */
class cmCTestUpdateCommand : public cmCTestHandlerCommand
{
public:

  cmCTestUpdateCommand() {}

  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone()
    {
    cmCTestUpdateCommand* ni = new cmCTestUpdateCommand;
    ni->CTest = this->CTest;
    ni->CTestScriptHandler = this->CTestScriptHandler;
    return ni;
    }

  /**
   * The name of the command as specified in CMakeList.txt.
   */
  virtual const char* GetName() { return "ctest_update";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation()
    {
    return "Updates the repository.";
    }

  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  ctest_update([SOURCE source] [RETURN_VALUE res])\n"
      "Updates the given source directory and stores results in Update.xml. "
      "The second argument is a variable that will hold the number of files "
      "modified. If there is a problem, the variable will be -1.";
    }

  cmTypeMacro(cmCTestUpdateCommand, cmCTestHandlerCommand);

protected:
  cmCTestGenericHandler* InitializeHandler();
};


#endif
