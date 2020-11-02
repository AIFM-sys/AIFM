/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmMakefileExecutableTargetGenerator.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:07 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmMakefileExecutableTargetGenerator_h
#define cmMakefileExecutableTargetGenerator_h

#include "cmMakefileTargetGenerator.h"

class cmMakefileExecutableTargetGenerator: public cmMakefileTargetGenerator
{
public:
  cmMakefileExecutableTargetGenerator(cmTarget* target);

  /* the main entry point for this class. Writes the Makefiles associated
     with this target */
  virtual void WriteRuleFiles();
  
protected:
  virtual void WriteExecutableRule(bool relink);
  void CreateAppBundle(std::string& targetName, std::string& outpath);
};

#endif
