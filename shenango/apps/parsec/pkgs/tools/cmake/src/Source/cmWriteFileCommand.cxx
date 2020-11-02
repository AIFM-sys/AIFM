/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmWriteFileCommand.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:07 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmWriteFileCommand.h"

#include <sys/types.h>
#include <sys/stat.h>

// cmLibraryCommand
bool cmWriteFileCommand
::InitialPass(std::vector<std::string> const& args, cmExecutionStatus &)
{
  if(args.size() < 2 )
    {
    this->SetError("called with incorrect number of arguments");
    return false;
    }
  std::string message;
  std::vector<std::string>::const_iterator i = args.begin();

  std::string fileName = *i;
  bool overwrite = true;
  i++;

  for(;i != args.end(); ++i)
    {
    if ( *i == "APPEND" )
      {
      overwrite = false;
      }
    else
      {
      message += *i;
      }
    }

  if ( !this->Makefile->CanIWriteThisFile(fileName.c_str()) )
    {
    std::string e = "attempted to write a file: " + fileName
      + " into a source directory.";
    this->SetError(e.c_str());
    cmSystemTools::SetFatalErrorOccured();
    return false;
    }

  std::string dir = cmSystemTools::GetFilenamePath(fileName);
  cmSystemTools::MakeDirectory(dir.c_str());

  mode_t mode =
#if defined( _MSC_VER ) || defined( __MINGW32__ )
    S_IREAD | S_IWRITE
#elif defined( __BORLANDC__ )
    S_IRUSR | S_IWUSR
#else
    S_IRUSR | S_IWUSR |
    S_IRGRP |
    S_IROTH
#endif
    ;

  // Set permissions to writable
  if ( cmSystemTools::GetPermissions(fileName.c_str(), mode) )
    {
    cmSystemTools::SetPermissions(fileName.c_str(),
#if defined( _MSC_VER ) || defined( __MINGW32__ )
      S_IREAD | S_IWRITE
#else
      S_IRUSR | S_IWUSR
#endif
    );
    }
  // If GetPermissions fails, pretend like it is ok. File open will fail if
  // the file is not writable
  std::ofstream file(fileName.c_str(), 
                     overwrite?std::ios::out : std::ios::app);
  if ( !file )
    {
    std::string error = "Internal CMake error when trying to open file: ";
    error += fileName.c_str();
    error += " for writing.";
    this->SetError(error.c_str());
    return false;
    }
  file << message << std::endl;
  file.close();
  cmSystemTools::SetPermissions(fileName.c_str(), mode);

  return true;
}

