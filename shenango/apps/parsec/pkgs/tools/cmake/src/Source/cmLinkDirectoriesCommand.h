/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmLinkDirectoriesCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmLinkDirectoriesCommand_h
#define cmLinkDirectoriesCommand_h

#include "cmCommand.h"

/** \class cmLinkDirectoriesCommand
 * \brief Define a list of directories containing files to link.
 *
 * cmLinkDirectoriesCommand is used to specify a list
 * of directories containing files to link into executable(s). 
 * Note that the command supports the use of CMake built-in variables 
 * such as CMAKE_BINARY_DIR and CMAKE_SOURCE_DIR.
 */
class cmLinkDirectoriesCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmLinkDirectoriesCommand;
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
  virtual const char* GetName() { return "link_directories";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Specify directories in which the linker will look for libraries.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  link_directories(directory1 directory2 ...)\n"
      "Specify the paths in which the linker should search for libraries. "
      "The command will apply only to targets created after it is called.";
    }
  
  cmTypeMacro(cmLinkDirectoriesCommand, cmCommand);
};



#endif
