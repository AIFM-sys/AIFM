/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmMakefileUtilityTargetGenerator.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmMakefileUtilityTargetGenerator_h
#define cmMakefileUtilityTargetGenerator_h

#include "cmMakefileTargetGenerator.h"

class cmMakefileUtilityTargetGenerator: 
  public cmMakefileTargetGenerator
{
public:
  cmMakefileUtilityTargetGenerator(cmTarget* target);

  /* the main entry point for this class. Writes the Makefiles associated
     with this target */
  virtual void WriteRuleFiles();  
  
protected:

};

#endif
