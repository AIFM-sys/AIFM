/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmFindPathCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:07 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmFindPathCommand_h
#define cmFindPathCommand_h

#include "cmFindBase.h"


/** \class cmFindPathCommand
 * \brief Define a command to search for a library.
 *
 * cmFindPathCommand is used to define a CMake variable
 * that specifies a library. The command searches for a given
 * file in a list of directories.
 */
class cmFindPathCommand : public cmFindBase
{
public:
  cmFindPathCommand();
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmFindPathCommand;
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
  virtual const char* GetName() {return "find_path";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Find the directory containing a file.";
    }

  virtual const char* GetFullDocumentation();
  cmTypeMacro(cmFindPathCommand, cmFindBase);
  bool IncludeFileInPath;
  bool ExtraDocAdded;
private:
  std::string FindHeaderInFramework(std::string const& file,
                                    std::string const& dir);
  std::string FindHeader();
  std::string FindNormalHeader();
  std::string FindFrameworkHeader();
};



#endif
