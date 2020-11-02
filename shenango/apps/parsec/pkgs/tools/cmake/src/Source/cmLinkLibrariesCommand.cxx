/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmLinkLibrariesCommand.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmLinkLibrariesCommand.h"

// cmLinkLibrariesCommand
bool cmLinkLibrariesCommand
::InitialPass(std::vector<std::string> const& args, cmExecutionStatus &)
{
  if(args.size() < 1 )
    {
    return true;
    }
  // add libraries, nothe that there is an optional prefix 
  // of debug and optimized than can be used
  for(std::vector<std::string>::const_iterator i = args.begin();
      i != args.end(); ++i)
    {
    if (*i == "debug")
      {
      ++i;
      if(i == args.end())
        {
        this->SetError("The \"debug\" argument must be followed by "
                       "a library");
        return false;
        }
      this->Makefile->AddLinkLibrary(i->c_str(),
                                 cmTarget::DEBUG);
      }
    else if (*i == "optimized")
      {
      ++i;
      if(i == args.end())
        {
        this->SetError("The \"optimized\" argument must be followed by "
                       "a library");
        return false;
        }
      this->Makefile->AddLinkLibrary(i->c_str(),
                                 cmTarget::OPTIMIZED);
      }
    else
      {
      this->Makefile->AddLinkLibrary(i->c_str());  
      }
    }
  
  return true;
}

