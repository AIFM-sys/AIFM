/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmListFileCache.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmListFileCache_h
#define cmListFileCache_h

#include "cmStandardIncludes.h"

/** \class cmListFileCache
 * \brief A class to cache list file contents.
 *
 * cmListFileCache is a class used to cache the contents of parsed
 * cmake list files.
 */

class cmMakefile;
 
struct cmListFileArgument
{
  cmListFileArgument(): Value(), Quoted(false), FilePath(0), Line(0) {}
  cmListFileArgument(const cmListFileArgument& r):
    Value(r.Value), Quoted(r.Quoted), FilePath(r.FilePath), Line(r.Line) {}
  cmListFileArgument(const std::string& v, bool q, const char* file,
                     long line): Value(v), Quoted(q),
                                 FilePath(file), Line(line) {}
  bool operator == (const cmListFileArgument& r) const
    {
    return (this->Value == r.Value) && (this->Quoted == r.Quoted);
    }
  bool operator != (const cmListFileArgument& r) const
    {
    return !(*this == r);
    }
  std::string Value;
  bool Quoted;
  const char* FilePath;
  long Line;
};

struct cmListFileContext
{
  std::string Name;
  std::string FilePath;
  long Line;
};

std::ostream& operator<<(std::ostream&, cmListFileContext const&);

struct cmListFileFunction: public cmListFileContext
{
  std::vector<cmListFileArgument> Arguments;
};

class cmListFileBacktrace: public std::vector<cmListFileContext> {};

struct cmListFile
{
  cmListFile() 
    :ModifiedTime(0) 
    {
    }
  bool ParseFile(const char* path, 
                 bool topLevel,
                 cmMakefile *mf);

  long int ModifiedTime;
  std::vector<cmListFileFunction> Functions;
};

#endif
