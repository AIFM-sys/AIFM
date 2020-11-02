/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmMakefileLibraryTargetGenerator.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmMakefileLibraryTargetGenerator.h"

#include "cmGeneratedFileStream.h"
#include "cmGlobalUnixMakefileGenerator3.h"
#include "cmLocalUnixMakefileGenerator3.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"
#include "cmTarget.h"
#include "cmake.h"

#include <memory> // auto_ptr

//----------------------------------------------------------------------------
cmMakefileLibraryTargetGenerator
::cmMakefileLibraryTargetGenerator(cmTarget* target):
  cmMakefileTargetGenerator(target)
{
  this->CustomCommandDriver = OnDepends;
  this->Target->GetLibraryNames(
    this->TargetNameOut, this->TargetNameSO, this->TargetNameReal,
    this->TargetNameImport, this->TargetNamePDB,
    this->LocalGenerator->ConfigurationName.c_str());

  if(this->Target->IsFrameworkOnApple())
    {
    this->FrameworkVersion = this->Target->GetFrameworkVersion();
    this->MacContentDirectory = this->Target->GetDirectory();
    this->MacContentDirectory += "/";
    this->MacContentDirectory += this->TargetNameOut;
    this->MacContentDirectory += ".framework/Versions/";
    this->MacContentDirectory += this->FrameworkVersion;
    this->MacContentDirectory += "/";
    }
}

//----------------------------------------------------------------------------
void cmMakefileLibraryTargetGenerator::WriteRuleFiles()
{
  // create the build.make file and directory, put in the common blocks
  this->CreateRuleFile();

  // write rules used to help build object files
  this->WriteCommonCodeRules();

  // write in rules for object files and custom commands
  this->WriteTargetBuildRules();

  // write the per-target per-language flags
  this->WriteTargetLanguageFlags();

  // write the link rules
  // Write the rule for this target type.
  switch(this->Target->GetType())
    {
    case cmTarget::STATIC_LIBRARY:
      this->WriteStaticLibraryRules();
      break;
    case cmTarget::SHARED_LIBRARY:
      this->WriteSharedLibraryRules(false);
      if(this->Target->NeedRelinkBeforeInstall())
        {
        // Write rules to link an installable version of the target.
        this->WriteSharedLibraryRules(true);
        }
      break;
    case cmTarget::MODULE_LIBRARY:
      this->WriteModuleLibraryRules(false);
      if(this->Target->NeedRelinkBeforeInstall())
        {
        // Write rules to link an installable version of the target.
        this->WriteModuleLibraryRules(true);
        }
      break;
    default:
      // If language is not known, this is an error.
      cmSystemTools::Error("Unknown Library Type");
      break;
    }

  // Write the requires target.
  this->WriteTargetRequiresRules();

  // Write clean target
  this->WriteTargetCleanRules();

  // Write the dependency generation rule.  This must be done last so
  // that multiple output pair information is available.
  this->WriteTargetDependRules();

  // close the streams
  this->CloseFileStreams();
}

//----------------------------------------------------------------------------
void cmMakefileLibraryTargetGenerator::WriteStaticLibraryRules()
{
  const char* linkLanguage =
    this->Target->GetLinkerLanguage(this->GlobalGenerator);
  std::string linkRuleVar = "CMAKE_";
  if (linkLanguage)
    {
    linkRuleVar += linkLanguage;
    }
  linkRuleVar += "_CREATE_STATIC_LIBRARY";

  std::string extraFlags;
  this->LocalGenerator->AppendFlags
    (extraFlags,this->Target->GetProperty("STATIC_LIBRARY_FLAGS"));
  this->WriteLibraryRules(linkRuleVar.c_str(), extraFlags.c_str(), false);
}

