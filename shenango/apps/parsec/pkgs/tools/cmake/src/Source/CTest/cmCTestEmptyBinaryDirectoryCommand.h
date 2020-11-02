/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCTestEmptyBinaryDirectoryCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmCTestEmptyBinaryDirectoryCommand_h
#define cmCTestEmptyBinaryDirectoryCommand_h

#include "cmCTestCommand.h"

/** \class cmCTestEmptyBinaryDirectory
 * \brief Run a ctest script
 *
 * cmLibrarysCommand defines a list of executable (i.e., test)
 * programs to create.
 */
class cmCTestEmptyBinaryDirectoryCommand : public cmCTestCommand
{
public:

  cmCTestEmptyBinaryDirectoryCommand() {}

  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone()
    {
    cmCTestEmptyBinaryDirectoryCommand* ni
      = new cmCTestEmptyBinaryDirectoryCommand;
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
  virtual const char* GetName() { return "ctest_empty_binary_directory";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation()
    {
    return "empties the binary directory";
    }

  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  ctest_empty_binary_directory( directory )\n"
      "Removes a binary directory. This command will perform some checks "
      "prior to deleting the directory in an attempt to avoid malicious "
      "or accidental directory deletion.";
    }

  cmTypeMacro(cmCTestEmptyBinaryDirectoryCommand, cmCTestCommand);

};


#endif
