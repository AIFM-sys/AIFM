/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCTestBuildCommand.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmCTestBuildCommand.h"

#include "cmCTest.h"
#include "cmCTestGenericHandler.h"
#include "cmake.h"
#include "cmGlobalGenerator.h"


//----------------------------------------------------------------------------
cmCTestBuildCommand::cmCTestBuildCommand()
{
  this->GlobalGenerator = 0;
}

//----------------------------------------------------------------------------
cmCTestBuildCommand::~cmCTestBuildCommand()
{
  if ( this->GlobalGenerator )
    {
    delete this->GlobalGenerator;
    this->GlobalGenerator = 0;
    }
}

//----------------------------------------------------------------------------
cmCTestGenericHandler* cmCTestBuildCommand::InitializeHandler()
{
  cmCTestGenericHandler* handler
    = this->CTest->GetInitializedHandler("build");
  if ( !handler )
    {
    this->SetError("internal CTest error. Cannot instantiate build handler");
    return 0;
    }

  const char* ctestBuildCommand
    = this->Makefile->GetDefinition("CTEST_BUILD_COMMAND");
  if ( ctestBuildCommand && *ctestBuildCommand )
    {
    this->CTest->SetCTestConfiguration("MakeCommand", ctestBuildCommand);
    }
  else
    {
    const char* cmakeGeneratorName
      = this->Makefile->GetDefinition("CTEST_CMAKE_GENERATOR");
    const char* cmakeProjectName
      = this->Makefile->GetDefinition("CTEST_PROJECT_NAME");
    const char* cmakeBuildConfiguration
      = this->Makefile->GetDefinition("CTEST_BUILD_CONFIGURATION");
    const char* cmakeBuildAdditionalFlags
      = this->Makefile->GetDefinition("CTEST_BUILD_FLAGS");
    const char* cmakeBuildTarget
      = this->Makefile->GetDefinition("CTEST_BUILD_TARGET");
    if ( cmakeGeneratorName && *cmakeGeneratorName &&
      cmakeProjectName && *cmakeProjectName )
      {
      if ( !cmakeBuildConfiguration )
        {
        cmakeBuildConfiguration = "Release";
        }
      if ( this->GlobalGenerator )
        {
        if ( strcmp(this->GlobalGenerator->GetName(),
            cmakeGeneratorName) != 0 )
          {
          delete this->GlobalGenerator;
          this->GlobalGenerator = 0;
          }
        }
      if ( !this->GlobalGenerator )
        {
        this->GlobalGenerator =
          this->Makefile->GetCMakeInstance()->CreateGlobalGenerator(
            cmakeGeneratorName);
        }
      this->GlobalGenerator->FindMakeProgram(this->Makefile);
      const char* cmakeMakeProgram
        = this->Makefile->GetDefinition("CMAKE_MAKE_PROGRAM");
      if(strlen(cmakeBuildConfiguration) == 0)
        {
        const char* config = 0;
#ifdef CMAKE_INTDIR
        config = CMAKE_INTDIR;
#endif
        if(!config)
          {
          config = "Debug";
          }
        cmakeBuildConfiguration = config;
        }
      
      std::string buildCommand
        = this->GlobalGenerator->
        GenerateBuildCommand(cmakeMakeProgram,
                             cmakeProjectName,
                             cmakeBuildAdditionalFlags, cmakeBuildTarget,
                             cmakeBuildConfiguration, true, false);
      cmCTestLog(this->CTest, HANDLER_VERBOSE_OUTPUT,
                 "SetMakeCommand:"
                 << buildCommand.c_str() << "\n");
      this->CTest->SetCTestConfiguration("MakeCommand", buildCommand.c_str());
      }
    else
      {
      cmOStringStream ostr;
      ostr << "CTEST_BUILD_COMMAND or CTEST_CMAKE_GENERATOR not specified. "
        "Please specify the CTEST_CMAKE_GENERATOR and CTEST_PROJECT_NAME if "
        "this is a CMake project, or specify the CTEST_BUILD_COMMAND for "
        "cmake or any other project.";
      this->SetError(ostr.str().c_str());
      return 0;
      }
    }

  return handler;
}


