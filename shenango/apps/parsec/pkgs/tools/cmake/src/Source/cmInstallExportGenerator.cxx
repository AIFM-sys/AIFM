/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmInstallExportGenerator.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmInstallExportGenerator.h"

#include <stdio.h>

#include "cmake.h"
#include "cmInstallTargetGenerator.h"
#include "cmGeneratedFileStream.h"
#include "cmTarget.h"
#include "cmMakefile.h"
#include "cmLocalGenerator.h"
#include "cmGlobalGenerator.h"

#include "cmInstallFilesGenerator.h"

#include "cmExportInstallFileGenerator.h"

//----------------------------------------------------------------------------
cmInstallExportGenerator::cmInstallExportGenerator(
  const char* name,
  const char* destination,
  const char* file_permissions,
  std::vector<std::string> const& configurations,
  const char* component,
  const char* filename, const char* name_space,
  cmMakefile* mf)
  :cmInstallGenerator(destination, configurations, component)
  ,Name(name)
  ,FilePermissions(file_permissions)
  ,FileName(filename)
  ,Namespace(name_space)
  ,Makefile(mf)
{
  this->EFGen = new cmExportInstallFileGenerator(this);
}

//----------------------------------------------------------------------------
cmInstallExportGenerator::~cmInstallExportGenerator()
{
  delete this->EFGen;
}

//----------------------------------------------------------------------------
void cmInstallExportGenerator::ComputeTempDir()
{
  // Choose a temporary directory in which to generate the import
  // files to be installed.
  this->TempDir = this->Makefile->GetCurrentOutputDirectory();
  this->TempDir += cmake::GetCMakeFilesDirectory();
  this->TempDir += "/Export";
  if(this->Destination.empty())
    {
    return;
    }
  else
    {
    this->TempDir += "/";
    }

  // Enforce a maximum length.
  bool useMD5 = false;
#if defined(_WIN32) || defined(__CYGWIN__)
  std::string::size_type const max_total_len = 250;
#else
  std::string::size_type const max_total_len = 1000;
#endif
  if(this->TempDir.size() < max_total_len)
    {
    // Keep the total path length below the limit.
    std::string::size_type max_len = max_total_len - this->TempDir.size();
    if(this->Destination.size() > max_len)
      {
      useMD5 = true;
      }
    }
  else
    {
    useMD5 = true;
    }
  if(useMD5)
    {
    // Replace the destination path with a hash to keep it short.
    this->TempDir +=
      cmSystemTools::ComputeStringMD5(this->Destination.c_str());
    }
  else
    {
    std::string dest = this->Destination;
    // Avoid unix full paths.
    if(dest[0] == '/')
      {
      dest[0] = '_';
      }
    // Avoid windows full paths by removing colons.
    cmSystemTools::ReplaceString(dest, ":", "_");
    // Avoid relative paths that go up the tree.
    cmSystemTools::ReplaceString(dest, "../", "__/");
    // Avoid spaces.
    cmSystemTools::ReplaceString(dest, " ", "_");
    this->TempDir += dest;
    }
}

//----------------------------------------------------------------------------
void cmInstallExportGenerator::GenerateScript(std::ostream& os)
{
  // Get the export set requested.
  ExportSet const* exportSet =
    this->Makefile->GetLocalGenerator()->GetGlobalGenerator()
    ->GetExportSet(this->Name.c_str());

  // Skip empty sets.
  if(!exportSet)
    {
    cmOStringStream e;
    e << "INSTALL(EXPORT) given unknown export \"" << this->Name << "\"";
    cmSystemTools::Error(e.str().c_str());
    return;
    }

  // Create the temporary directory in which to store the files.
  this->ComputeTempDir();
  cmSystemTools::MakeDirectory(this->TempDir.c_str());

  // Construct a temporary location for the file.
  this->MainImportFile = this->TempDir;
  this->MainImportFile += "/";
  this->MainImportFile += this->FileName;

  // Generate the import file for this export set.
  this->EFGen->SetName(this->Name.c_str());
  this->EFGen->SetExportSet(exportSet);
  this->EFGen->SetExportFile(this->MainImportFile.c_str());
  this->EFGen->SetNamespace(this->Namespace.c_str());
  if(this->ConfigurationTypes->empty())
    {
    if(this->ConfigurationName && *this->ConfigurationName)
      {
      this->EFGen->AddConfiguration(this->ConfigurationName);
      }
    else
      {
      this->EFGen->AddConfiguration("");
      }
    }
  else
    {
    for(std::vector<std::string>::const_iterator
          ci = this->ConfigurationTypes->begin();
        ci != this->ConfigurationTypes->end(); ++ci)
      {
      this->EFGen->AddConfiguration(ci->c_str());
      }
    }
  this->EFGen->GenerateImportFile();

  // Perform the main install script generation.
  this->cmInstallGenerator::GenerateScript(os);
}

//----------------------------------------------------------------------------
void
cmInstallExportGenerator::GenerateScriptConfigs(std::ostream& os,
                                                Indent const& indent)
{
  // Create the main install rules first.
  this->cmInstallGenerator::GenerateScriptConfigs(os, indent);

  // Now create a configuration-specific install rule for the import
  // file of each configuration.
  std::vector<std::string> files;
  for(std::map<cmStdString, cmStdString>::const_iterator
        i = this->EFGen->GetConfigImportFiles().begin();
      i != this->EFGen->GetConfigImportFiles().end(); ++i)
    {
    files.push_back(i->second);
    std::string config_test = this->CreateConfigTest(i->first.c_str());
    os << indent << "IF(" << config_test << ")\n";
    this->AddInstallRule(os, cmTarget::INSTALL_FILES, files, false, 0,
                         this->FilePermissions.c_str(), 0, 0, 0,
                         indent.Next());
    os << indent << "ENDIF(" << config_test << ")\n";
    files.clear();
    }
}

//----------------------------------------------------------------------------
void cmInstallExportGenerator::GenerateScriptActions(std::ostream& os,
                                                     Indent const& indent)
{
  // Install the main export file.
  std::vector<std::string> files;
  files.push_back(this->MainImportFile);
  this->AddInstallRule(os, cmTarget::INSTALL_FILES, files, false, 0,
                       this->FilePermissions.c_str(), 0, 0, 0, indent);
}
