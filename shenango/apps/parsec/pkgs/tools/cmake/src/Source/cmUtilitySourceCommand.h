/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmUtilitySourceCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmUtilitySourceCommand_h
#define cmUtilitySourceCommand_h

#include "cmCommand.h"

/** \class cmUtilitySourceCommand
 * \brief A command to setup a cache entry with the location of a third-party
 * utility's source.
 *
 * cmUtilitySourceCommand is used when a third-party utility's source is
 * included in the project's source tree.  It specifies the location of
 * the executable's source, and any files that may be needed to confirm the
 * identity of the source.
 */
class cmUtilitySourceCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmUtilitySourceCommand;
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
  virtual const char* GetName() { return "utility_source";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Specify the source tree of a third-party utility.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  utility_source(cache_entry executable_name\n"
      "                 path_to_source [file1 file2 ...])\n"
      "When a third-party utility's source is included in the distribution, "
      "this command specifies its location and name.  The cache entry will "
      "not be set unless the path_to_source and all listed files exist.  It "
      "is assumed that the source tree of the utility will have been built "
      "before it is needed.\n"
      "When cross compiling CMake will print a warning if a utility_source() "
      "command is executed, because in many cases it is used to build an "
      "executable which is executed later on. This doesn't work when "
      "cross compiling, since the executable can run only on their target "
      "platform. So in this case the cache entry has to be adjusted manually "
      "so it points to an executable which is runnable on the build host.";
    }

  /** This command is kept for compatibility with older CMake versions. */
  virtual bool IsDiscouraged()
    {
    return true;
    }


  cmTypeMacro(cmUtilitySourceCommand, cmCommand);
};



#endif
