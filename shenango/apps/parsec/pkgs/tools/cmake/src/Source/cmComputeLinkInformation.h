/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmComputeLinkInformation.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmComputeLinkInformation_h
#define cmComputeLinkInformation_h

#include "cmStandardIncludes.h"

#include <cmsys/RegularExpression.hxx>

class cmake;
class cmGlobalGenerator;
class cmLocalGenerator;
class cmMakefile;
class cmTarget;
class cmOrderDirectories;

/** \class cmComputeLinkInformation
 * \brief Compute link information for a target in one configuration.
 */
class cmComputeLinkInformation
{
public:
  cmComputeLinkInformation(cmTarget* target, const char* config);
  ~cmComputeLinkInformation();
  bool Compute();

  struct Item
  {
    Item(): Value(), IsPath(true) {}
    Item(Item const& item): Value(item.Value), IsPath(item.IsPath) {}
    Item(std::string const& v, bool p): Value(v), IsPath(p) {}
    std::string Value;
    bool IsPath;
  };
  typedef std::vector<Item> ItemVector;
  ItemVector const& GetItems();
  std::vector<std::string> const& GetDirectories();
  std::vector<std::string> const& GetDepends();
  std::vector<std::string> const& GetFrameworkPaths();
  const char* GetLinkLanguage() const { return this->LinkLanguage; }
  std::vector<std::string> const& GetRuntimeSearchPath();
  std::string const& GetRuntimeFlag() const { return this->RuntimeFlag; }
  std::string const& GetRuntimeSep() const { return this->RuntimeSep; }
  void GetRPath(std::vector<std::string>& runtimeDirs, bool for_install);
  std::string GetRPathString(bool for_install);
  std::string GetChrpathString();
  std::set<cmTarget*> const& GetSharedLibrariesLinked();

  std::string const& GetRPathLinkFlag() const { return this->RPathLinkFlag; }
  std::string GetRPathLinkString();
private:
  void AddItem(std::string const& item, cmTarget* tgt);
  void AddSharedDepItem(std::string const& item, cmTarget* tgt);

  // Output information.
  ItemVector Items;
  std::vector<std::string> Directories;
  std::vector<std::string> Depends;
  std::vector<std::string> FrameworkPaths;
  std::vector<std::string> RuntimeSearchPath;
  std::set<cmTarget*> SharedLibrariesLinked;

  // Context information.
  cmTarget* Target;
  cmMakefile* Makefile;
  cmLocalGenerator* LocalGenerator;
  cmGlobalGenerator* GlobalGenerator;
  cmake* CMakeInstance;

  // Configuration information.
  const char* Config;
  const char* LinkLanguage;

  // Modes for dealing with dependent shared libraries.
  enum SharedDepMode
  {
    SharedDepModeNone,   // Drop
    SharedDepModeDir,    // List dir in -rpath-link flag
    SharedDepModeLibDir, // List dir in linker search path
    SharedDepModeLink    // List file on link line
  };

  // System info.
  bool UseImportLibrary;
  const char* LoaderFlag;
  std::string LibLinkFlag;
  std::string LibLinkFileFlag;
  std::string LibLinkSuffix;
  std::string RuntimeFlag;
  std::string RuntimeSep;
  std::string RuntimeAlways;
  bool RuntimeUseChrpath;
  bool NoSONameUsesPath;
  bool LinkWithRuntimePath;
  std::string RPathLinkFlag;
  SharedDepMode SharedDependencyMode;

  // Link type adjustment.
  void ComputeLinkTypeInfo();
  enum LinkType { LinkUnknown, LinkStatic, LinkShared };
  LinkType StartLinkType;
  LinkType CurrentLinkType;
  std::string StaticLinkTypeFlag;
  std::string SharedLinkTypeFlag;
  bool LinkTypeEnabled;
  void SetCurrentLinkType(LinkType lt);
  bool ArchivesMayBeShared;

  // Link item parsing.
  void ComputeItemParserInfo();
  std::vector<std::string> StaticLinkExtensions;
  std::vector<std::string> SharedLinkExtensions;
  std::vector<std::string> LinkExtensions;
  std::set<cmStdString> LinkPrefixes;
  cmsys::RegularExpression ExtractStaticLibraryName;
  cmsys::RegularExpression ExtractSharedLibraryName;
  cmsys::RegularExpression ExtractAnyLibraryName;
  std::string SharedRegexString;
  void AddLinkPrefix(const char* p);
  void AddLinkExtension(const char* e, LinkType type);
  std::string CreateExtensionRegex(std::vector<std::string> const& exts);
  std::string NoCaseExpression(const char* str);

  // Handling of link items.
  void AddTargetItem(std::string const& item, cmTarget* target);
  void AddFullItem(std::string const& item);
  bool CheckImplicitDirItem(std::string const& item);
  void AddUserItem(std::string const& item, bool pathNotKnown);
  void AddDirectoryItem(std::string const& item);
  void AddFrameworkItem(std::string const& item);
  void DropDirectoryItem(std::string const& item);
  bool CheckSharedLibNoSOName(std::string const& item);
  void AddSharedLibNoSOName(std::string const& item);
  void HandleBadFullItem(std::string const& item, std::string const& file);

  // Framework info.
  void ComputeFrameworkInfo();
  void AddFrameworkPath(std::string const& p);
  std::set<cmStdString> FrameworkPathsEmmitted;
  cmsys::RegularExpression SplitFramework;

  // Linker search path computation.
  cmOrderDirectories* OrderLinkerSearchPath;
  bool FinishLinkerSearchDirectories();
  void PrintLinkPolicyDiagnosis(std::ostream&);
  std::set<cmStdString> ImplicitLinkDirs;

  // Linker search path compatibility mode.
  std::set<cmStdString> OldLinkDirMask;
  std::vector<std::string> OldLinkDirItems;
  std::vector<std::string> OldUserFlagItems;
  bool OldLinkDirMode;

  // Runtime path computation.
  cmOrderDirectories* OrderRuntimeSearchPath;
  void AddLibraryRuntimeInfo(std::string const& fullPath, cmTarget* target);
  void AddLibraryRuntimeInfo(std::string const& fullPath);

  // Dependent library path computation.
  cmOrderDirectories* OrderDependentRPath;
};

#endif
