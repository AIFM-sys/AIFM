/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmBuildCommand.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmBuildCommand.h"

#include "cmLocalGenerator.h"
#include "cmGlobalGenerator.h"

// cmBuildCommand
bool cmBuildCommand
::InitialPass(std::vector<std::string> const& args, cmExecutionStatus &)
{
  if(args.size() < 2 )
    {
    this->SetError("called with incorrect number of arguments");
    return false;
    }
  const char* define = args[0].c_str();
  const char* cacheValue
    = this->Makefile->GetDefinition(define);
  std::string makeprogram = args[1];
  std::string configType = "Release";
  const char* cfg = getenv("CMAKE_CONFIG_TYPE");
  if ( cfg )
    {
    configType = cfg;
    }
  std::string makecommand = this->Makefile->GetLocalGenerator()
    ->GetGlobalGenerator()->GenerateBuildCommand
    (makeprogram.c_str(), this->Makefile->GetProjectName(), 0,
     0, configType.c_str(), true, false);

  if(cacheValue)
    {
    return true;
    }
  this->Makefile->AddCacheDefinition(define,
                                 makecommand.c_str(),
                                 "Command used to build entire project "
                                 "from the command line.",
                                 cmCacheManager::STRING);
  return true;
}

