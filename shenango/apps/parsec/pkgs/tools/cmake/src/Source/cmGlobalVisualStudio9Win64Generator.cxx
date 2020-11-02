/*=========================================================================

Program:   CMake - Cross-Platform Makefile Generator
Module:    $RCSfile: cmGlobalVisualStudio9Win64Generator.cxx,v $
Language:  C++
Date:      $Date: 2012/03/29 17:21:08 $
Version:   $Revision: 1.1.1.1 $

Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even 
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmGlobalVisualStudio9Win64Generator.h"
#include "cmLocalVisualStudio7Generator.h"
#include "cmMakefile.h"


cmGlobalVisualStudio9Win64Generator::cmGlobalVisualStudio9Win64Generator()
{
  this->PlatformName = "x64";
}

///! Create a local generator appropriate to this Global Generator
cmLocalGenerator *cmGlobalVisualStudio9Win64Generator::CreateLocalGenerator()
{
  cmLocalVisualStudio7Generator *lg = new cmLocalVisualStudio7Generator;
  lg->SetVersion9();
  lg->SetPlatformName(this->PlatformName.c_str());
  lg->SetExtraFlagTable(this->GetExtraFlagTableVS8());
  lg->SetGlobalGenerator(this);
  return lg;
}

//----------------------------------------------------------------------------
void cmGlobalVisualStudio9Win64Generator
::GetDocumentation(cmDocumentationEntry& entry) const
{
  entry.Name = this->GetName();
  entry.Brief = "Generates Visual Studio 9 2008 Win64 project files.";
  entry.Full = "";
}

void cmGlobalVisualStudio9Win64Generator
::EnableLanguage(std::vector<std::string>const &  lang, 
                 cmMakefile *mf, bool optional)
{
  mf->AddDefinition("CMAKE_FORCE_WIN64", "TRUE");
  cmGlobalVisualStudio9Generator::EnableLanguage(lang, mf, optional);
}
