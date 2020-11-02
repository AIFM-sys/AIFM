/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmInstallDirectoryGenerator.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmInstallDirectoryGenerator.h"

#include "cmTarget.h"

//----------------------------------------------------------------------------
cmInstallDirectoryGenerator
::cmInstallDirectoryGenerator(std::vector<std::string> const& dirs,
                              const char* dest,
                              const char* file_permissions,
                              const char* dir_permissions,
                              std::vector<std::string> const& configurations,
                              const char* component,
                              const char* literal_args):
  cmInstallGenerator(dest, configurations, component), Directories(dirs),
  FilePermissions(file_permissions), DirPermissions(dir_permissions),
  LiteralArguments(literal_args)
{
}

//----------------------------------------------------------------------------
cmInstallDirectoryGenerator
::~cmInstallDirectoryGenerator()
{
}

//----------------------------------------------------------------------------
void
cmInstallDirectoryGenerator::GenerateScriptActions(std::ostream& os,
                                                   Indent const& indent)
{
  // Write code to install the directories.
  bool not_optional = false;
  const char* no_properties = 0;
  const char* no_rename = 0;
  this->AddInstallRule(os, cmTarget::INSTALL_DIRECTORY,
                       this->Directories,
                       not_optional, no_properties,
                       this->FilePermissions.c_str(),
                       this->DirPermissions.c_str(),
                       no_rename, this->LiteralArguments.c_str(),
                       indent);
}
