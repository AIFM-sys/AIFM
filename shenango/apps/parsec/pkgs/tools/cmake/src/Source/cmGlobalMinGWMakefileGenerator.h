/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmGlobalMinGWMakefileGenerator.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmGlobalMinGWMakefileGenerator_h
#define cmGlobalMinGWMakefileGenerator_h

#include "cmGlobalUnixMakefileGenerator3.h"

/** \class cmGlobalMinGWMakefileGenerator
 * \brief Write a NMake makefiles.
 *
 * cmGlobalMinGWMakefileGenerator manages nmake build process for a tree
 */
class cmGlobalMinGWMakefileGenerator : public cmGlobalUnixMakefileGenerator3
{
public:
  cmGlobalMinGWMakefileGenerator();
  static cmGlobalGenerator* New() { 
    return new cmGlobalMinGWMakefileGenerator; }
  ///! Get the name for the generator.
  virtual const char* GetName() const {
    return cmGlobalMinGWMakefileGenerator::GetActualName();}
  static const char* GetActualName() {return "MinGW Makefiles";}

  /** Get the documentation entry for this generator.  */
  virtual void GetDocumentation(cmDocumentationEntry& entry) const;
  
  ///! Create a local generator appropriate to this Global Generator
  virtual cmLocalGenerator *CreateLocalGenerator();

  /**
   * Try to determine system infomation such as shared library
   * extension, pthreads, byte order etc.  
   */
  virtual void EnableLanguage(std::vector<std::string>const& languages,
                              cmMakefile *, bool optional);
};

#endif
