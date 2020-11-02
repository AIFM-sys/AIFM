/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmGetTestPropertyCommand.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmGetTestPropertyCommand.h"

#include "cmake.h"
#include "cmTest.h"

// cmGetTestPropertyCommand
bool cmGetTestPropertyCommand
::InitialPass(std::vector<std::string> const& args, cmExecutionStatus &)
{
  if(args.size() < 3 )
    {
    this->SetError("called with incorrect number of arguments");
    return false;
    }

  std::string testName = args[0];
  std::string var = args[2];
  cmTest *test = this->Makefile->GetTest(testName.c_str());
  if (test)
    {
    const char *prop = test->GetProperty(args[1].c_str());
    if (prop)
      {
      this->Makefile->AddDefinition(var.c_str(), prop);
      return true;
      }
    }
  this->Makefile->AddDefinition(var.c_str(), "NOTFOUND");
  return true;
}

