/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmTargetLinkLibrariesCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmTargetLinkLibrariesCommand_h
#define cmTargetLinkLibrariesCommand_h

#include "cmCommand.h"

/** \class cmTargetLinkLibrariesCommand
 * \brief Specify a list of libraries to link into executables.
 *
 * cmTargetLinkLibrariesCommand is used to specify a list of libraries to link
 * into executable(s) or shared objects. The names of the libraries
 * should be those defined by the LIBRARY(library) command(s).  
 */
class cmTargetLinkLibrariesCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmTargetLinkLibrariesCommand;
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
  virtual const char* GetName() { return "target_link_libraries";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return 
      "Link a target to given libraries.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  target_link_libraries(target library1\n"
      "                        <debug | optimized | general> library2\n"
      "                        ...)\n"
      "Specify a list of libraries to be linked into the specified target.  "
      "The debug and optimized strings may be used to indicate that "
      "the next library listed is to be used only for that specific "
      "type of build. general indicates it is used for all build types "
      "and is assumed if not specified.\n"
      "If any library name matches that of a target in the current project "
      "a dependency will automatically be added in the build system to make "
      "sure the library being linked is up-to-date before the target links.";
    }
  
  cmTypeMacro(cmTargetLinkLibrariesCommand, cmCommand);
private:
};



#endif
