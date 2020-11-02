/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCTestStartCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmCTestStartCommand_h
#define cmCTestStartCommand_h

#include "cmCTestCommand.h"

/** \class cmCTestStart
 * \brief Run a ctest script
 *
 * cmCTestStartCommand defineds the command to start the nightly testing.
 */
class cmCTestStartCommand : public cmCTestCommand
{
public:

  cmCTestStartCommand() {}

  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone()
    {
    cmCTestStartCommand* ni = new cmCTestStartCommand;
    ni->CTest = this->CTest;
    ni->CTestScriptHandler = this->CTestScriptHandler;
    return ni;
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
  virtual const char* GetName() { return "ctest_start";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation()
    {
    return "Starts the testing for a given model";
    }

  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  ctest_start(Model [TRACK <track>] [source [binary]])\n"
      "Starts the testing for a given model. The command should be called "
      "after the binary directory is initialized. If the 'source' and "
      "'binary' directory are not specified, it reads the "
      "CTEST_SOURCE_DIRECTORY and CTEST_BINARY_DIRECTORY. If the track is "
      "specified, the submissions will go to the specified track.";
    }

  cmTypeMacro(cmCTestStartCommand, cmCTestCommand);

};


#endif
