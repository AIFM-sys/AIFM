/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmSourceFileLocation.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmSourceFileLocation_h
#define cmSourceFileLocation_h

#include "cmStandardIncludes.h"

class cmMakefile;

/** \class cmSourceFileLocation
 * \brief cmSourceFileLocation tracks knowledge about a source file location
 *
 * Source files can be referenced by a variety of names.  The
 * directory and/or extension may be omitted leading to a certain
 * level of ambiguity about the source file location.  This class is
 * used by cmSourceFile to keep track of what is known about the
 * source file location.  Each reference may add some information
 * about the directory or extension of the file.
 */
class cmSourceFileLocation
{
public:
  /**
   * Construct for a source file created in a given cmMakefile
   * instance with an initial name.
   */
  cmSourceFileLocation(cmMakefile* mf, const char* name);

  /**
   * Return whether the givne source file location could refers to the
   * same source file as this location given the level of ambiguity in
   * each location.
   */
  bool Matches(cmSourceFileLocation const& loc);

  /**
   * Explicity state that the source file is located in the source tree.
   */
  void DirectoryUseSource();

  /**
   * Explicity state that the source file is located in the build tree.
   */
  void DirectoryUseBinary();

  /**
   * Return whether the directory containing the source is ambiguous.
   */
  bool DirectoryIsAmbiguous() const { return this->AmbiguousDirectory; }

  /**
   * Return whether the extension of the source name is ambiguous.
   */
  bool ExtensionIsAmbiguous() const { return this->AmbiguousExtension; }

  /**
   * Get the directory containing the file as best is currently known.
   * If DirectoryIsAmbiguous() returns false this will be a full path.
   * Otherwise it will be a relative path (possibly empty) that is
   * either with respect to the source or build tree.
   */
  const char* GetDirectory() const { return this->Directory.c_str(); }

  /**
   * Get the file name as best is currently known.  If
   * ExtensionIsAmbiguous() returns true this name may not be the
   * final name (but could be).  Otherwise the returned name is the
   * final name.
   */
  const char* GetName() const { return this->Name.c_str(); }

  /**
   * Get the cmMakefile instance for which the source file was created.
   */
  cmMakefile* GetMakefile() const { return this->Makefile; }
private:
  cmMakefile* Makefile;
  bool AmbiguousDirectory;
  bool AmbiguousExtension;
  std::string Directory;
  std::string Name;

  // Update the location with additional knowledge.
  void Update(cmSourceFileLocation const& loc);
  void Update(const char* name);
  void UpdateExtension(const char* name);
  void UpdateDirectory(const char* name);
};

#endif
