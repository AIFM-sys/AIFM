/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmPropertyMap.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmPropertyMap_h
#define cmPropertyMap_h

#include "cmProperty.h"

class cmake;

class cmPropertyMap : public std::map<cmStdString,cmProperty>
{
public:
  cmProperty *GetOrCreateProperty(const char *name);

  void SetProperty(const char *name, const char *value, 
                   cmProperty::ScopeType scope);

  void AppendProperty(const char* name, const char* value,
                      cmProperty::ScopeType scope);

  const char *GetPropertyValue(const char *name, 
                               cmProperty::ScopeType scope,
                               bool &chain) const;

  void SetCMakeInstance(cmake *cm) { this->CMakeInstance = cm; };

  cmPropertyMap() { this->CMakeInstance = 0;};

private:
  cmake *CMakeInstance;
};

#endif

