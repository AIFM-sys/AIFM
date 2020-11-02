/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmSubdirDependsCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmSubdirDependsCommand_h
#define cmSubdirDependsCommand_h

#include "cmCommand.h"

/** \class cmSubdirDependsCommand
 * \brief Legacy command.  Do not use.
 *
 * cmSubdirDependsCommand has been left in CMake for compatability with
 * projects already using it.  Its functionality in supporting parallel
 * builds is now automatic.  The command does not do anything.
 */
class cmSubdirDependsCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmSubdirDependsCommand;
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
  virtual const char* GetName() { return "subdir_depends";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Deprecated.  Does nothing.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  subdir_depends(subdir dep1 dep2 ...)\n"
      "Does not do anything.  This command used to help projects order "
      "parallel builds correctly.  This functionality is now automatic.";
    }
  
  /** This command is kept for compatibility with older CMake versions. */
  virtual bool IsDiscouraged()
    {
    return true;
    }

  cmTypeMacro(cmSubdirDependsCommand, cmCommand);
};



#endif
