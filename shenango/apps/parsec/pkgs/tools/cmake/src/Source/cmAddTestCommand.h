/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmAddTestCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmAddTestCommand_h
#define cmAddTestCommand_h

#include "cmCommand.h"

/** \class cmAddTestCommand
 * \brief Add a test to the lists of tests to run.
 *
 * cmAddTestCommand adds a test to the list of tests to run .
 */
class cmAddTestCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmAddTestCommand;
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
  virtual const char* GetName() { return "add_test";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Add a test to the project with the specified arguments.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  add_test(testname Exename arg1 arg2 ...)\n"
      "If the ENABLE_TESTING command has been run, this command adds a "
      "test target to the current directory. If ENABLE_TESTING has not "
      "been run, this command does nothing.  "
      "The tests are run by the testing subsystem by executing Exename "
      "with the specified arguments.  Exename can be either an executable "
      "built by this project or an arbitrary executable on the "
      "system (like tclsh).  The test will be run with the current working "
      "directory set to the CMakeList.txt files corresponding directory "
      "in the binary tree.";
    }
  
  cmTypeMacro(cmAddTestCommand, cmCommand);

};


#endif
