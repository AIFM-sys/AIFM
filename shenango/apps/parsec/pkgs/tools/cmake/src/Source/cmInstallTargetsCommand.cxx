/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmInstallTargetsCommand.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:07 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmInstallTargetsCommand.h"

// cmExecutableCommand
bool cmInstallTargetsCommand
::InitialPass(std::vector<std::string> const& args, cmExecutionStatus &)
{
  if(args.size() < 2 )
    {
    this->SetError("called with incorrect number of arguments");
    return false;
    }

  // Enable the install target.
  this->Makefile->GetLocalGenerator()
    ->GetGlobalGenerator()->EnableInstallTarget();

  cmTargets &tgts = this->Makefile->GetTargets();
  std::vector<std::string>::const_iterator s = args.begin();
  ++s;
  std::string runtime_dir = "/bin";
  for (;s != args.end(); ++s)
    {
    if (*s == "RUNTIME_DIRECTORY")
      {
      ++s;
      if ( s == args.end() )
        {
        this->SetError("called with RUNTIME_DIRECTORY but no actual "
                       "directory");
        return false;
        }

      runtime_dir = *s;
      }
    else if (tgts.find(*s) != tgts.end())
      {
      tgts[*s].SetInstallPath(args[0].c_str());
      tgts[*s].SetRuntimeInstallPath(runtime_dir.c_str());
      tgts[*s].SetHaveInstallRule(true);
      }
    else
      {
      std::string str = "Cannot find target: \"" + *s + "\" to install.";
      this->SetError(str.c_str());
      return false;
      }
    }

  this->Makefile->GetLocalGenerator()->GetGlobalGenerator()
                       ->AddInstallComponent("Unspecified");

  return true;
}

