/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmData.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmData_h
#define cmData_h

#include "cmStandardIncludes.h"

/** \class cmData
 * \brief Hold extra data on a cmMakefile instance for a command.
 *
 * When CMake commands need to store extra information in a cmMakefile
 * instance, but the information is not needed by the makefile generators,
 * it can be held in a subclass of cmData.  The cmMakefile class has a map
 * from std::string to cmData*.  On its destruction, it destroys all the
 * extra data through the virtual destructor of cmData.
 */
class cmData
{
public:
  cmData(const char* name): Name(name) {}
  virtual ~cmData() {}
  
  const std::string& GetName() const
    { return this->Name; }
protected:
  std::string Name;
};

#endif