//----------------------------------------------------------------------------
void cmMakefileLibraryTargetGenerator::WriteSharedLibraryRules(bool relink)
{
  if(this->Target->IsFrameworkOnApple())
    {
    this->WriteFrameworkRules(relink);
    return;
    }
  const char* linkLanguage =
    this->Target->GetLinkerLanguage(this->GlobalGenerator);
  std::string linkRuleVar = "CMAKE_";
  if (linkLanguage)
    {
    linkRuleVar += linkLanguage;
    }
  linkRuleVar += "_CREATE_SHARED_LIBRARY";

  std::string extraFlags;
  this->LocalGenerator->AppendFlags
    (extraFlags, this->Target->GetProperty("LINK_FLAGS"));
  std::string linkFlagsConfig = "LINK_FLAGS_";
  linkFlagsConfig += 
    cmSystemTools::UpperCase(this->LocalGenerator->ConfigurationName.c_str());
  this->LocalGenerator->AppendFlags
    (extraFlags, this->Target->GetProperty(linkFlagsConfig.c_str()));
                                    
  this->LocalGenerator->AddConfigVariableFlags
    (extraFlags, "CMAKE_SHARED_LINKER_FLAGS",
     this->LocalGenerator->ConfigurationName.c_str());
  if(this->Makefile->IsOn("WIN32") && !(this->Makefile->IsOn("CYGWIN") 
                                        || this->Makefile->IsOn("MINGW")))
    {
    const std::vector<cmSourceFile*>& sources = 
      this->Target->GetSourceFiles();
    for(std::vector<cmSourceFile*>::const_iterator i = sources.begin();
        i != sources.end(); ++i)
      {
      cmSourceFile* sf = *i;
      if(sf->GetExtension() == "def")
        {
        extraFlags += " ";
        extraFlags += 
          this->Makefile->GetSafeDefinition("CMAKE_LINK_DEF_FILE_FLAG");
        extraFlags += 
          this->Convert(sf->GetFullPath().c_str(),
                        cmLocalGenerator::START_OUTPUT,
                        cmLocalGenerator::SHELL);
        }
      }
    }
  this->WriteLibraryRules(linkRuleVar.c_str(), extraFlags.c_str(), relink);
}

//----------------------------------------------------------------------------
void cmMakefileLibraryTargetGenerator::WriteModuleLibraryRules(bool relink)
{
  const char* linkLanguage =
    this->Target->GetLinkerLanguage(this->GlobalGenerator);
  std::string linkRuleVar = "CMAKE_";
  if (linkLanguage)
    {
    linkRuleVar += linkLanguage;
    }
  linkRuleVar += "_CREATE_SHARED_MODULE";

  std::string extraFlags;
  this->LocalGenerator->AppendFlags(extraFlags, 
                                    this->Target->GetProperty("LINK_FLAGS"));
  std::string linkFlagsConfig = "LINK_FLAGS_";
  linkFlagsConfig += 
    cmSystemTools::UpperCase(this->LocalGenerator->ConfigurationName.c_str());
  this->LocalGenerator->AppendFlags
    (extraFlags, this->Target->GetProperty(linkFlagsConfig.c_str()));
  this->LocalGenerator->AddConfigVariableFlags
    (extraFlags, "CMAKE_MODULE_LINKER_FLAGS",
     this->LocalGenerator->ConfigurationName.c_str());

  // TODO: .def files should be supported here also.
  this->WriteLibraryRules(linkRuleVar.c_str(), extraFlags.c_str(), relink);
}

//----------------------------------------------------------------------------
void cmMakefileLibraryTargetGenerator::WriteFrameworkRules(bool relink)
{
  const char* linkLanguage =
    this->Target->GetLinkerLanguage(this->GlobalGenerator);
  std::string linkRuleVar = "CMAKE_";
  if (linkLanguage)
    {
    linkRuleVar += linkLanguage;
    }
  linkRuleVar += "_CREATE_MACOSX_FRAMEWORK";

  std::string extraFlags;
  this->LocalGenerator->AppendFlags(extraFlags, 
                                    this->Target->GetProperty("LINK_FLAGS"));
  std::string linkFlagsConfig = "LINK_FLAGS_";
  linkFlagsConfig += 
    cmSystemTools::UpperCase(this->LocalGenerator->ConfigurationName.c_str());
  this->LocalGenerator->AppendFlags
    (extraFlags, this->Target->GetProperty(linkFlagsConfig.c_str()));
  this->LocalGenerator->AddConfigVariableFlags
    (extraFlags, "CMAKE_MACOSX_FRAMEWORK_LINKER_FLAGS",
     this->LocalGenerator->ConfigurationName.c_str());

  // TODO: .def files should be supported here also.
  this->WriteLibraryRules(linkRuleVar.c_str(), extraFlags.c_str(), relink);
}

