/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmFunctionBlocker.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmFunctionBlocker_h
#define cmFunctionBlocker_h

#include "cmStandardIncludes.h"
#include "cmExecutionStatus.h"
class cmMakefile;

/** \class cmFunctionBlocker
 * \brief A class that defines an interface for blocking cmake functions
 *
 * This is the superclass for any classes that need to block a cmake function
 */
class cmFunctionBlocker
{
public:
  /**
   * should a function be blocked
   */
  virtual bool IsFunctionBlocked(const cmListFileFunction& lff,
                                 cmMakefile&mf,
                                 cmExecutionStatus &status) = 0;

  /**
   * should this function blocker be removed, useful when one function adds a
   * blocker and another must remove it 
   */
  virtual bool ShouldRemove(const cmListFileFunction&,
                            cmMakefile&) {return false;}

  /**
   * When the end of a CMakeList file is reached this method is called.  It
   * is not called on the end of an INCLUDE cmake file, just at the end of a
   * regular CMakeList file 
   */
  virtual void ScopeEnded(cmMakefile&) {}

  virtual ~cmFunctionBlocker() {}
};

#endif
