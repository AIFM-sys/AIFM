/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCTestReadCustomFilesCommand.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmCTestReadCustomFilesCommand.h"

#include "cmCTest.h"

bool cmCTestReadCustomFilesCommand
::InitialPass(std::vector<std::string> const& args, cmExecutionStatus &)
{
  if (args.size() < 1)
    {
    this->SetError("called with incorrect number of arguments");
    return false;
    }

  std::vector<std::string>::const_iterator dit;
  for ( dit = args.begin(); dit != args.end(); ++ dit )
    {
    this->CTest->ReadCustomConfigurationFileTree(dit->c_str(),
      this->Makefile);
    }

  return true;
}


