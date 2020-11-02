/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmGlobalVisualStudio8Win64Generator.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmGlobalVisualStudio8Win64Generator_h
#define cmGlobalVisualStudio8Win64Generator_h

#include "cmGlobalVisualStudio8Generator.h"


/** \class cmGlobalVisualStudio8Win64Generator
 * \brief Write a Unix makefiles.
 *
 * cmGlobalVisualStudio8Win64Generator manages UNIX build process for a tree
 */
class cmGlobalVisualStudio8Win64Generator : 
  public cmGlobalVisualStudio8Generator
{
public:
  cmGlobalVisualStudio8Win64Generator();
  static cmGlobalGenerator* New() { 
    return new cmGlobalVisualStudio8Win64Generator; }
  
  ///! Get the name for the generator.
  virtual const char* GetName() const {
    return cmGlobalVisualStudio8Win64Generator::GetActualName();}
  static const char* GetActualName() {return "Visual Studio 8 2005 Win64";}

  /** Get the documentation entry for this generator.  */
  virtual void GetDocumentation(cmDocumentationEntry& entry) const;

  ///! create the correct local generator
  virtual cmLocalGenerator *CreateLocalGenerator();

  /**
   * Try to determine system infomation such as shared library
   * extension, pthreads, byte order etc.  
   */
  virtual void EnableLanguage(std::vector<std::string>const& languages, 
                              cmMakefile *, bool optional);
};
#endif
