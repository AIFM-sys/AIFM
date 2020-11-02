/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCPackCygwinBinaryGenerator.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include "cmCPackCygwinBinaryGenerator.h"

#include "cmake.h"
#include "cmGlobalGenerator.h"
#include "cmLocalGenerator.h"
#include "cmSystemTools.h"
#include "cmMakefile.h"
#include "cmGeneratedFileStream.h"
#include "cmCPackLog.h"

#include <cmsys/SystemTools.hxx>

//----------------------------------------------------------------------
cmCPackCygwinBinaryGenerator::cmCPackCygwinBinaryGenerator()
{
  this->Compress = false;
}

//----------------------------------------------------------------------
cmCPackCygwinBinaryGenerator::~cmCPackCygwinBinaryGenerator()
{
}

//----------------------------------------------------------------------
int cmCPackCygwinBinaryGenerator::InitializeInternal()
{
  this->SetOptionIfNotSet("CPACK_PACKAGING_INSTALL_PREFIX", "/usr");
  this->SetOptionIfNotSet("CPACK_INCLUDE_TOPLEVEL_DIRECTORY", "0");
  std::vector<std::string> path;
  std::string pkgPath = cmSystemTools::FindProgram("bzip2", path, false);
  if ( pkgPath.empty() )
    {
    cmCPackLogger(cmCPackLog::LOG_ERROR, "Cannot find BZip2" << std::endl);
    return 0;
    }
  this->SetOptionIfNotSet("CPACK_INSTALLER_PROGRAM", pkgPath.c_str());
  cmCPackLogger(cmCPackLog::LOG_VERBOSE, "Found Compress program: "
    << pkgPath.c_str()
    << std::endl);

  return this->Superclass::InitializeInternal();
}

//----------------------------------------------------------------------
int cmCPackCygwinBinaryGenerator::CompressFiles(const char* outFileName,
  const char* toplevel, const std::vector<std::string>& files)
{
  std::string packageName = this->GetOption("CPACK_PACKAGE_NAME");
  packageName += "-";
  packageName += this->GetOption("CPACK_PACKAGE_VERSION");
  packageName = cmsys::SystemTools::LowerCase(packageName);
  std::string manifest = "/usr/share/doc/";
  manifest += packageName;
  manifest += "/MANIFEST";
  std::string manifestFile 
    = this->GetOption("CPACK_TEMPORARY_DIRECTORY");
  // Create a MANIFEST file that contains all of the files in
  // the tar file
  std::string tempdir = manifestFile;
  manifestFile += manifest;
  // create an extra scope to force the stream
  // to create the file before the super class is called
  {
  cmGeneratedFileStream ofs(manifestFile.c_str());
  for(std::vector<std::string>::const_iterator i = files.begin();
      i != files.end(); ++i)
    {
    // remove the temp dir and replace with /usr
    ofs << (*i).substr(tempdir.size()) << "\n";
    }
  ofs << manifest << "\n";
  }
  // add the manifest file to the list of all files
  std::vector<std::string> filesWithManifest = files;
  filesWithManifest.push_back(manifestFile);
  // create the bzip2 tar file 
  return this->Superclass::CompressFiles(outFileName, toplevel, 
                                         filesWithManifest);
}

const char* cmCPackCygwinBinaryGenerator::GetOutputExtension()
{
  this->OutputExtension = "-";
  const char* patchNumber =this->GetOption("CPACK_CYGWIN_PATCH_NUMBER");
  if(!patchNumber)
    {
    patchNumber = "1";  
    cmCPackLogger(cmCPackLog::LOG_WARNING, 
                  "CPACK_CYGWIN_PATCH_NUMBER not specified using 1"
                  << std::endl);
    }
  this->OutputExtension += patchNumber;
  this->OutputExtension += ".tar.bz2";
  return this->OutputExtension.c_str();
}
