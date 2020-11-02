/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmDocumentationFormatterMan.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef _cmDocumentationFormatterMan_h
#define _cmDocumentationFormatterMan_h

#include "cmStandardIncludes.h"

#include "cmDocumentationFormatter.h"

/** Class to print the documentation as man page.  */
class cmDocumentationFormatterMan : public cmDocumentationFormatter
{
public:
  cmDocumentationFormatterMan();

  virtual cmDocumentationEnums::Form GetForm() const
                                      { return cmDocumentationEnums::ManForm;}

  virtual void PrintHeader(const char* name, std::ostream& os);
  virtual void PrintSection(std::ostream& os,
                    const cmDocumentationSection& section,
                    const char* name);
  virtual void PrintPreformatted(std::ostream& os, const char* text);
  virtual void PrintParagraph(std::ostream& os, const char* text);
};

#endif
