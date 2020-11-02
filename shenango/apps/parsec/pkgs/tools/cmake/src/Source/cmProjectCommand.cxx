/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmProjectCommand.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:07 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmProjectCommand.h"

// cmProjectCommand
bool cmProjectCommand
::InitialPass(std::vector<std::string> const& args, cmExecutionStatus &)
{
  if(args.size() < 1 )
    {
    this->SetError("PROJECT called with incorrect number of arguments");
    return false;
    } 
  this->Makefile->SetProjectName(args[0].c_str());

  std::string bindir = args[0];
  bindir += "_BINARY_DIR";
  std::string srcdir = args[0];
  srcdir += "_SOURCE_DIR";
  
  this->Makefile->AddCacheDefinition
    (bindir.c_str(),
     this->Makefile->GetCurrentOutputDirectory(),
     "Value Computed by CMake", cmCacheManager::STATIC);
  this->Makefile->AddCacheDefinition
    (srcdir.c_str(),
     this->Makefile->GetCurrentDirectory(),
     "Value Computed by CMake", cmCacheManager::STATIC);
  
  bindir = "PROJECT_BINARY_DIR";
  srcdir = "PROJECT_SOURCE_DIR";

  this->Makefile->AddDefinition(bindir.c_str(),
          this->Makefile->GetCurrentOutputDirectory());
  this->Makefile->AddDefinition(srcdir.c_str(),
          this->Makefile->GetCurrentDirectory());

  this->Makefile->AddDefinition("PROJECT_NAME", args[0].c_str());

  // Set the CMAKE_PROJECT_NAME variable to be the highest-level
  // project name in the tree.  This is always the first PROJECT
  // command encountered.
  if(!this->Makefile->GetDefinition("CMAKE_PROJECT_NAME"))
    {
    this->Makefile->AddDefinition("CMAKE_PROJECT_NAME", args[0].c_str());
    }

  std::vector<std::string> languages;
  if(args.size() > 1)
    {
    for(size_t i =1; i < args.size(); ++i)
      {
      languages.push_back(args[i]);
      }
    }
  else
    {
    // if no language is specified do c and c++
    languages.push_back("C");
    languages.push_back("CXX");
    }
  this->Makefile->EnableLanguage(languages, false);
  return true;
}