//----------------------------------------------------------------------------
void cmMakefileLibraryTargetGenerator::CreateFramework()
{
  // TODO: Use the cmMakefileTargetGenerator::ExtraFiles vector to
  // drive rules to create these files at build time.
  std::string oldName;
  std::string newName;

  // Compute the location of the top-level foo.framework directory.
  std::string top = this->Target->GetDirectory();
  top += "/";
  top += this->TargetNameOut;
  top += ".framework/";

  // Make foo.framework/Versions
  std::string versions = top;
  versions += "Versions";
  cmSystemTools::MakeDirectory(versions.c_str());

  // Make foo.framework/Versions/version
  std::string version = versions;
  version += "/";
  version += this->FrameworkVersion;
  cmSystemTools::MakeDirectory(version.c_str());

  // Current -> version
  oldName = this->FrameworkVersion;
  newName = versions;
  newName += "/Current";
  cmSystemTools::RemoveFile(newName.c_str());
  cmSystemTools::CreateSymlink(oldName.c_str(), newName.c_str());
  this->Makefile->AddCMakeOutputFile(newName.c_str());

  // foo -> Versions/Current/foo
  oldName = "Versions/Current/";
  oldName += this->TargetNameOut;
  newName = top;
  newName += this->TargetNameOut;
  cmSystemTools::RemoveFile(newName.c_str());
  cmSystemTools::CreateSymlink(oldName.c_str(), newName.c_str());
  this->Makefile->AddCMakeOutputFile(newName.c_str());

  // Resources -> Versions/Current/Resources
  if(this->MacContentFolders.find("Resources") !=
     this->MacContentFolders.end())
    {
    oldName = "Versions/Current/Resources";
    newName = top;
    newName += "Resources";
    cmSystemTools::RemoveFile(newName.c_str());
    cmSystemTools::CreateSymlink(oldName.c_str(), newName.c_str());
    this->Makefile->AddCMakeOutputFile(newName.c_str());
    }

  // Headers -> Versions/Current/Headers
  if(this->MacContentFolders.find("Headers") !=
     this->MacContentFolders.end())
    {
    oldName = "Versions/Current/Headers";
    newName = top;
    newName += "Headers";
    cmSystemTools::RemoveFile(newName.c_str());
    cmSystemTools::CreateSymlink(oldName.c_str(), newName.c_str());
    this->Makefile->AddCMakeOutputFile(newName.c_str());
    }

  // PrivateHeaders -> Versions/Current/PrivateHeaders
  if(this->MacContentFolders.find("PrivateHeaders") !=
     this->MacContentFolders.end())
    {
    oldName = "Versions/Current/PrivateHeaders";
    newName = top;
    newName += "PrivateHeaders";
    cmSystemTools::RemoveFile(newName.c_str());
    cmSystemTools::CreateSymlink(oldName.c_str(), newName.c_str());
    this->Makefile->AddCMakeOutputFile(newName.c_str());
    }
}

