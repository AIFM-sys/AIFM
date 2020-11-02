/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmFindCommon.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmFindCommon_h
#define cmFindCommon_h

#include "cmCommand.h"

/** \class cmFindCommon
 * \brief Base class for FIND_XXX implementations.
 *
 * cmFindCommon is a parent class for cmFindBase,
 * cmFindProgramCommand, cmFindPathCommand, cmFindLibraryCommand,
 * cmFindFileCommand, and cmFindPackageCommand.
 */
class cmFindCommon : public cmCommand
{
public:
  cmFindCommon();
  ~cmFindCommon();
  cmTypeMacro(cmFindCommon, cmCommand);

protected:

  enum RootPathMode { RootPathModeBoth,
                      RootPathModeOnlyRootPath,
                      RootPathModeNoRootPath };

  enum PathType { FullPath, CMakePath, EnvPath };

  /** Place a set of search paths under the search roots.  */
  void RerootPaths(std::vector<std::string>& paths);

  /** Add trailing slashes to all search paths.  */
  void AddTrailingSlashes(std::vector<std::string>& paths);

  /** Compute the current default root path mode.  */
  void SelectDefaultRootPathMode();

  /** Compute the current default bundle/framework search policy.  */
  void SelectDefaultMacMode();

  cmStdString CMakePathName;
  RootPathMode FindRootPathMode;

  bool CheckCommonArgument(std::string const& arg);
  void AddPathSuffix(std::string const& arg);
  void AddUserPath(std::string const& p,
                   std::vector<std::string>& paths);
  void AddCMakePath(const char* variable);
  void AddEnvPath(const char* variable);
  void AddPathsInternal(std::vector<std::string> const& in_paths,
                        PathType pathType);
  void AddPathInternal(std::string const& in_path, PathType pathType);

  bool NoDefaultPath;
  bool NoCMakePath;
  bool NoCMakeEnvironmentPath;
  bool NoSystemEnvironmentPath;
  bool NoCMakeSystemPath;

  std::vector<std::string> SearchPathSuffixes;
  std::vector<std::string> UserPaths;
  std::vector<std::string> UserHints;
  std::vector<std::string> SearchPaths;
  std::set<cmStdString> SearchPathsEmitted;

  std::string GenericDocumentationMacPolicy;
  std::string GenericDocumentationRootPath;
  std::string GenericDocumentationPathsOrder;

  bool SearchFrameworkFirst;
  bool SearchFrameworkOnly;
  bool SearchFrameworkLast;

  bool SearchAppBundleFirst;
  bool SearchAppBundleOnly;
  bool SearchAppBundleLast;
};

#endif
