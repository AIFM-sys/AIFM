/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmMarkAsAdvancedCommand.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmMarkAsAdvancedCommand.h"

// cmMarkAsAdvancedCommand
bool cmMarkAsAdvancedCommand
::InitialPass(std::vector<std::string> const& args, cmExecutionStatus &)
{
  if(args.size() < 1 )
    {
    this->SetError("called with incorrect number of arguments");
    return false;
    }

  unsigned int i =0;
  const char* value = "1";
  bool overwrite = false;
  if(args[0] == "CLEAR" || args[0] == "FORCE")
    {
    overwrite = true;
    if(args[0] == "CLEAR")
      {
      value = "0";
      }
    i = 1;
    }
  for(; i < args.size(); ++i)
    {
    std::string variable = args[i];
    cmCacheManager* manager = this->Makefile->GetCacheManager();
    cmCacheManager::CacheIterator it = 
      manager->GetCacheIterator(variable.c_str());
    if ( it.IsAtEnd() )
      {
      this->Makefile->GetCacheManager()
        ->AddCacheEntry(variable.c_str(), 0, 0,
          cmCacheManager::UNINITIALIZED);
      overwrite = true;
      }
    it.Find(variable.c_str());
    if ( it.IsAtEnd() )
      {
      cmSystemTools::Error("This should never happen...");
      return false;
      }
    if ( !it.PropertyExists("ADVANCED") || overwrite )
      {
      it.SetProperty("ADVANCED", value);
      }
    }
  return true;
}

