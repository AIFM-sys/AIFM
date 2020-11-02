/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmInstallDirectoryGenerator.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmInstallDirectoryGenerator_h
#define cmInstallDirectoryGenerator_h

#include "cmInstallGenerator.h"

/** \class cmInstallDirectoryGenerator
 * \brief Generate directory installation rules.
 */
class cmInstallDirectoryGenerator: public cmInstallGenerator
{
public:
  cmInstallDirectoryGenerator(std::vector<std::string> const& dirs,
                              const char* dest,
                              const char* file_permissions,
                              const char* dir_permissions,
                              std::vector<std::string> const& configurations,
                              const char* component,
                              const char* literal_args);
  virtual ~cmInstallDirectoryGenerator();

protected:
  typedef cmInstallGeneratorIndent Indent;
  virtual void GenerateScriptActions(std::ostream& os, Indent const& indent);
  std::vector<std::string> Directories;
  std::string FilePermissions;
  std::string DirPermissions;
  std::string LiteralArguments;
};

#endif