//----------------------------------------------------------------------------
void cmMakefileLibraryTargetGenerator::WriteLibraryRules
(const char* linkRuleVar, const char* extraFlags, bool relink)
{
  // TODO: Merge the methods that call this method to avoid
  // code duplication.
  std::vector<std::string> commands;

  std::string relPath = this->LocalGenerator->GetHomeRelativeOutputPath();
  std::string objTarget;

  // Build list of dependencies.
  std::vector<std::string> depends;
  for(std::vector<std::string>::const_iterator obj = this->Objects.begin();
      obj != this->Objects.end(); ++obj)
    {
    objTarget = relPath;
    objTarget += *obj;
    depends.push_back(objTarget);
    }

  // Add dependencies on targets that must be built first.
  this->AppendTargetDepends(depends);

  // Add a dependency on the rule file itself.
  this->LocalGenerator->AppendRuleDepend(depends, 
                                         this->BuildFileNameFull.c_str());
  
  for(std::vector<std::string>::const_iterator obj 
        = this->ExternalObjects.begin();
      obj != this->ExternalObjects.end(); ++obj)
    {
    depends.push_back(*obj);
    }
  
  // Get the language to use for linking this library.
  const char* linkLanguage =
    this->Target->GetLinkerLanguage(this->GlobalGenerator);

  // Make sure we have a link language.
  if(!linkLanguage)
    {
    cmSystemTools::Error("Cannot determine link language for target \"",
                         this->Target->GetName(), "\".");
    return;
    }

  // Create set of linking flags.
  std::string linkFlags;
  this->LocalGenerator->AppendFlags(linkFlags, extraFlags);

  // Construct the name of the library.
  std::string targetName;
  std::string targetNameSO;
  std::string targetNameReal;
  std::string targetNameImport;
  std::string targetNamePDB;
  this->Target->GetLibraryNames(
    targetName, targetNameSO, targetNameReal, targetNameImport, targetNamePDB,
    this->LocalGenerator->ConfigurationName.c_str());

  // Construct the full path version of the names.
  std::string outpath;
  std::string outpathImp;
  if(this->Target->IsFrameworkOnApple())
    {
    outpath = this->MacContentDirectory;
    this->CreateFramework();
    }
  else if(relink)
    {
    outpath = this->Makefile->GetStartOutputDirectory();
    outpath += cmake::GetCMakeFilesDirectory();
    outpath += "/CMakeRelink.dir";
    cmSystemTools::MakeDirectory(outpath.c_str());
    outpath += "/";
    if(!targetNameImport.empty())
      {
      outpathImp = outpath;
      }
    }
  else
    {
    outpath = this->Target->GetDirectory();
    cmSystemTools::MakeDirectory(outpath.c_str());
    outpath += "/";
    if(!targetNameImport.empty())
      {
      outpathImp = this->Target->GetDirectory(0, true);
      cmSystemTools::MakeDirectory(outpathImp.c_str());
      outpathImp += "/";
      }
    }

  std::string targetFullPath = outpath + targetName;
  std::string targetFullPathPDB = outpath + targetNamePDB;
  std::string targetFullPathSO = outpath + targetNameSO;
  std::string targetFullPathReal = outpath + targetNameReal;
  std::string targetFullPathImport = outpathImp + targetNameImport;

  // Construct the output path version of the names for use in command
  // arguments.
  std::string targetOutPathPDB = 
    this->Convert(targetFullPathPDB.c_str(),cmLocalGenerator::FULL,
                  cmLocalGenerator::SHELL);
  std::string targetOutPath = 
    this->Convert(targetFullPath.c_str(),cmLocalGenerator::START_OUTPUT,
                  cmLocalGenerator::SHELL);
  std::string targetOutPathSO = 
    this->Convert(targetFullPathSO.c_str(),cmLocalGenerator::START_OUTPUT,
                  cmLocalGenerator::SHELL);
  std::string targetOutPathReal = 
    this->Convert(targetFullPathReal.c_str(),cmLocalGenerator::START_OUTPUT,
                  cmLocalGenerator::SHELL);
  std::string targetOutPathImport =
    this->Convert(targetFullPathImport.c_str(),cmLocalGenerator::START_OUTPUT,
                  cmLocalGenerator::SHELL);

  // Add the link message.
  std::string buildEcho = "Linking ";
  buildEcho += linkLanguage;
  const char* forbiddenFlagVar = 0;
  switch(this->Target->GetType())
    {
    case cmTarget::STATIC_LIBRARY:
      buildEcho += " static library "; 
      break;
    case cmTarget::SHARED_LIBRARY:
      forbiddenFlagVar = "_CREATE_SHARED_LIBRARY_FORBIDDEN_FLAGS";
      buildEcho += " shared library ";
      break;
    case cmTarget::MODULE_LIBRARY:
      forbiddenFlagVar = "_CREATE_SHARED_MODULE_FORBIDDEN_FLAGS";
      buildEcho += " shared module ";
      break;
    default:
      buildEcho += " library "; 
      break;
    }
  buildEcho += targetOutPath.c_str();
  this->LocalGenerator->AppendEcho(commands, buildEcho.c_str(),
                                   cmLocalUnixMakefileGenerator3::EchoLink);

  // Construct a list of files associated with this library that may
  // need to be cleaned.
  std::vector<std::string> libCleanFiles;
  if(this->Target->GetPropertyAsBool("CLEAN_DIRECT_OUTPUT"))
    {
    // The user has requested that only the files directly built
    // by this target be cleaned instead of all possible names.
    libCleanFiles.push_back(this->Convert(targetFullPath.c_str(),
          cmLocalGenerator::START_OUTPUT,
          cmLocalGenerator::UNCHANGED));
    if(targetNameReal != targetName)
      {
      libCleanFiles.push_back(this->Convert(targetFullPathReal.c_str(),
          cmLocalGenerator::START_OUTPUT,
          cmLocalGenerator::UNCHANGED));
      }
    if(targetNameSO != targetName &&
       targetNameSO != targetNameReal)
      {
      libCleanFiles.push_back(this->Convert(targetFullPathSO.c_str(),
          cmLocalGenerator::START_OUTPUT,
          cmLocalGenerator::UNCHANGED));
      }
    if(!targetNameImport.empty())
      {
      libCleanFiles.push_back(this->Convert(targetFullPathImport.c_str(),
          cmLocalGenerator::START_OUTPUT,
          cmLocalGenerator::UNCHANGED));
      }
    }
  else
    {
    // This target may switch between static and shared based
    // on a user option or the BUILD_SHARED_LIBS switch.  Clean
    // all possible names.
    std::string cleanStaticName;
    std::string cleanSharedName;
    std::string cleanSharedSOName;
    std::string cleanSharedRealName;
    std::string cleanImportName;
    std::string cleanPDBName;
    this->Target->GetLibraryCleanNames(
      cleanStaticName,
      cleanSharedName,
      cleanSharedSOName,
      cleanSharedRealName,
      cleanImportName,
      cleanPDBName,
      this->LocalGenerator->ConfigurationName.c_str());
    std::string cleanFullStaticName = outpath + cleanStaticName;
    std::string cleanFullSharedName = outpath + cleanSharedName;
    std::string cleanFullSharedSOName = outpath + cleanSharedSOName;
    std::string cleanFullSharedRealName = outpath + cleanSharedRealName;
    std::string cleanFullImportName = outpathImp + cleanImportName;
    std::string cleanFullPDBName = outpath + cleanPDBName;
    libCleanFiles.push_back
      (this->Convert(cleanFullStaticName.c_str(),
                     cmLocalGenerator::START_OUTPUT,
                     cmLocalGenerator::UNCHANGED));
    if(cleanSharedRealName != cleanStaticName)
      {
      libCleanFiles.push_back(this->Convert(cleanFullSharedRealName.c_str(),
          cmLocalGenerator::START_OUTPUT,
          cmLocalGenerator::UNCHANGED));
      }
    if(cleanSharedSOName != cleanStaticName &&
      cleanSharedSOName != cleanSharedRealName)
      {
      libCleanFiles.push_back(this->Convert(cleanFullSharedSOName.c_str(),
          cmLocalGenerator::START_OUTPUT,
          cmLocalGenerator::UNCHANGED));
      }
    if(cleanSharedName != cleanStaticName &&
      cleanSharedName != cleanSharedSOName &&
      cleanSharedName != cleanSharedRealName)
      {
      libCleanFiles.push_back(this->Convert(cleanFullSharedName.c_str(),
          cmLocalGenerator::START_OUTPUT,
          cmLocalGenerator::UNCHANGED));
      }
    if(!cleanImportName.empty())
      {
      libCleanFiles.push_back(this->Convert(cleanFullImportName.c_str(),
          cmLocalGenerator::START_OUTPUT,
          cmLocalGenerator::UNCHANGED));
      }

    // List the PDB for cleaning only when the whole target is
    // cleaned.  We do not want to delete the .pdb file just before
    // linking the target.
    this->CleanFiles.push_back
      (this->Convert(cleanFullPDBName.c_str(),
                     cmLocalGenerator::START_OUTPUT,
                     cmLocalGenerator::UNCHANGED));
    }

#ifdef _WIN32
  // There may be a manifest file for this target.  Add it to the
  // clean set just in case.
  if(this->Target->GetType() != cmTarget::STATIC_LIBRARY)
    {
    libCleanFiles.push_back(
      this->Convert((targetFullPath+".manifest").c_str(),
                    cmLocalGenerator::START_OUTPUT,
                    cmLocalGenerator::UNCHANGED));
    }
#endif

  std::vector<std::string> commands1;
  // Add a command to remove any existing files for this library.
  // for static libs only 
  if(this->Target->GetType() == cmTarget::STATIC_LIBRARY)
    {
    this->LocalGenerator->AppendCleanCommand(commands1, libCleanFiles,
                                             *this->Target, "target");
    this->LocalGenerator->CreateCDCommand
      (commands1,
       this->Makefile->GetStartOutputDirectory(),
       this->Makefile->GetHomeOutputDirectory());
    commands.insert(commands.end(), commands1.begin(), commands1.end());
    commands1.clear();
    }

  // Add the pre-build and pre-link rules building but not when relinking.
  if(!relink)
    {
    this->LocalGenerator
      ->AppendCustomCommands(commands, this->Target->GetPreBuildCommands());
    this->LocalGenerator
      ->AppendCustomCommands(commands, this->Target->GetPreLinkCommands());
    }

  // Determine whether a link script will be used.
  bool useLinkScript = this->GlobalGenerator->GetUseLinkScript();

  // Select whether to use a response file for objects.
  bool useResponseFile = false;
  {
  std::string responseVar = "CMAKE_";
  responseVar += linkLanguage;
  responseVar += "_USE_RESPONSE_FILE_FOR_OBJECTS";
  if(this->Makefile->IsOn(responseVar.c_str()))
    {
    useResponseFile = true;
    }
  }

  // For static libraries there might be archiving rules.
  std::vector<std::string> archiveCreateCommands;
  std::vector<std::string> archiveAppendCommands;
  std::vector<std::string> archiveFinishCommands;
  std::string::size_type archiveCommandLimit = std::string::npos;
  if(this->Target->GetType() == cmTarget::STATIC_LIBRARY)
    {
    std::string arCreateVar = "CMAKE_";
    arCreateVar += linkLanguage;
    arCreateVar += "_ARCHIVE_CREATE";
    if(const char* rule = this->Makefile->GetDefinition(arCreateVar.c_str()))
      {
      cmSystemTools::ExpandListArgument(rule, archiveCreateCommands);
      }
    std::string arAppendVar = "CMAKE_";
    arAppendVar += linkLanguage;
    arAppendVar += "_ARCHIVE_APPEND";
    if(const char* rule = this->Makefile->GetDefinition(arAppendVar.c_str()))
      {
      cmSystemTools::ExpandListArgument(rule, archiveAppendCommands);
      }
    std::string arFinishVar = "CMAKE_";
    arFinishVar += linkLanguage;
    arFinishVar += "_ARCHIVE_FINISH";
    if(const char* rule = this->Makefile->GetDefinition(arFinishVar.c_str()))
      {
      cmSystemTools::ExpandListArgument(rule, archiveFinishCommands);
      }
    }

  // Decide whether to use archiving rules.
  bool useArchiveRules =
    !archiveCreateCommands.empty() && !archiveAppendCommands.empty();
  if(useArchiveRules)
    {
    // Archiving rules are always run with a link script.
    useLinkScript = true;

    // Archiving rules never use a response file.
    useResponseFile = false;

    // Limit the length of individual object lists to less than the
    // 32K command line length limit on Windows.  We could make this a
    // platform file variable but this should work everywhere.
    archiveCommandLimit = 30000;
    }

  // Expand the rule variables.
  std::vector<std::string> real_link_commands;
  {
  // Set path conversion for link script shells.
  this->LocalGenerator->SetLinkScriptShell(useLinkScript);

  // Collect up flags to link in needed libraries.
  cmOStringStream linklibs;
  if(this->Target->GetType() != cmTarget::STATIC_LIBRARY)
    {
    this->LocalGenerator
      ->OutputLinkLibraries(linklibs, *this->Target, relink);
    }

  // Construct object file lists that may be needed to expand the
  // rule.
  std::string variableName;
  std::string variableNameExternal;
  this->WriteObjectsVariable(variableName, variableNameExternal);
  std::string buildObjs;
  if(useResponseFile)
    {
    std::string objects;
    this->WriteObjectsString(objects);
    std::string objects_rsp =
      this->CreateResponseFile("objects.rsp", objects, depends);
    buildObjs = "@";
    buildObjs += this->Convert(objects_rsp.c_str(),
                               cmLocalGenerator::NONE,
                               cmLocalGenerator::SHELL);
    }
  else if(useLinkScript)
    {
    if(!useArchiveRules)
      {
      this->WriteObjectsString(buildObjs);
      }
    }
  else
    {
    buildObjs = "$(";
    buildObjs += variableName;
    buildObjs += ") $(";
    buildObjs += variableNameExternal;
    buildObjs += ")";
    }
  std::string cleanObjs = "$(";
  cleanObjs += variableName;
  cleanObjs += ")";
  cmLocalGenerator::RuleVariables vars;
  vars.TargetPDB = targetOutPathPDB.c_str();

  // Setup the target version.
  std::string targetVersionMajor;
  std::string targetVersionMinor;
  {
  cmOStringStream majorStream;
  cmOStringStream minorStream;
  int major;
  int minor;
  this->Target->GetTargetVersion(major, minor);
  majorStream << major;
  minorStream << minor;
  targetVersionMajor = majorStream.str();
  targetVersionMinor = minorStream.str();
  }
  vars.TargetVersionMajor = targetVersionMajor.c_str();
  vars.TargetVersionMinor = targetVersionMinor.c_str();

  vars.Language = linkLanguage;
  vars.Objects = buildObjs.c_str();
  std::string objdir = cmake::GetCMakeFilesDirectoryPostSlash();
  objdir += this->Target->GetName();
  objdir += ".dir";
  objdir = this->Convert(objdir.c_str(),
                         cmLocalGenerator::START_OUTPUT,
                         cmLocalGenerator::SHELL);
  vars.ObjectDir = objdir.c_str();
  vars.Target = targetOutPathReal.c_str();
  std::string linkString = linklibs.str();
  vars.LinkLibraries = linkString.c_str();
  vars.ObjectsQuoted = buildObjs.c_str();
  vars.TargetSOName= targetNameSO.c_str();
  vars.LinkFlags = linkFlags.c_str();

  // Compute the directory portion of the install_name setting.
  std::string install_name_dir;
  if(this->Target->GetType() == cmTarget::SHARED_LIBRARY)
    {
    // Get the install_name directory for the build tree.
    const char* config = this->LocalGenerator->ConfigurationName.c_str();
    install_name_dir = this->Target->GetInstallNameDirForBuildTree(config);

    // Set the rule variable replacement value.
    if(install_name_dir.empty())
      {
      vars.TargetInstallNameDir = "";
      }
    else
      {
      // Convert to a path for the native build tool.
      install_name_dir =
        this->LocalGenerator->Convert(install_name_dir.c_str(),
                                      cmLocalGenerator::NONE,
                                      cmLocalGenerator::SHELL, false);
      vars.TargetInstallNameDir = install_name_dir.c_str();
      }
    }
  std::string langFlags;
  this->LocalGenerator
    ->AddLanguageFlags(langFlags, linkLanguage,
                       this->LocalGenerator->ConfigurationName.c_str());
  // remove any language flags that might not work with the
  // particular os
  if(forbiddenFlagVar)
    {
    this->RemoveForbiddenFlags(forbiddenFlagVar,
                               linkLanguage, langFlags);
    }
  vars.LanguageCompileFlags = langFlags.c_str();

  // Construct the main link rule and expand placeholders.
  this->LocalGenerator->TargetImplib = targetOutPathImport;
  if(useArchiveRules)
    {
    // Construct the individual object list strings.
    std::vector<std::string> object_strings;
    this->WriteObjectsStrings(object_strings, archiveCommandLimit);

    // Create the archive with the first set of objects.
    std::vector<std::string>::iterator osi = object_strings.begin();
    {
    vars.Objects = osi->c_str();
    for(std::vector<std::string>::const_iterator
          i = archiveCreateCommands.begin();
        i != archiveCreateCommands.end(); ++i)
      {
      std::string cmd = *i;
      this->LocalGenerator->ExpandRuleVariables(cmd, vars);
      real_link_commands.push_back(cmd);
      }
    }
    // Append to the archive with the other object sets.
    for(++osi; osi != object_strings.end(); ++osi)
      {
      vars.Objects = osi->c_str();
      for(std::vector<std::string>::const_iterator
            i = archiveAppendCommands.begin();
          i != archiveAppendCommands.end(); ++i)
        {
        std::string cmd = *i;
        this->LocalGenerator->ExpandRuleVariables(cmd, vars);
        real_link_commands.push_back(cmd);
        }
      }
    // Finish the archive.
    vars.Objects = "";
    for(std::vector<std::string>::const_iterator
          i = archiveFinishCommands.begin();
        i != archiveFinishCommands.end(); ++i)
      {
      std::string cmd = *i;
      this->LocalGenerator->ExpandRuleVariables(cmd, vars);
      real_link_commands.push_back(cmd);
      }
    }
  else
    {
    // Get the set of commands.
    std::string linkRule = this->Makefile->GetRequiredDefinition(linkRuleVar);
    cmSystemTools::ExpandListArgument(linkRule, real_link_commands);

    // Expand placeholders.
    for(std::vector<std::string>::iterator i = real_link_commands.begin();
        i != real_link_commands.end(); ++i)
      {
      this->LocalGenerator->ExpandRuleVariables(*i, vars);
      }
    }
  this->LocalGenerator->TargetImplib = "";

  // Restore path conversion to normal shells.
  this->LocalGenerator->SetLinkScriptShell(false);
  }

  // Optionally convert the build rule to use a script to avoid long
  // command lines in the make shell.
  if(useLinkScript)
    {
    // Use a link script.
    const char* name = (relink? "relink.txt" : "link.txt");
    this->CreateLinkScript(name, real_link_commands, commands1, depends);
    }
  else
    {
    // No link script.  Just use the link rule directly.
    commands1 = real_link_commands;
    }
  this->LocalGenerator->CreateCDCommand
    (commands1,
     this->Makefile->GetStartOutputDirectory(),
     this->Makefile->GetHomeOutputDirectory());
  commands.insert(commands.end(), commands1.begin(), commands1.end());
  commands1.clear();

  // Add a rule to create necessary symlinks for the library.
  if(targetOutPath != targetOutPathReal)
    {
    std::string symlink = "$(CMAKE_COMMAND) -E cmake_symlink_library ";
    symlink += targetOutPathReal;
    symlink += " ";
    symlink += targetOutPathSO;
    symlink += " ";
    symlink += targetOutPath;
    commands1.push_back(symlink);
    this->LocalGenerator->CreateCDCommand(commands1,
                                  this->Makefile->GetStartOutputDirectory(),
                                  this->Makefile->GetHomeOutputDirectory());
    commands.insert(commands.end(), commands1.begin(), commands1.end());
    commands1.clear();
    }
  // Add the post-build rules when building but not when relinking.
  if(!relink)
    {
    this->LocalGenerator->
      AppendCustomCommands(commands, this->Target->GetPostBuildCommands());
    }

  // Write the build rule.
  this->LocalGenerator->WriteMakeRule(*this->BuildFileStream, 0,
                                      targetFullPathReal.c_str(),
                                      depends, commands, false);

  // Some targets have more than one output file.  Create rules to
  // drive the build if any extra outputs are missing.
  std::vector<std::string> extraOutputs;
  if(targetNameSO != targetNameReal)
    {
    this->GenerateExtraOutput(targetFullPathSO.c_str(),
                              targetFullPathReal.c_str());
    }
  if(targetName != targetNameSO &&
     targetName != targetNameReal)
    {
    this->GenerateExtraOutput(targetFullPath.c_str(),
                              targetFullPathReal.c_str());
    }

  // Write the main driver rule to build everything in this target.
  this->WriteTargetDriverRule(targetFullPath.c_str(), relink);

  // Clean all the possible library names and symlinks.
  this->CleanFiles.insert(this->CleanFiles.end(),
                          libCleanFiles.begin(),libCleanFiles.end());
}
