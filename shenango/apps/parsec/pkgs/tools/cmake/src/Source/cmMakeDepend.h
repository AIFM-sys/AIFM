/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmMakeDepend.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmMakeDepend_h
#define cmMakeDepend_h

#include "cmMakefile.h"
#include "cmSourceFile.h"

#include <cmsys/RegularExpression.hxx>

/** \class cmDependInformation
 * \brief Store dependency information for a single source file.
 *
 * This structure stores the depend information for a single source file.
 */
class cmDependInformation
{
public:
  /**
   * Construct with dependency generation marked not done; instance
   * not placed in cmMakefile's list.
   */
  cmDependInformation(): DependDone(false), SourceFile(0) {}

  /**
   * The set of files on which this one depends.
   */
  typedef std::set<cmDependInformation*> DependencySetType;
  DependencySetType DependencySet;

  /**
   * This flag indicates whether dependency checking has been
   * performed for this file.
   */
  bool DependDone;

  /**
   * If this object corresponds to a cmSourceFile instance, this points
   * to it.
   */
  const cmSourceFile *SourceFile;
  
  /**
   * Full path to this file.
   */
  std::string FullPath;
  
  /**
   * Full path not including file name.
   */
  std::string PathOnly;
  
  /**
   * Name used to #include this file.
   */
  std::string IncludeName;
  
  /**
   * This method adds the dependencies of another file to this one.
   */
  void AddDependencies(cmDependInformation*);  
};


// cmMakeDepend is used to generate dependancy information for
// the classes in a makefile
class cmMakeDepend
{
public:
  /**
   * Construct the object with verbose turned off.
   */
  cmMakeDepend();

  /**
   * Destructor.
   */
  virtual ~cmMakeDepend();
  
  /** 
   * Set the makefile that is used as a source of classes.
   */
  virtual void SetMakefile(cmMakefile* makefile); 

  /** 
   * Get the depend info struct for a source file
   */
  const cmDependInformation 
  *GetDependInformationForSourceFile(const cmSourceFile &sf) const;

  /**
   * Add a directory to the search path for include files.
   */
  virtual void AddSearchPath(const char*);

  /**
   * Generate dependencies for all the sources of all the targets
   * in the makefile.
   */
  void GenerateMakefileDependencies();

  /**
   * Generate dependencies for the file given.  Returns a pointer to
   * the cmDependInformation object for the file.
   */
  const cmDependInformation* FindDependencies(const char* file);

protected: 
  /**
   * Add a source file to the search path.
   */
  void AddFileToSearchPath(const char* filepath);

  /**
   * Compute the depend information for this class.
   */
  virtual void DependWalk(cmDependInformation* info);
  
  /**
   * Add a dependency.  Possibly walk it for more dependencies.
   */
  virtual void AddDependency(cmDependInformation* info, const char* file);
  
  /**
   * Fill in the given object with dependency information.  If the
   * information is already complete, nothing is done.
   */
  void GenerateDependInformation(cmDependInformation* info);
  
  /**
   * Get an instance of cmDependInformation corresponding to the given file
   * name.
   */
  cmDependInformation* GetDependInformation(const char* file, 
                                            const char *extraPath);  
  
  /** 
   * Find the full path name for the given file name.
   * This uses the include directories.
   * TODO: Cache path conversions to reduce FileExists calls.
   */
  std::string FullPath(const char *filename, const char *extraPath);

  cmMakefile* Makefile;
  bool Verbose;
  cmsys::RegularExpression IncludeFileRegularExpression;
  cmsys::RegularExpression ComplainFileRegularExpression;
  std::vector<std::string> IncludeDirectories;
  typedef std::map<cmStdString, cmStdString> FileToPathMapType;
  typedef std::map<cmStdString, FileToPathMapType> 
  DirectoryToFileToPathMapType;
  typedef std::map<cmStdString, cmDependInformation*> 
  DependInformationMapType;
  DependInformationMapType DependInformationMap;
  DirectoryToFileToPathMapType DirectoryToFileToPathMap;
};

#endif
