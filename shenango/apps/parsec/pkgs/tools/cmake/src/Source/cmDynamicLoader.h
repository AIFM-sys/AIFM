/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmDynamicLoader.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
// .NAME cmDynamicLoader - class interface to system dynamic libraries
// .SECTION Description
// cmDynamicLoader provides a portable interface to loading dynamic
// libraries into a process.


#ifndef __cmDynamicLoader_h
#define __cmDynamicLoader_h

#include "cmStandardIncludes.h"

#include <cmsys/DynamicLoader.hxx>

class cmDynamicLoader
{
public:
  // Description:
  // Load a dynamic library into the current process.
  // The returned cmsys::DynamicLoader::LibraryHandle can be used to access
  // the symbols in the library.
  static cmsys::DynamicLoader::LibraryHandle OpenLibrary(const char*);

  // Description:
  // Flush the cache of dynamic loader.
  static void FlushCache();

protected:
  cmDynamicLoader() {};
  ~cmDynamicLoader() {};

private:
  cmDynamicLoader(const cmDynamicLoader&);  // Not implemented.
  void operator=(const cmDynamicLoader&);  // Not implemented.
};

#endif
