/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmPropertyDefinition.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmPropertyDefinition_h
#define cmPropertyDefinition_h

#include "cmProperty.h"

class cmPropertyDefinition 
{
public:
  // Define this property
  void DefineProperty(const char *name, cmProperty::ScopeType scope,
                      const char *ShortDescription,
                      const char *FullDescription, 
                      const char *DocumentationSection,
                      bool chained);

  // get the documentation string
  cmDocumentationEntry GetDocumentation() const;

  // basic constructor 
  cmPropertyDefinition() { this->Chained = false; };

  // is it chained?
  bool IsChained() {return this->Chained; };

  // Get the section if any
  const std::string &GetDocumentationSection() const {
    return this->DocumentationSection; }; 
  
  // get the scope
  cmProperty::ScopeType GetScope() const {
    return this->Scope; };

  // get the docs
  const std::string &GetShortDescription() const {
    return this->ShortDescription; }; 
  const std::string &GetFullDescription() const {
    return this->FullDescription; }; 
  
protected:
  std::string Name;
  std::string ShortDescription;
  std::string FullDescription;
  std::string DocumentationSection;
  cmProperty::ScopeType Scope; 
  bool Chained;
};

#endif
