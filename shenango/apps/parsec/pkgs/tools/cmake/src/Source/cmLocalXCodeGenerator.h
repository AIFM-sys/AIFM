/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmLocalXCodeGenerator.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmLocalXCodeGenerator_h
#define cmLocalXCodeGenerator_h

#include "cmLocalGenerator.h"

/** \class cmLocalXCodeGenerator
 * \brief Write a local Xcode project
 *
 * cmLocalXCodeGenerator produces a LocalUnix makefile from its
 * member Makefile.
 */
class cmLocalXCodeGenerator : public cmLocalGenerator
{
public:
  ///! Set cache only and recurse to false by default.
  cmLocalXCodeGenerator();

  virtual ~cmLocalXCodeGenerator();
  void GetTargetObjectFileDirectories(cmTarget* target,
                                      std::vector<std::string>& 
                                      dirs);
  virtual std::string GetTargetDirectory(cmTarget const& target) const;
private:

};

#endif

