/*=========================================================================

Program:   CMake - Cross-Platform Makefile Generator
Module:    $RCSfile: cmGlobalXCodeGenerator.cxx,v $
Language:  C++
Date:      $Date: 2012/03/29 17:21:08 $
Version:   $Revision: 1.1.1.1 $

Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even 
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmGlobalXCodeGenerator.h"
#include "cmGlobalXCode21Generator.h"
#include "cmLocalXCodeGenerator.h"
#include "cmMakefile.h"
#include "cmXCodeObject.h"
#include "cmXCode21Object.h"
#include "cmake.h"
#include "cmGeneratedFileStream.h"
#include "cmComputeLinkInformation.h"
#include "cmSourceFile.h"

//----------------------------------------------------------------------------
#if defined(CMAKE_BUILD_WITH_CMAKE)
#include "cmXMLParser.h"

// parse the xml file storing the installed version of Xcode on
// the machine
class cmXcodeVersionParser : public cmXMLParser
{
public:
  void StartElement(const char* , const char** )
    {
      this->Data = "";
    }
  void EndElement(const char* name)
    {
      if(strcmp(name, "key") == 0)
        {
        this->Key = this->Data;
        }
      else if(strcmp(name, "string") == 0)
        {
        if(this->Key == "CFBundleShortVersionString")
          {
          this->Version = (int)(10.0 * atof(this->Data.c_str()));
          }
        }
    }
  void CharacterDataHandler(const char* data, int length)
    {
      this->Data.append(data, length);
    }
  int Version;
  std::string Key;
  std::string Data;
};
#endif

//----------------------------------------------------------------------------
cmGlobalXCodeGenerator::cmGlobalXCodeGenerator()
{
  this->FindMakeProgramFile = "CMakeFindXCode.cmake";
  this->RootObject = 0;
  this->MainGroupChildren = 0;
  this->SourcesGroupChildren = 0;
  this->ResourcesGroupChildren = 0;
  this->CurrentMakefile = 0;
  this->CurrentLocalGenerator = 0;
  this->XcodeVersion = 15;
}

//----------------------------------------------------------------------------
cmGlobalGenerator* cmGlobalXCodeGenerator::New()
{ 
#if defined(CMAKE_BUILD_WITH_CMAKE)  
  cmXcodeVersionParser parser;
  parser.ParseFile
    ("/Developer/Applications/Xcode.app/Contents/version.plist");
  if(parser.Version == 15)
    {
    return new cmGlobalXCodeGenerator;
    }
  else if (parser.Version == 20)
    {
    cmSystemTools::Message("Xcode 2.0 not really supported by cmake, "
                           "using Xcode 15 generator\n");
    return new cmGlobalXCodeGenerator;
    }
  cmGlobalXCodeGenerator* ret = new cmGlobalXCode21Generator;
  ret->SetVersion(parser.Version);
  return ret;
#else
  std::cerr << "CMake should be built with cmake to use XCode, "
    "default to Xcode 1.5\n";
  return new cmGlobalXCodeGenerator;
#endif
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::EnableLanguage(std::vector<std::string>const&
                                            lang,
                                            cmMakefile * mf, bool optional)
{ 
  mf->AddDefinition("XCODE","1");
  if(this->XcodeVersion == 15)
    {
    }
  else
    {
    mf->AddCacheDefinition(
      "CMAKE_CONFIGURATION_TYPES",
      "Debug;Release;MinSizeRel;RelWithDebInfo",
      "Semicolon separated list of supported configuration types, "
      "only supports Debug, Release, MinSizeRel, and RelWithDebInfo, "
      "anything else will be ignored.",
      cmCacheManager::STRING);
    }
  mf->AddDefinition("CMAKE_GENERATOR_CC", "gcc");
  mf->AddDefinition("CMAKE_GENERATOR_CXX", "g++");
  mf->AddDefinition("CMAKE_GENERATOR_NO_COMPILER_ENV", "1");
  // initialize Architectures so it can be used by 
  //  GetTargetObjectFileDirectories
  this->cmGlobalGenerator::EnableLanguage(lang, mf, optional);
    const char* osxArch = 
      mf->GetDefinition("CMAKE_OSX_ARCHITECTURES");
  const char* sysroot = 
      mf->GetDefinition("CMAKE_OSX_SYSROOT");
  if(osxArch && sysroot)
    {
    this->Architectures.clear();
    cmSystemTools::ExpandListArgument(std::string(osxArch),
                                      this->Architectures);
    }
}

//----------------------------------------------------------------------------
std::string cmGlobalXCodeGenerator
::GenerateBuildCommand(const char* makeProgram,
                       const char *projectName, 
                       const char* additionalOptions, 
                       const char *targetName,
                       const char* config, 
                       bool ignoreErrors,
                       bool)
{
  // Config is not used yet
  (void) ignoreErrors;

  // now build the test
  if(makeProgram == 0 || !strlen(makeProgram))
    {
    cmSystemTools::Error(
      "Generator cannot find the appropriate make command.");
    return "";
    }
  std::string makeCommand = 
    cmSystemTools::ConvertToOutputPath(makeProgram);
  std::string lowerCaseCommand = makeCommand;
  cmSystemTools::LowerCase(lowerCaseCommand);

  makeCommand += " -project ";
  makeCommand += projectName;
  makeCommand += ".xcode";
  if(this->XcodeVersion > 20)
    {
    makeCommand += "proj";
    }

  bool clean = false;
  if ( targetName && strcmp(targetName, "clean") == 0 )
    {
    clean = true;
    targetName = "ALL_BUILD";
    }
  if(clean)
    {
    makeCommand += " clean";
    }
  else
    {
    makeCommand += " build";
    }
  makeCommand += " -target ";
  // if it is a null string for config don't use it
  if(config && *config == 0)
    {
    config = 0;
    }
  if (targetName && strlen(targetName))
    {
    makeCommand += targetName;
    }
  else
    {
    makeCommand += "ALL_BUILD";
    }
  if(this->XcodeVersion == 15)
    {
    makeCommand += " -buildstyle Development ";
    }
  else
    {
    makeCommand += " -configuration ";
    makeCommand += config?config:"Debug";
    }
  if ( additionalOptions )
    {
    makeCommand += " ";
    makeCommand += additionalOptions;
    }
  return makeCommand;
}

//----------------------------------------------------------------------------
///! Create a local generator appropriate to this Global Generator
cmLocalGenerator *cmGlobalXCodeGenerator::CreateLocalGenerator()
{
  cmLocalGenerator *lg = new cmLocalXCodeGenerator;
  lg->SetGlobalGenerator(this);
  return lg;
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::Generate()
{
  std::map<cmStdString, std::vector<cmLocalGenerator*> >::iterator it;
  // make sure extra targets are added before calling
  // the parent generate which will call trace depends
  for(it = this->ProjectMap.begin(); it!= this->ProjectMap.end(); ++it)
    { 
    cmLocalGenerator* root = it->second[0];
    this->SetGenerationRoot(root);
    // add ALL_BUILD, INSTALL, etc
    this->AddExtraTargets(root, it->second);
    }
  this->cmGlobalGenerator::Generate();
  for(it = this->ProjectMap.begin(); it!= this->ProjectMap.end(); ++it)
    { 
    cmLocalGenerator* root = it->second[0];
    this->SetGenerationRoot(root);
    // now create the project
    this->OutputXCodeProject(root, it->second);
    }
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::SetGenerationRoot(cmLocalGenerator* root)
{
  this->CurrentProject = root->GetMakefile()->GetProjectName();
  this->SetCurrentLocalGenerator(root);
  std::string outDir = this->CurrentMakefile->GetHomeOutputDirectory();
  outDir =cmSystemTools::CollapseFullPath(outDir.c_str());
  cmSystemTools::SplitPath(outDir.c_str(),
                           this->ProjectOutputDirectoryComponents);

  this->CurrentXCodeHackMakefile =
    root->GetMakefile()->GetCurrentOutputDirectory();
  this->CurrentXCodeHackMakefile += "/CMakeScripts";
  cmSystemTools::MakeDirectory(this->CurrentXCodeHackMakefile.c_str());
  this->CurrentXCodeHackMakefile += "/XCODE_DEPEND_HELPER.make";
}

//----------------------------------------------------------------------------
void 
cmGlobalXCodeGenerator::AddExtraTargets(cmLocalGenerator* root, 
                                        std::vector<cmLocalGenerator*>& gens)
{
  cmMakefile* mf = root->GetMakefile();

  // Add ALL_BUILD
  const char* no_working_directory = 0;
  std::vector<std::string> no_depends;
  mf->AddUtilityCommand("ALL_BUILD", true, no_depends,
                        no_working_directory,
                        "echo", "Build all projects");
  cmTarget* allbuild = mf->FindTarget("ALL_BUILD");

  // Add XCODE depend helper 
  std::string dir = mf->GetCurrentOutputDirectory();
  cmCustomCommandLine makecommand;
  makecommand.push_back("make");
  makecommand.push_back("-C");
  makecommand.push_back(dir.c_str());
  makecommand.push_back("-f");
  makecommand.push_back(this->CurrentXCodeHackMakefile.c_str());
  if(this->XcodeVersion > 20)
    {
    makecommand.push_back("all.$(CONFIGURATION)");
    }
  cmCustomCommandLines commandLines;
  commandLines.push_back(makecommand);
  // Add Re-Run CMake rules
  this->CreateReRunCMakeFile(root);

  // now make the allbuild depend on all the non-utility targets
  // in the project
  for(std::vector<cmLocalGenerator*>::iterator i = gens.begin();
      i != gens.end(); ++i)
    {
    cmLocalGenerator* lg = *i; 
    if(this->IsExcluded(root, *i))
      {
      continue;
      }
    cmTargets& tgts = lg->GetMakefile()->GetTargets();
    for(cmTargets::iterator l = tgts.begin(); l != tgts.end(); l++)
      {
      cmTarget& target = l->second;
      // make all exe, shared libs and modules 
      // run the depend check makefile as a post build rule
      // this will make sure that when the next target is built
      // things are up-to-date
      if((target.GetType() == cmTarget::EXECUTABLE ||
          target.GetType() == cmTarget::STATIC_LIBRARY ||
          target.GetType() == cmTarget::SHARED_LIBRARY ||
          target.GetType() == cmTarget::MODULE_LIBRARY))
        {
        lg->GetMakefile()->AddCustomCommandToTarget(target.GetName(),
                                                    no_depends,
                                                    commandLines,
                                                    cmTarget::POST_BUILD,
                                                    "Depend check for xcode",
                                                    dir.c_str());
                                                    
        }
      if(!target.GetPropertyAsBool("EXCLUDE_FROM_ALL"))
        {
        allbuild->AddUtility(target.GetName());
        }
      }
    }
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::CreateReRunCMakeFile(cmLocalGenerator* root)
{
  cmMakefile* mf = root->GetMakefile();
  std::vector<std::string> lfiles = mf->GetListFiles();
  // sort the array
  std::sort(lfiles.begin(), lfiles.end(), std::less<std::string>()); 
  std::vector<std::string>::iterator new_end = 
    std::unique(lfiles.begin(), lfiles.end());
  lfiles.erase(new_end, lfiles.end());
  std::string dir = mf->GetHomeOutputDirectory();
  this->CurrentReRunCMakeMakefile = dir;
  this->CurrentReRunCMakeMakefile += "/CMakeScripts";
  cmSystemTools::MakeDirectory(this->CurrentReRunCMakeMakefile.c_str());
  this->CurrentReRunCMakeMakefile += "/ReRunCMake.make";
  cmGeneratedFileStream makefileStream
    (this->CurrentReRunCMakeMakefile.c_str());
  makefileStream.SetCopyIfDifferent(true);
  makefileStream << "# Generated by CMake, DO NOT EDIT\n";
  makefileStream << cmake::GetCMakeFilesDirectoryPostSlash();
  makefileStream << "cmake.check_cache: ";
  for(std::vector<std::string>::const_iterator i = lfiles.begin();
      i !=  lfiles.end(); ++i)
    {
    makefileStream << "\\\n" << this->ConvertToRelativeForMake(i->c_str());
    }
  std::string cmake = mf->GetRequiredDefinition("CMAKE_COMMAND");
  makefileStream << "\n\t" << this->ConvertToRelativeForMake(cmake.c_str()) 
                 << " -H" << this->ConvertToRelativeForMake(
                   mf->GetHomeDirectory())
                 << " -B" << this->ConvertToRelativeForMake(
                   mf->GetHomeOutputDirectory()) << "\n";
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::ClearXCodeObjects()
{
  this->TargetDoneSet.clear();
  for(unsigned int i = 0; i < this->XCodeObjects.size(); ++i)
    {
    delete this->XCodeObjects[i];
    }
  this->XCodeObjects.clear();
  this->GroupMap.clear();
  this->GroupNameMap.clear();
  this->TargetGroup.clear();
  this->FileRefs.clear();
}

//----------------------------------------------------------------------------
cmXCodeObject* 
cmGlobalXCodeGenerator::CreateObject(cmXCodeObject::PBXType ptype)
{
  cmXCodeObject* obj;
  if(this->XcodeVersion == 15)
    {
    obj = new cmXCodeObject(ptype, cmXCodeObject::OBJECT);
    }
  else
    {
    obj = new cmXCode21Object(ptype, cmXCodeObject::OBJECT);
    }
  this->XCodeObjects.push_back(obj);
  return obj;
}

//----------------------------------------------------------------------------
cmXCodeObject* 
cmGlobalXCodeGenerator::CreateObject(cmXCodeObject::Type type)
{
  cmXCodeObject* obj = new cmXCodeObject(cmXCodeObject::None, type);
  this->XCodeObjects.push_back(obj);
  return obj;
}

//----------------------------------------------------------------------------
cmXCodeObject*
cmGlobalXCodeGenerator::CreateString(const char* s)
{
  cmXCodeObject* obj = this->CreateObject(cmXCodeObject::STRING);
  obj->SetString(s);
  return obj;
}

//----------------------------------------------------------------------------
cmXCodeObject* cmGlobalXCodeGenerator
::CreateObjectReference(cmXCodeObject* ref)
{
  cmXCodeObject* obj = this->CreateObject(cmXCodeObject::OBJECT_REF);
  obj->SetObject(ref);
  return obj;
}

//----------------------------------------------------------------------------
cmStdString GetGroupMapKey(cmTarget& cmtarget, cmSourceFile* sf)
{
  cmStdString key(cmtarget.GetName());
  key += "-";
  key += sf->GetFullPath();
  return key;
}

//----------------------------------------------------------------------------
cmXCodeObject*
cmGlobalXCodeGenerator::CreateXCodeSourceFile(cmLocalGenerator* lg,
                                              cmSourceFile* sf,
                                              cmTarget& cmtarget)
{
  // Add flags from target and source file properties.
  std::string flags;
  if(cmtarget.GetProperty("COMPILE_FLAGS"))
    {
    lg->AppendFlags(flags, cmtarget.GetProperty("COMPILE_FLAGS"));
    }
  lg->AppendFlags(flags, sf->GetProperty("COMPILE_FLAGS"));

  // Add per-source definitions.
  this->AppendDefines(flags, sf->GetProperty("COMPILE_DEFINITIONS"), true);

  // Using a map and the full path guarantees that we will always get the same
  // fileRef object for any given full path.
  //
  std::string fname = sf->GetFullPath();
  cmXCodeObject* fileRef = this->FileRefs[fname];
  if(!fileRef)
    {
    fileRef = this->CreateObject(cmXCodeObject::PBXFileReference);
    std::string comment = fname;
    comment += " in ";
    //std::string gname = group->GetObject("name")->GetString();
    //comment += gname.substr(1, gname.size()-2);
    fileRef->SetComment(fname.c_str());

    this->FileRefs[fname] = fileRef;
    }

  cmStdString key = GetGroupMapKey(cmtarget, sf);
  cmXCodeObject* group = this->GroupMap[key];
  cmXCodeObject* children = group->GetObject("children");
  if (!children->HasObject(fileRef))
    {
    children->AddObject(fileRef);
    }

  cmXCodeObject* buildFile = this->CreateObject(cmXCodeObject::PBXBuildFile);
  buildFile->SetComment(fileRef->GetComment());
  buildFile->AddAttribute("fileRef", this->CreateObjectReference(fileRef));

  cmXCodeObject* settings = 
    this->CreateObject(cmXCodeObject::ATTRIBUTE_GROUP);
  settings->AddAttribute("COMPILER_FLAGS", this->CreateString(flags.c_str()));

  // Is this a resource file in this target? Add it to the resources group...
  //
  cmTarget::SourceFileFlags tsFlags = cmtarget.GetTargetSourceFileFlags(sf);
  bool isResource = (tsFlags.Type == cmTarget::SourceFileTypeResource);

  // Is this a "private" or "public" framework header file?
  // Set the ATTRIBUTES attribute appropriately...
  //
  if(cmtarget.IsFrameworkOnApple())
    {
    if(tsFlags.Type == cmTarget::SourceFileTypePrivateHeader)
      {
      cmXCodeObject* attrs = this->CreateObject(cmXCodeObject::OBJECT_LIST);
      attrs->AddObject(this->CreateString("Private"));
      settings->AddAttribute("ATTRIBUTES", attrs);
      isResource = true;
      }
    else if(tsFlags.Type == cmTarget::SourceFileTypePublicHeader)
      {
      cmXCodeObject* attrs = this->CreateObject(cmXCodeObject::OBJECT_LIST);
      attrs->AddObject(this->CreateString("Public"));
      settings->AddAttribute("ATTRIBUTES", attrs);
      isResource = true;
      }
    }

  // Add the fileRef to the top level Resources group/folder if it is not
  // already there.
  //
  if(isResource && this->ResourcesGroupChildren &&
    !this->ResourcesGroupChildren->HasObject(fileRef))
    {
    this->ResourcesGroupChildren->AddObject(fileRef);
    }

  buildFile->AddAttribute("settings", settings);
  fileRef->AddAttribute("fileEncoding", this->CreateString("4"));
  const char* lang = 
    this->CurrentLocalGenerator->GetSourceFileLanguage(*sf);
  std::string sourcecode = "sourcecode";
  std::string ext = sf->GetExtension();
  ext = cmSystemTools::LowerCase(ext);
  if(ext == "o")
    {
    sourcecode = "compiled.mach-o.objfile";
    }
  else if(ext == "mm")
    {
    sourcecode += ".cpp.objcpp";
    }
  else if(ext == "m")
    {
    sourcecode += ".cpp.objc";
    }
  else if(ext == "plist")
    {
    sourcecode += ".text.plist";
    }
  else if(!lang)
    {
    sourcecode += ext;
    sourcecode += ".";
    sourcecode += ext;
    }
  else if(strcmp(lang, "C") == 0)
    {
    sourcecode += ".c.c";
    }
  else if(strcmp(lang, "CXX") == 0)
    {
    sourcecode += ".cpp.cpp";
    }
  else
    {
    sourcecode += ext;
    sourcecode += ".";
    sourcecode += ext;
    }
  fileRef->AddAttribute("lastKnownFileType", 
                        this->CreateString(sourcecode.c_str()));
  std::string path = 
    this->ConvertToRelativeForXCode(sf->GetFullPath().c_str());
  std::string dir;
  std::string file;
  cmSystemTools::SplitProgramPath(sf->GetFullPath().c_str(),
                                  dir, file);
  
  fileRef->AddAttribute("name", this->CreateString(file.c_str()));
  fileRef->AddAttribute("path", this->CreateString(path.c_str()));
  if(this->XcodeVersion == 15)
    {
    fileRef->AddAttribute("refType", this->CreateString("4"));
    }
  if(path.size() > 1 && path[0] == '.' && path[1] == '.')
    {
    fileRef->AddAttribute("sourceTree", this->CreateString("<group>"));
    }
  else
    {
    fileRef->AddAttribute("sourceTree", this->CreateString("<absolute>"));
    }
  return buildFile;
}

//----------------------------------------------------------------------------
bool cmGlobalXCodeGenerator::SpecialTargetEmitted(std::string const& tname)
{
  if(tname == "ALL_BUILD" || tname == "XCODE_DEPEND_HELPER" ||
     tname == "install" || tname == "package" || tname == "RUN_TESTS" )
    {
    if(this->TargetDoneSet.find(tname) != this->TargetDoneSet.end())
      {
      return true;
      }
    this->TargetDoneSet.insert(tname);
    return false;
    }
  return false;
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::SetCurrentLocalGenerator(cmLocalGenerator* gen)
{
  this->CurrentLocalGenerator = gen;
  this->CurrentMakefile = gen->GetMakefile();
  std::string outdir =
    cmSystemTools::CollapseFullPath(this->CurrentMakefile->
                                    GetCurrentOutputDirectory());
  cmSystemTools::SplitPath(outdir.c_str(), 
                           this->CurrentOutputDirectoryComponents);

  // Select the current set of configuration types.
  this->CurrentConfigurationTypes.clear();
  if(this->XcodeVersion > 20)
    {
    if(const char* types =
       this->CurrentMakefile->GetDefinition("CMAKE_CONFIGURATION_TYPES"))
      {
      cmSystemTools::ExpandListArgument(types, 
                                        this->CurrentConfigurationTypes);
      }
    }
  if(this->CurrentConfigurationTypes.empty())
    {
    if(const char* buildType =
       this->CurrentMakefile->GetDefinition("CMAKE_BUILD_TYPE"))
      {
      this->CurrentConfigurationTypes.push_back(buildType);
      }
    else
      {
      this->CurrentConfigurationTypes.push_back("");
      }
    }
}

//----------------------------------------------------------------------------
void
cmGlobalXCodeGenerator::CreateXCodeTargets(cmLocalGenerator* gen,
                                           std::vector<cmXCodeObject*>&
                                           targets)
{
  this->SetCurrentLocalGenerator(gen);
  cmTargets &tgts = this->CurrentMakefile->GetTargets();
  for(cmTargets::iterator l = tgts.begin(); l != tgts.end(); l++)
    {
    cmTarget& cmtarget = l->second;

    // make sure ALL_BUILD, INSTALL, etc are only done once
    if(this->SpecialTargetEmitted(l->first.c_str()))
      {
      continue;
      }

    if(cmtarget.GetType() == cmTarget::UTILITY ||
       cmtarget.GetType() == cmTarget::GLOBAL_TARGET)
      {
      targets.push_back(this->CreateUtilityTarget(cmtarget));
      continue;
      }

    // organize the sources
    std::vector<cmSourceFile*> const &classes = cmtarget.GetSourceFiles();
    std::vector<cmXCodeObject*> externalObjFiles;
    std::vector<cmXCodeObject*> headerFiles;
    std::vector<cmXCodeObject*> resourceFiles;
    std::vector<cmXCodeObject*> sourceFiles;
    for(std::vector<cmSourceFile*>::const_iterator i = classes.begin();
        i != classes.end(); ++i)
      {
      cmXCodeObject* xsf =
        this->CreateXCodeSourceFile(this->CurrentLocalGenerator,
                                    *i, cmtarget);
      cmXCodeObject* fr = xsf->GetObject("fileRef");
      cmXCodeObject* filetype = 
        fr->GetObject()->GetObject("lastKnownFileType");

      cmTarget::SourceFileFlags tsFlags =
        cmtarget.GetTargetSourceFileFlags(*i);

      if(strcmp(filetype->GetString(), "\"compiled.mach-o.objfile\"") == 0)
        {
        externalObjFiles.push_back(xsf);
        }
      else if((*i)->GetPropertyAsBool("HEADER_FILE_ONLY"))
        {
        headerFiles.push_back(xsf);
        }
      else if(tsFlags.Type == cmTarget::SourceFileTypeResource)
        {
        resourceFiles.push_back(xsf);
        }
      else
        {
        // Include this file in the build if it has a known language
        // and has not been listed as an ignored extension for this
        // generator.
        if(this->CurrentLocalGenerator->GetSourceFileLanguage(**i) &&
           !this->IgnoreFile((*i)->GetExtension().c_str()))
          {
          sourceFiles.push_back(xsf);
          }
        }
      }

    // some build phases only apply to bundles and/or frameworks
    bool isFrameworkTarget = cmtarget.IsFrameworkOnApple();
    bool isBundleTarget = cmtarget.GetPropertyAsBool("MACOSX_BUNDLE");

    cmXCodeObject* buildFiles = 0;

    // create source build phase
    cmXCodeObject* sourceBuildPhase = 0;
    if (!sourceFiles.empty())
      {
      sourceBuildPhase =
        this->CreateObject(cmXCodeObject::PBXSourcesBuildPhase);
      sourceBuildPhase->SetComment("Sources");
      sourceBuildPhase->AddAttribute("buildActionMask", 
                                     this->CreateString("2147483647"));
      buildFiles = this->CreateObject(cmXCodeObject::OBJECT_LIST);
      for(std::vector<cmXCodeObject*>::iterator i = sourceFiles.begin();
          i != sourceFiles.end(); ++i)
        {
        buildFiles->AddObject(*i);
        }
      sourceBuildPhase->AddAttribute("files", buildFiles);
      sourceBuildPhase->AddAttribute("runOnlyForDeploymentPostprocessing", 
                                     this->CreateString("0"));
      }

    // create header build phase - only for framework targets
    cmXCodeObject* headerBuildPhase = 0;
    if (!headerFiles.empty() && isFrameworkTarget)
      {
      headerBuildPhase =
        this->CreateObject(cmXCodeObject::PBXHeadersBuildPhase);
      headerBuildPhase->SetComment("Headers");
      headerBuildPhase->AddAttribute("buildActionMask",
                                     this->CreateString("2147483647"));
      buildFiles = this->CreateObject(cmXCodeObject::OBJECT_LIST);
      for(std::vector<cmXCodeObject*>::iterator i = headerFiles.begin();
          i != headerFiles.end(); ++i)
        {
        buildFiles->AddObject(*i);
        }
      headerBuildPhase->AddAttribute("files", buildFiles);
      headerBuildPhase->AddAttribute("runOnlyForDeploymentPostprocessing",
                                     this->CreateString("0"));
      }

    // create resource build phase - only for framework or bundle targets
    cmXCodeObject* resourceBuildPhase = 0;
    if (!resourceFiles.empty() && (isFrameworkTarget || isBundleTarget))
      {
      resourceBuildPhase =
        this->CreateObject(cmXCodeObject::PBXResourcesBuildPhase);
      resourceBuildPhase->SetComment("Resources");
      resourceBuildPhase->AddAttribute("buildActionMask",
                                       this->CreateString("2147483647"));
      buildFiles = this->CreateObject(cmXCodeObject::OBJECT_LIST);
      for(std::vector<cmXCodeObject*>::iterator i = resourceFiles.begin();
          i != resourceFiles.end(); ++i)
        {
        buildFiles->AddObject(*i);
        }
      resourceBuildPhase->AddAttribute("files", buildFiles);
      resourceBuildPhase->AddAttribute("runOnlyForDeploymentPostprocessing",
                                     this->CreateString("0"));
      }

    // create vector of "non-resource content file" build phases - only for
    // framework or bundle targets
    std::vector<cmXCodeObject*> contentBuildPhases;
    if (isFrameworkTarget || isBundleTarget)
      {
      typedef std::map<cmStdString, std::vector<cmSourceFile*> >
        mapOfVectorOfSourceFiles;
      mapOfVectorOfSourceFiles bundleFiles;
      for(std::vector<cmSourceFile*>::const_iterator i = classes.begin();
          i != classes.end(); ++i)
        {
        cmTarget::SourceFileFlags tsFlags =
          cmtarget.GetTargetSourceFileFlags(*i);
        if(tsFlags.Type == cmTarget::SourceFileTypeMacContent)
          {
          bundleFiles[tsFlags.MacFolder].push_back(*i);
          }
        }
      mapOfVectorOfSourceFiles::iterator mit;
      for ( mit = bundleFiles.begin(); mit != bundleFiles.end(); ++ mit )
        {
        cmXCodeObject* copyFilesBuildPhase =
          this->CreateObject(cmXCodeObject::PBXCopyFilesBuildPhase);
        copyFilesBuildPhase->SetComment("Copy files");
        copyFilesBuildPhase->AddAttribute("buildActionMask",
          this->CreateString("2147483647"));
        copyFilesBuildPhase->AddAttribute("dstSubfolderSpec",
          this->CreateString("6"));
        cmOStringStream ostr;
        if (cmtarget.IsFrameworkOnApple())
          {
          // dstPath in frameworks is relative to Versions/<version>
          ostr << mit->first;
          }
        else if ( mit->first != "MacOS" )
          {
          // dstPath in bundles is relative to Contents/MacOS
          ostr << "../" << mit->first.c_str();
          }
        copyFilesBuildPhase->AddAttribute("dstPath",
          this->CreateString(ostr.str().c_str()));
        copyFilesBuildPhase->AddAttribute(
          "runOnlyForDeploymentPostprocessing", this->CreateString("0"));
        buildFiles = this->CreateObject(cmXCodeObject::OBJECT_LIST);
        copyFilesBuildPhase->AddAttribute("files", buildFiles);
        std::vector<cmSourceFile*>::iterator sfIt;
        for ( sfIt = mit->second.begin(); sfIt != mit->second.end(); ++ sfIt )
          {
          cmXCodeObject* xsf =
            this->CreateXCodeSourceFile(this->CurrentLocalGenerator, 
                                        *sfIt, cmtarget);
          buildFiles->AddObject(xsf);
          }
        contentBuildPhases.push_back(copyFilesBuildPhase);
        }
      }

    // create framework build phase
    cmXCodeObject* frameworkBuildPhase = 0;
    if (!externalObjFiles.empty())
      {
      frameworkBuildPhase =
        this->CreateObject(cmXCodeObject::PBXFrameworksBuildPhase);
      frameworkBuildPhase->SetComment("Frameworks");
      frameworkBuildPhase->AddAttribute("buildActionMask",
                                        this->CreateString("2147483647"));
      buildFiles = this->CreateObject(cmXCodeObject::OBJECT_LIST);
      frameworkBuildPhase->AddAttribute("files", buildFiles);
      for(std::vector<cmXCodeObject*>::iterator i =  externalObjFiles.begin();
          i != externalObjFiles.end(); ++i)
        {
        buildFiles->AddObject(*i);
        }
      frameworkBuildPhase->AddAttribute("runOnlyForDeploymentPostprocessing", 
                                        this->CreateString("0"));
      }

    // create list of build phases and create the XCode target
    cmXCodeObject* buildPhases = 
      this->CreateObject(cmXCodeObject::OBJECT_LIST);

    this->CreateCustomCommands(buildPhases, sourceBuildPhase,
                               headerBuildPhase, resourceBuildPhase,
                               contentBuildPhases,
                               frameworkBuildPhase, cmtarget);

    targets.push_back(this->CreateXCodeTarget(cmtarget, buildPhases));
    }
}

//----------------------------------------------------------------------------
cmXCodeObject*
cmGlobalXCodeGenerator::CreateBuildPhase(const char* name,
                                         const char* name2,
                                         cmTarget& cmtarget,
                                         const std::vector<cmCustomCommand>&
                                         commands)
{
  if(commands.size() == 0 && strcmp(name, "CMake ReRun") != 0)
    {
    return 0;
    }
  cmXCodeObject* buildPhase = 
    this->CreateObject(cmXCodeObject::PBXShellScriptBuildPhase);
  buildPhase->AddAttribute("buildActionMask",
                           this->CreateString("2147483647"));
  cmXCodeObject* buildFiles = this->CreateObject(cmXCodeObject::OBJECT_LIST);
  buildPhase->AddAttribute("files", buildFiles);
  buildPhase->AddAttribute("name", 
                           this->CreateString(name));
  buildPhase->AddAttribute("runOnlyForDeploymentPostprocessing", 
                           this->CreateString("0"));
  buildPhase->AddAttribute("shellPath",
                           this->CreateString("/bin/sh"));
  this->AddCommandsToBuildPhase(buildPhase, cmtarget, commands,
                                name2);
  return buildPhase;
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::CreateCustomCommands(cmXCodeObject* buildPhases,
                                                  cmXCodeObject*
                                                  sourceBuildPhase,
                                                  cmXCodeObject*
                                                  headerBuildPhase,
                                                  cmXCodeObject*
                                                  resourceBuildPhase,
                                                  std::vector<cmXCodeObject*>
                                                  contentBuildPhases,
                                                  cmXCodeObject*
                                                  frameworkBuildPhase,
                                                  cmTarget& cmtarget)
{
  std::vector<cmCustomCommand> const & prebuild 
    = cmtarget.GetPreBuildCommands();
  std::vector<cmCustomCommand> const & prelink 
    = cmtarget.GetPreLinkCommands();
  std::vector<cmCustomCommand> const & postbuild 
    = cmtarget.GetPostBuildCommands();
  std::vector<cmSourceFile*>const &classes = cmtarget.GetSourceFiles();
  // add all the sources
  std::vector<cmCustomCommand> commands;
  for(std::vector<cmSourceFile*>::const_iterator i = classes.begin(); 
      i != classes.end(); ++i)
    {
    if((*i)->GetCustomCommand())
      {
      commands.push_back(*(*i)->GetCustomCommand());
      }
    }
  std::vector<cmCustomCommand> reruncom;
  cmXCodeObject* cmakeReRunPhase =  
    this->CreateBuildPhase("CMake ReRun", "cmakeReRunPhase",
                           cmtarget, reruncom);
  buildPhases->AddObject(cmakeReRunPhase);
  // create prebuild phase
  cmXCodeObject* cmakeRulesBuildPhase =
    this->CreateBuildPhase("CMake Rules",
                           "cmakeRulesBuildPhase",
                           cmtarget, commands);
  // create prebuild phase
  cmXCodeObject* preBuildPhase = 
    this->CreateBuildPhase("CMake PreBuild Rules", "preBuildCommands",
                           cmtarget, prebuild);
  // create prelink phase
  cmXCodeObject* preLinkPhase = 
    this->CreateBuildPhase("CMake PreLink Rules", "preLinkCommands",
                           cmtarget, prelink);
  // create postbuild phase
  cmXCodeObject* postBuildPhase = 
    this->CreateBuildPhase("CMake PostBuild Rules", "postBuildPhase",
                           cmtarget, postbuild);

  // The order here is the order they will be built in.
  // The order "headers, resources, sources" mimics a native project generated
  // from an xcode template...
  //
  if(preBuildPhase)
    {
    buildPhases->AddObject(preBuildPhase);
    }
  if(cmakeRulesBuildPhase)
    {
    buildPhases->AddObject(cmakeRulesBuildPhase);
    }
  if(headerBuildPhase)
    {
    buildPhases->AddObject(headerBuildPhase);
    }
  if(resourceBuildPhase)
    {
    buildPhases->AddObject(resourceBuildPhase);
    }
  std::vector<cmXCodeObject*>::iterator cit;
  for (cit = contentBuildPhases.begin(); cit != contentBuildPhases.end();
       ++cit)
    {
    buildPhases->AddObject(*cit);
    }
  if(sourceBuildPhase)
    {
    buildPhases->AddObject(sourceBuildPhase);
    }
  if(preLinkPhase)
    {
    buildPhases->AddObject(preLinkPhase);
    }
  if(frameworkBuildPhase)
    {
    buildPhases->AddObject(frameworkBuildPhase);
    }
  if(postBuildPhase)
    {
    buildPhases->AddObject(postBuildPhase);
    }
}

//----------------------------------------------------------------------------
std::string cmGlobalXCodeGenerator::ExtractFlag(const char* flag,
                                                std::string& flags)
{
  std::string retFlag;
  std::string::size_type pos = flags.find(flag);
  if(pos != flags.npos && (pos ==0 || flags[pos]==' '))
    {
    while(pos < flags.size() && flags[pos] != ' ')
      {
      retFlag += flags[pos];
      flags[pos] = ' ';
      pos++;
      }
    }
  return retFlag;
}

//----------------------------------------------------------------------------
void 
cmGlobalXCodeGenerator::AddCommandsToBuildPhase(cmXCodeObject* buildphase,
                                                cmTarget& target,
                                                std::vector<cmCustomCommand> 
                                                const & commands,
                                                const char* name)
{
  if(strcmp(name, "cmakeReRunPhase") == 0)
    {
    std::string cdir = this->CurrentMakefile->GetHomeOutputDirectory();
    cdir = this->ConvertToRelativeForMake(cdir.c_str());
    std::string makecmd = "make -C ";
    makecmd += cdir;
    makecmd += " -f ";
    makecmd += 
      this->ConvertToRelativeForMake(this->CurrentReRunCMakeMakefile.c_str());
    cmSystemTools::ReplaceString(makecmd, "\\ ", "\\\\ ");
    buildphase->AddAttribute("shellScript",
                             this->CreateString(makecmd.c_str()));
    return;
    }

  // collect multiple outputs of custom commands into a set
  // which will be used for every configuration
  std::map<cmStdString, cmStdString> multipleOutputPairs;
  for(std::vector<cmCustomCommand>::const_iterator i = commands.begin();
      i != commands.end(); ++i)
    {
    cmCustomCommand const& cc = *i; 
    if(!cc.GetCommandLines().empty())
      {
      const std::vector<std::string>& outputs = cc.GetOutputs();
      if(!outputs.empty())
        {
        // If there are more than one outputs treat the
        // first as the primary output and make the rest depend on it.
        std::vector<std::string>::const_iterator o = outputs.begin();
        std::string primaryOutput = this->ConvertToRelativeForMake(o->c_str());
        for(++o; o != outputs.end(); ++o)
          {
          std::string currentOutput=this->ConvertToRelativeForMake(o->c_str());
          multipleOutputPairs[currentOutput] = primaryOutput;
          }
        }
      }
    }
    
  std::string dir = this->CurrentMakefile->GetCurrentOutputDirectory();
  dir += "/CMakeScripts";
  cmSystemTools::MakeDirectory(dir.c_str());
  std::string makefile = dir;
  makefile += "/";
  makefile += target.GetName();
  makefile += "_";
  makefile += name;
  makefile += ".make";
  
  for (std::vector<std::string>::const_iterator currentConfig= 
            this->CurrentConfigurationTypes.begin();
       currentConfig!=this->CurrentConfigurationTypes.end();
       currentConfig++ )
    {
    this->CreateCustomRulesMakefile(makefile.c_str(), 
                                    target, 
                                    commands, 
                                    currentConfig->c_str(),
                                    multipleOutputPairs);
    }
  
  std::string cdir = this->CurrentMakefile->GetCurrentOutputDirectory();
  cdir = this->ConvertToRelativeForXCode(cdir.c_str());
  std::string makecmd = "make -C ";
  makecmd += cdir;
  makecmd += " -f ";
  makecmd += this->ConvertToRelativeForMake(
                                          (makefile+"$CONFIGURATION").c_str());
  if(!multipleOutputPairs.empty())
    {
    makecmd += " cmake_check_multiple_outputs";
    }
  makecmd += " all";
  cmSystemTools::ReplaceString(makecmd, "\\ ", "\\\\ ");
  buildphase->AddAttribute("shellScript", 
                           this->CreateString(makecmd.c_str()));
}

//----------------------------------------------------------------------------
void  cmGlobalXCodeGenerator
::CreateCustomRulesMakefile(const char* makefileBasename, 
                            cmTarget& target, 
                            std::vector<cmCustomCommand> 
                            const & commands,
                            const char* configName,
                            const std::map<cmStdString, 
                            cmStdString>& multipleOutputPairs
                           )
{
  std::string makefileName=makefileBasename;
  makefileName+=configName;
  cmGeneratedFileStream makefileStream(makefileName.c_str());
  if(!makefileStream)
    {
    return;
    }
  makefileStream.SetCopyIfDifferent(true);
  makefileStream << "# Generated by CMake, DO NOT EDIT\n";
  makefileStream << "# Custom rules for " << target.GetName() << "\n";
  
  // have all depend on all outputs
  makefileStream << "all: ";
  std::map<const cmCustomCommand*, cmStdString> tname;
  int count = 0;
  for(std::vector<cmCustomCommand>::const_iterator i = commands.begin();
      i != commands.end(); ++i)
    {
    cmCustomCommand const& cc = *i; 
    if(!cc.GetCommandLines().empty())
      {
      const std::vector<std::string>& outputs = cc.GetOutputs();
      if(!outputs.empty())
        {
        for(std::vector<std::string>::const_iterator o = outputs.begin();
            o != outputs.end(); ++o)
          {
          makefileStream
              << "\\\n\t" << this->ConvertToRelativeForMake(o->c_str());
          }
        }
      else
        {  
        cmOStringStream str;
        str << "_buildpart_" << count++ ;
        tname[&cc] = std::string(target.GetName()) + str.str(); 
        makefileStream << "\\\n\t" << tname[&cc];
        }
      }
    }
  makefileStream << "\n\n";
  for(std::vector<cmCustomCommand>::const_iterator i = commands.begin();
      i != commands.end(); ++i)
    {
    cmCustomCommand const& cc = *i; 
    if(!cc.GetCommandLines().empty())
      {
      bool escapeOldStyle = cc.GetEscapeOldStyle();
      bool escapeAllowMakeVars = cc.GetEscapeAllowMakeVars();
      makefileStream << "\n";
      const std::vector<std::string>& outputs = cc.GetOutputs();
      if(!outputs.empty())
        {
        // There is at least one output, start the rule for it
        std::string primary_output =
            this->ConvertToRelativeForMake(outputs.begin()->c_str());
        makefileStream << primary_output << ": ";
        }
      else
        {
        // There are no outputs.  Use the generated force rule name.
        makefileStream << tname[&cc] << ": ";
        }
      for(std::vector<std::string>::const_iterator d = 
          cc.GetDepends().begin();
          d != cc.GetDepends().end(); ++d)
        {
        std::string dep =
          this->CurrentLocalGenerator->GetRealDependency(d->c_str(),
                                                         configName);
        makefileStream << "\\\n" << this
          ->ConvertToRelativeForMake(dep.c_str());
        }
      makefileStream << "\n";

      if(const char* comment = cc.GetComment())
        {
        std::string echo_cmd = "echo ";
        echo_cmd += (this->CurrentLocalGenerator->
                     EscapeForShell(comment, escapeAllowMakeVars));
        makefileStream << "\t" << echo_cmd.c_str() << "\n";
        }

      // Add each command line to the set of commands.
      for(cmCustomCommandLines::const_iterator cl = 
          cc.GetCommandLines().begin();
          cl != cc.GetCommandLines().end(); ++cl)
        {
        // Build the command line in a single string.
        const cmCustomCommandLine& commandLine = *cl;
        std::string cmd2 = this->CurrentLocalGenerator
                         ->GetRealLocation(commandLine[0].c_str(), configName);
        
        cmSystemTools::ReplaceString(cmd2, "/./", "/");
        cmd2 = this->ConvertToRelativeForMake(cmd2.c_str());
        std::string cmd;
        if(cc.GetWorkingDirectory())
          {
          cmd += "cd ";
          cmd += this->ConvertToRelativeForMake(cc.GetWorkingDirectory());
          cmd += " && ";
          }
        cmd += cmd2;
        for(unsigned int j=1; j < commandLine.size(); ++j)
          {
          cmd += " ";
          if(escapeOldStyle)
            {
            cmd += (this->CurrentLocalGenerator
                ->EscapeForShellOldStyle(commandLine[j].c_str()));
            }
          else
            {
            cmd += (this->CurrentLocalGenerator->
                EscapeForShell(commandLine[j].c_str(),
                               escapeAllowMakeVars));
            }
          }
        makefileStream << "\t" << cmd.c_str() << "\n";
        }
      }
    }

  // Add rules to deal with multiple outputs of custom commands.
  if(!multipleOutputPairs.empty())
    {
    makefileStream << 
        "\n# Dependencies of multiple outputs to their primary outputs \n";

    for(std::map<cmStdString, cmStdString>::const_iterator o =
        multipleOutputPairs.begin(); o != multipleOutputPairs.end(); ++o)
      {
      makefileStream << o->first << ": " << o->second << "\n";
      }

    makefileStream <<
        "\n"
        "cmake_check_multiple_outputs:\n";
    for(std::map<cmStdString, cmStdString>::const_iterator o =
        multipleOutputPairs.begin(); o != multipleOutputPairs.end(); ++o)
      {
      makefileStream << "\t@if [ ! -f "
          << o->first << " ]; then rm -f "
          << o->second << "; fi\n";
      }
    }
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::CreateBuildSettings(cmTarget& target,
                                                 cmXCodeObject* buildSettings,
                                                 std::string& fileType,
                                                 std::string& productType,
                                                 std::string& productName,
                                                 const char* configName)
{
  std::string flags;
  std::string defFlags;
  bool shared = ((target.GetType() == cmTarget::SHARED_LIBRARY) ||
                 (target.GetType() == cmTarget::MODULE_LIBRARY));

  const char* lang = target.GetLinkerLanguage(this);
  std::string cflags;
  if(lang)
    {
    // for c++ projects get the c flags as well
    if(strcmp(lang, "CXX") == 0)
      {
      this->CurrentLocalGenerator->AddLanguageFlags(cflags, "C", configName);
      this->CurrentLocalGenerator->AddSharedFlags(cflags, lang, shared);
      }

    // Add language-specific flags.
    this->CurrentLocalGenerator->AddLanguageFlags(flags, lang, configName);
    
    // Add shared-library flags if needed.
    this->CurrentLocalGenerator->AddSharedFlags(flags, lang, shared);
    }

  // Add define flags
  this->CurrentLocalGenerator->
    AppendFlags(defFlags,
                this->CurrentMakefile->GetDefineFlags());

  // Add preprocessor definitions for this target and configuration.
  std::string ppDefs;
  if(this->XcodeVersion > 15)
    {
    this->AppendDefines(ppDefs, "CMAKE_INTDIR=\"$(CONFIGURATION)\"");
    }
  if(const char* exportMacro = target.GetExportMacro())
    {
    // Add the export symbol definition for shared library objects.
    this->AppendDefines(ppDefs, exportMacro);
    }
  this->AppendDefines
    (ppDefs, this->CurrentMakefile->GetProperty("COMPILE_DEFINITIONS"));
  this->AppendDefines(ppDefs, target.GetProperty("COMPILE_DEFINITIONS"));
  if(configName)
    {
    std::string defVarName = "COMPILE_DEFINITIONS_";
    defVarName += cmSystemTools::UpperCase(configName);
    this->AppendDefines
      (ppDefs, this->CurrentMakefile->GetProperty(defVarName.c_str()));
    this->AppendDefines(ppDefs, target.GetProperty(defVarName.c_str()));
    }
  buildSettings->AddAttribute
    ("GCC_PREPROCESSOR_DEFINITIONS", this->CreateString(ppDefs.c_str()));

  std::string extraLinkOptions;
  if(target.GetType() == cmTarget::EXECUTABLE)
    {
    extraLinkOptions = 
      this->CurrentMakefile->GetRequiredDefinition("CMAKE_EXE_LINKER_FLAGS");
    }
  if(target.GetType() == cmTarget::SHARED_LIBRARY)
    {
    extraLinkOptions = this->CurrentMakefile->
      GetRequiredDefinition("CMAKE_SHARED_LINKER_FLAGS");
    }
  if(target.GetType() == cmTarget::MODULE_LIBRARY)
    {
    extraLinkOptions = this->CurrentMakefile->
      GetRequiredDefinition("CMAKE_MODULE_LINKER_FLAGS");
    }
  
  const char* targetLinkFlags = target.GetProperty("LINK_FLAGS");
  if(targetLinkFlags)
    {
    extraLinkOptions += " ";
    extraLinkOptions += targetLinkFlags;
    }

  // The product name is the full name of the target for this configuration.
  productName = target.GetFullName(configName);

  // Get the product name components.
  std::string pnprefix;
  std::string pnbase;
  std::string pnsuffix;
  target.GetFullNameComponents(pnprefix, pnbase, pnsuffix, configName);

  // Store the product name for all target types.
  buildSettings->AddAttribute("PRODUCT_NAME",
                              this->CreateString(pnbase.c_str()));

  // Set attributes to specify the proper name for the target.
  if(target.GetType() == cmTarget::STATIC_LIBRARY ||
     target.GetType() == cmTarget::SHARED_LIBRARY ||
     target.GetType() == cmTarget::MODULE_LIBRARY ||
     target.GetType() == cmTarget::EXECUTABLE)
    {
    std::string pndir = target.GetDirectory();
    buildSettings->AddAttribute("SYMROOT", 
                                this->CreateString(pndir.c_str()));
    buildSettings->AddAttribute("EXECUTABLE_PREFIX", 
                                this->CreateString(pnprefix.c_str()));
    buildSettings->AddAttribute("EXECUTABLE_SUFFIX", 
                                this->CreateString(pnsuffix.c_str()));
    }

  // Handle settings for each target type.
  switch(target.GetType())
    {
    case cmTarget::STATIC_LIBRARY:
    {
    fileType = "archive.ar";
    productType = "com.apple.product-type.library.static";

    buildSettings->AddAttribute("LIBRARY_STYLE", 
                                this->CreateString("STATIC"));
    break;
    }
    
    case cmTarget::MODULE_LIBRARY:
    {
    buildSettings->AddAttribute("LIBRARY_STYLE", 
                                this->CreateString("BUNDLE"));
    if(this->XcodeVersion >= 22)
      {
      fileType = "compiled.mach-o.executable";
      productType = "com.apple.product-type.tool";

      buildSettings->AddAttribute("MACH_O_TYPE", 
                                  this->CreateString("mh_bundle"));
      buildSettings->AddAttribute("GCC_DYNAMIC_NO_PIC", 
                                  this->CreateString("NO"));
      // Add the flags to create an executable.
      std::string createFlags =
        this->LookupFlags("CMAKE_", lang, "_LINK_FLAGS", "");
      if(!createFlags.empty())
        {
        extraLinkOptions += " ";
        extraLinkOptions += createFlags;
        }
      }
    else
      {
      fileType = "compiled.mach-o.dylib";
      productType = "com.apple.product-type.library.dynamic";

      // Add the flags to create a module.
      std::string createFlags =
        this->LookupFlags("CMAKE_SHARED_MODULE_CREATE_", lang, "_FLAGS",
                          "-bundle");
      if(!createFlags.empty())
        {
        extraLinkOptions += " ";
        extraLinkOptions += createFlags;
        }
      }
    break;
    }
    case cmTarget::SHARED_LIBRARY:
    {
    if(target.GetPropertyAsBool("FRAMEWORK"))
      {
      fileType = "wrapper.framework";
      productType = "com.apple.product-type.framework";

      std::string version = target.GetFrameworkVersion();
      buildSettings->AddAttribute("FRAMEWORK_VERSION",
                                  this->CreateString(version.c_str()));
      }
    else
      {
      fileType = "compiled.mach-o.dylib";
      productType = "com.apple.product-type.library.dynamic";

      // Add the flags to create a shared library.
      std::string createFlags =
        this->LookupFlags("CMAKE_SHARED_LIBRARY_CREATE_", lang, "_FLAGS",
                          "-dynamiclib");
      if(!createFlags.empty())
        {
        extraLinkOptions += " ";
        extraLinkOptions += createFlags;
        }
      }

    buildSettings->AddAttribute("LIBRARY_STYLE",
                                this->CreateString("DYNAMIC"));
    buildSettings->AddAttribute("DYLIB_COMPATIBILITY_VERSION",
                                this->CreateString("1"));
    buildSettings->AddAttribute("DYLIB_CURRENT_VERSION",
                                this->CreateString("1"));
    break;
    }
    case cmTarget::EXECUTABLE:
    {
    fileType = "compiled.mach-o.executable";

    // Add the flags to create an executable.
    std::string createFlags =
      this->LookupFlags("CMAKE_", lang, "_LINK_FLAGS", "");
    if(!createFlags.empty())
      {
      extraLinkOptions += " ";
      extraLinkOptions += createFlags;
      }

    // Handle bundles and normal executables separately.
    if(target.GetPropertyAsBool("MACOSX_BUNDLE"))
      {
      productType = "com.apple.product-type.application";
      std::string plist = this->ComputeInfoPListLocation(target);
      // Xcode will create the final version of Info.plist at build time,
      // so let it replace the executable name.  This avoids creating
      // a per-configuration Info.plist file.
      this->CurrentLocalGenerator
        ->GenerateAppleInfoPList(&target, "$(EXECUTABLE_NAME)",
                                 plist.c_str());
      std::string path =
        this->ConvertToRelativeForXCode(plist.c_str());
      buildSettings->AddAttribute("INFOPLIST_FILE", 
                                  this->CreateString(path.c_str()));

      }
    else
      {
      productType = "com.apple.product-type.tool";
      }
    }
    break;
    default:
      break;
    }
  if(this->XcodeVersion >= 22)
    {
    buildSettings->AddAttribute("PREBINDING", 
                                this->CreateString("NO"));
    }
  std::string dirs;
  std::vector<std::string> includes;
  this->CurrentLocalGenerator->GetIncludeDirectories(includes);
  std::string fdirs;
  std::set<cmStdString> emitted;
  emitted.insert("/System/Library/Frameworks");
  for(std::vector<std::string>::iterator i = includes.begin();
      i != includes.end(); ++i)
    {
    if(this->NameResolvesToFramework(i->c_str()))
      {
      std::string frameworkDir = *i;
      frameworkDir += "/../";
      frameworkDir = cmSystemTools::CollapseFullPath(frameworkDir.c_str());
      if(emitted.insert(frameworkDir).second)
        {
        fdirs += this->XCodeEscapePath(frameworkDir.c_str());
        fdirs += " ";
        }
      }
    else
      {
      std::string incpath = 
        this->XCodeEscapePath(i->c_str());
      dirs += incpath + " ";
      }
    }
  std::vector<std::string>& frameworks = target.GetFrameworks();
  if(frameworks.size())
    {
    for(std::vector<std::string>::iterator fmIt = frameworks.begin();
        fmIt != frameworks.end(); ++fmIt)
      {
      if(emitted.insert(*fmIt).second)
        {
        fdirs += this->XCodeEscapePath(fmIt->c_str());
        fdirs += " ";
        }
      }
    }
  if(fdirs.size())
    {
    buildSettings->AddAttribute("FRAMEWORK_SEARCH_PATHS", 
                                this->CreateString(fdirs.c_str()));
    }
  if(dirs.size())
    {
    buildSettings->AddAttribute("HEADER_SEARCH_PATHS", 
                                this->CreateString(dirs.c_str()));
    }
  std::string oflagc = this->ExtractFlag("-O", cflags);
  char optLevel[2];
  optLevel[0] = '0';
  optLevel[1] = 0;
  if(oflagc.size() == 3)
    {
    optLevel[0] = oflagc[2];
    }
  if(oflagc.size() == 2)
    {
    optLevel[0] = '1';
    }
  std::string oflag = this->ExtractFlag("-O", flags);
  if(oflag.size() == 3)
    {
    optLevel[0] = oflag[2];
    }
  if(oflag.size() == 2)
    {
    optLevel[0] = '1';
    }
  std::string gflagc = this->ExtractFlag("-g", cflags);
  // put back gdwarf-2 if used since there is no way
  // to represent it in the gui, but we still want debug yes
  if(gflagc == "-gdwarf-2")
    {
    cflags += " ";
    cflags += gflagc;
    }
  std::string gflag = this->ExtractFlag("-g", flags);
  if(gflag == "-gdwarf-2")
    {
    flags += " ";
    flags += gflag;
    }
  const char* debugStr = "YES";
  if(gflagc.size() ==0  && gflag.size() == 0)
    {
    debugStr = "NO";
    }    

  // Convert "XCODE_ATTRIBUTE_*" properties directly.
  {
  cmPropertyMap const& props = target.GetProperties();
  for(cmPropertyMap::const_iterator i = props.begin();
      i != props.end(); ++i)
    {
    if(i->first.find("XCODE_ATTRIBUTE_") == 0)
      {
      buildSettings->AddAttribute(i->first.substr(16).c_str(),
                                  this->CreateString(i->second.GetValue()));
      }
    }
  }

  buildSettings->AddAttribute("GCC_GENERATE_DEBUGGING_SYMBOLS",
                              this->CreateString(debugStr));
  buildSettings->AddAttribute("GCC_OPTIMIZATION_LEVEL", 
                              this->CreateString(optLevel));
  buildSettings->AddAttribute("OPTIMIZATION_CFLAGS", 
                              this->CreateString(oflagc.c_str()));
  buildSettings->AddAttribute("GCC_SYMBOLS_PRIVATE_EXTERN",
                              this->CreateString("NO"));
  buildSettings->AddAttribute("GCC_INLINES_ARE_PRIVATE_EXTERN",
                              this->CreateString("NO"));
  if(lang && strcmp(lang, "CXX") == 0)
    {
    flags += " ";
    flags += defFlags;
    buildSettings->AddAttribute("OTHER_CPLUSPLUSFLAGS", 
                                this->CreateString(flags.c_str()));
    cflags += " ";
    cflags += defFlags;
    buildSettings->AddAttribute("OTHER_CFLAGS", 
                                this->CreateString(cflags.c_str()));

    }
  else
    {
    flags += " ";
    flags += defFlags;
    buildSettings->AddAttribute("OTHER_CFLAGS", 
                                this->CreateString(flags.c_str()));
    }

  // Create the INSTALL_PATH attribute.
  std::string install_name_dir;
  if(target.GetType() == cmTarget::SHARED_LIBRARY)
    {
    // Get the install_name directory for the build tree.
    install_name_dir = target.GetInstallNameDirForBuildTree(configName, true);
    if(install_name_dir.empty())
      {
      // Xcode will not pass the -install_name option at all if INSTALL_PATH
      // is not given or is empty.  We must explicitly put the flag in the
      // link flags to create an install_name with just the library soname.
      extraLinkOptions += " -install_name ";
      extraLinkOptions += productName;
      }
    else
      {
      // Convert to a path for the native build tool.
      cmSystemTools::ConvertToUnixSlashes(install_name_dir);
      // do not escape spaces on this since it is only a single path
      }
    }
  buildSettings->AddAttribute("INSTALL_PATH",
                              this->CreateString(install_name_dir.c_str()));

  buildSettings->AddAttribute("OTHER_LDFLAGS", 
                              this->CreateString(extraLinkOptions.c_str()));
  buildSettings->AddAttribute("OTHER_REZFLAGS", 
                              this->CreateString(""));
  buildSettings->AddAttribute("SECTORDER_FLAGS",
                              this->CreateString(""));
  buildSettings->AddAttribute("USE_HEADERMAP",
                              this->CreateString("NO"));
  buildSettings->AddAttribute("WARNING_CFLAGS",
                              this->CreateString(
                                "-Wmost -Wno-four-char-constants"
                                " -Wno-unknown-pragmas"));
}

//----------------------------------------------------------------------------
cmXCodeObject* 
cmGlobalXCodeGenerator::CreateUtilityTarget(cmTarget& cmtarget)
{
  cmXCodeObject* shellBuildPhase =
    this->CreateObject(cmXCodeObject::PBXShellScriptBuildPhase);
  shellBuildPhase->AddAttribute("buildActionMask", 
                                this->CreateString("2147483647"));
  cmXCodeObject* buildFiles = this->CreateObject(cmXCodeObject::OBJECT_LIST);
  shellBuildPhase->AddAttribute("files", buildFiles);
  cmXCodeObject* inputPaths = this->CreateObject(cmXCodeObject::OBJECT_LIST);
  shellBuildPhase->AddAttribute("inputPaths", inputPaths);
  cmXCodeObject* outputPaths = this->CreateObject(cmXCodeObject::OBJECT_LIST);
  shellBuildPhase->AddAttribute("outputPaths", outputPaths);
  shellBuildPhase->AddAttribute("runOnlyForDeploymentPostprocessing",
                                this->CreateString("0"));
  shellBuildPhase->AddAttribute("shellPath",
                                this->CreateString("/bin/sh"));
  shellBuildPhase->AddAttribute("shellScript",
                                this->CreateString(
                                  "# shell script goes here\nexit 0"));
  cmXCodeObject* target = 
    this->CreateObject(cmXCodeObject::PBXAggregateTarget);
  target->SetComment(cmtarget.GetName());
  cmXCodeObject* buildPhases = 
    this->CreateObject(cmXCodeObject::OBJECT_LIST);
  std::vector<cmXCodeObject*> emptyContentVector;
  this->CreateCustomCommands(buildPhases, 0, 0, 0, emptyContentVector, 0,
                             cmtarget);
  target->AddAttribute("buildPhases", buildPhases);
  cmXCodeObject* buildSettings =
    this->CreateObject(cmXCodeObject::ATTRIBUTE_GROUP);
  std::string fileTypeString;
  std::string productTypeString;
  std::string productName;
  const char* globalConfig = 0;
  if(this->XcodeVersion > 20)
    {
    this->AddConfigurations(target, cmtarget);
    }
  else
    {
    globalConfig = this->CurrentMakefile->GetDefinition("CMAKE_BUILD_TYPE");  
    }
  this->CreateBuildSettings(cmtarget, 
                            buildSettings, fileTypeString, 
                            productTypeString, productName, globalConfig);
  target->AddAttribute("buildSettings", buildSettings);
  cmXCodeObject* dependencies = 
    this->CreateObject(cmXCodeObject::OBJECT_LIST);
  target->AddAttribute("dependencies", dependencies);
  target->AddAttribute("name", this->CreateString(productName.c_str()));
  target->AddAttribute("productName",this->CreateString(productName.c_str()));
  target->SetTarget(&cmtarget);
  return target;
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::AddConfigurations(cmXCodeObject* target,
                                               cmTarget& cmtarget)
{
  std::string configTypes = 
    this->CurrentMakefile->GetRequiredDefinition("CMAKE_CONFIGURATION_TYPES");
  std::vector<std::string> configVectorIn;
  std::vector<std::string> configVector;
  configVectorIn.push_back(configTypes);
  cmSystemTools::ExpandList(configVectorIn, configVector);
  cmXCodeObject* configlist = 
    this->CreateObject(cmXCodeObject::XCConfigurationList);
  cmXCodeObject* buildConfigurations =
    this->CreateObject(cmXCodeObject::OBJECT_LIST);
  configlist->AddAttribute("buildConfigurations", buildConfigurations);
  std::string comment = "Build configuration list for ";
  comment += cmXCodeObject::PBXTypeNames[target->GetIsA()];
  comment += " \"";
  comment += cmtarget.GetName();
  comment += "\"";
  configlist->SetComment(comment.c_str());
  target->AddAttribute("buildConfigurationList", 
                       this->CreateObjectReference(configlist));
  for(unsigned int i = 0; i < configVector.size(); ++i)
    {
    cmXCodeObject* config = 
      this->CreateObject(cmXCodeObject::XCBuildConfiguration);
    buildConfigurations->AddObject(config);
    cmXCodeObject* buildSettings =
      this->CreateObject(cmXCodeObject::ATTRIBUTE_GROUP);
    std::string fileTypeString;
    std::string productTypeString;
    std::string productName;
    this->CreateBuildSettings(cmtarget, 
                              buildSettings, fileTypeString, 
                              productTypeString, productName,
                              configVector[i].c_str());
    config->AddAttribute("name", this->CreateString(configVector[i].c_str()));
    config->SetComment(configVector[i].c_str());
    config->AddAttribute("buildSettings", buildSettings);
    }
  if(configVector.size())
    {
    configlist->AddAttribute("defaultConfigurationName", 
                             this->CreateString(configVector[0].c_str()));
    configlist->AddAttribute("defaultConfigurationIsVisible", 
                             this->CreateString("0"));
    }
}

//----------------------------------------------------------------------------
cmXCodeObject*
cmGlobalXCodeGenerator::CreateXCodeTarget(cmTarget& cmtarget,
                                          cmXCodeObject* buildPhases)
{
  cmXCodeObject* target = 
    this->CreateObject(cmXCodeObject::PBXNativeTarget);
  target->AddAttribute("buildPhases", buildPhases);
  cmXCodeObject* buildRules = this->CreateObject(cmXCodeObject::OBJECT_LIST);
  target->AddAttribute("buildRules", buildRules);
  cmXCodeObject* buildSettings =
    this->CreateObject(cmXCodeObject::ATTRIBUTE_GROUP);
  std::string fileTypeString;
  std::string productTypeString;
  std::string productName;
  const char* globalConfig = 0;
  if(this->XcodeVersion > 20)
    {
    this->AddConfigurations(target, cmtarget);
    }
  else
    {
    globalConfig = this->CurrentMakefile->GetDefinition("CMAKE_BUILD_TYPE");  
    }
  this->CreateBuildSettings(cmtarget, 
                            buildSettings, fileTypeString, 
                            productTypeString, productName, globalConfig);
  target->AddAttribute("buildSettings", buildSettings);
  cmXCodeObject* dependencies = 
    this->CreateObject(cmXCodeObject::OBJECT_LIST);
  target->AddAttribute("dependencies", dependencies);
  target->AddAttribute("name", this->CreateString(productName.c_str()));
  target->AddAttribute("productName",this->CreateString(productName.c_str()));

  cmXCodeObject* fileRef = 
    this->CreateObject(cmXCodeObject::PBXFileReference);
  fileRef->AddAttribute("explicitFileType", 
                        this->CreateString(fileTypeString.c_str()));
  fileRef->AddAttribute("path", this->CreateString(productName.c_str()));
  fileRef->AddAttribute("refType", this->CreateString("0"));
  fileRef->AddAttribute("sourceTree",
                        this->CreateString("BUILT_PRODUCTS_DIR"));
  fileRef->SetComment(cmtarget.GetName());
  target->AddAttribute("productReference", 
                       this->CreateObjectReference(fileRef));
  target->AddAttribute("productType", 
                       this->CreateString(productTypeString.c_str()));
  target->SetTarget(&cmtarget);
  return target;
}

//----------------------------------------------------------------------------
cmXCodeObject* cmGlobalXCodeGenerator::FindXCodeTarget(cmTarget* t)
{
  if(!t)
    {
    return 0;
    }
  for(std::vector<cmXCodeObject*>::iterator i = this->XCodeObjects.begin();
      i != this->XCodeObjects.end(); ++i)
    {
    cmXCodeObject* o = *i;
    if(o->GetTarget() == t)
      {
      return o;
      }
    }
  return 0;
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::AddDependTarget(cmXCodeObject* target,
                                             cmXCodeObject* dependTarget)
{
  // make sure a target does not depend on itself
  if(target == dependTarget)
    {
    return;
    }
  // now avoid circular references if dependTarget already
  // depends on target then skip it.  Circular references crashes
  // xcode
  cmXCodeObject* dependTargetDepends = 
    dependTarget->GetObject("dependencies");
  if(dependTargetDepends)
    {
    if(dependTargetDepends->HasObject(target->GetPBXTargetDependency()))
      { 
      return;
      }
    }
  
  cmXCodeObject* targetdep = dependTarget->GetPBXTargetDependency();
  if(!targetdep)
    {
    cmXCodeObject* container = 
      this->CreateObject(cmXCodeObject::PBXContainerItemProxy);
    container->SetComment("PBXContainerItemProxy");
    container->AddAttribute("containerPortal",
                            this->CreateObjectReference(this->RootObject));
    container->AddAttribute("proxyType", this->CreateString("1"));
    container->AddAttribute("remoteGlobalIDString",
                            this->CreateObjectReference(dependTarget));
    container->AddAttribute("remoteInfo", 
                            this->CreateString(
                              dependTarget->GetTarget()->GetName()));
    targetdep = 
      this->CreateObject(cmXCodeObject::PBXTargetDependency);
    targetdep->SetComment("PBXTargetDependency");
    targetdep->AddAttribute("target",
                            this->CreateObjectReference(dependTarget));
    targetdep->AddAttribute("targetProxy", 
                            this->CreateObjectReference(container));
    dependTarget->SetPBXTargetDependency(targetdep);
    }
    
  cmXCodeObject* depends = target->GetObject("dependencies");
  if(!depends)
    {
    cmSystemTools::
      Error("target does not have dependencies attribute error..");
    
    }
  else
    {
    depends->AddUniqueObject(targetdep);
    }
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::AppendOrAddBuildSetting(cmXCodeObject* settings,
                                                     const char* attribute,
                                                     const char* value)
{
  if(settings)
    {
    cmXCodeObject* attr = settings->GetObject(attribute);
    if(!attr)
      {
      settings->AddAttribute(attribute, this->CreateString(value));
      }
    else
      {
      std::string oldValue = attr->GetString();

      // unescape escaped quotes internal to the string:
      cmSystemTools::ReplaceString(oldValue, "\\\"", "\"");

      // remove surrounding quotes, if any:
      std::string::size_type len = oldValue.length();
      if(oldValue[0] == '\"' && oldValue[len-1] == '\"')
        {
        oldValue = oldValue.substr(1, len-2);
        }

      oldValue += " ";
      oldValue += value;

      // SetString automatically escapes internal quotes and then surrounds
      // the result with quotes if necessary...
      attr->SetString(oldValue.c_str());
      }
    }
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator
::AppendBuildSettingAttribute(cmXCodeObject* target,
                              const char* attribute,
                              const char* value,
                              const char* configName)
{
  if(this->XcodeVersion < 21)
    {
    // There is only one configuration.  Add the setting to the buildSettings
    // of the target.
    this->AppendOrAddBuildSetting(target->GetObject("buildSettings"),
                                  attribute, value);
    }
  else
    {
    // There are multiple configurations.  Add the setting to the
    // buildSettings of the configuration name given.
    cmXCodeObject* configurationList = 
      target->GetObject("buildConfigurationList")->GetObject();
    cmXCodeObject* buildConfigs = 
      configurationList->GetObject("buildConfigurations");
    std::vector<cmXCodeObject*> list = buildConfigs->GetObjectList();
    // each configuration and the target itself has a buildSettings in it 
    //list.push_back(target);
    for(std::vector<cmXCodeObject*>::iterator i = list.begin(); 
        i != list.end(); ++i)
      {
      if(configName)
        {
        if(strcmp((*i)->GetObject("name")->GetString(), configName) == 0)
          {
          cmXCodeObject* settings = (*i)->GetObject("buildSettings");
          this->AppendOrAddBuildSetting(settings, attribute, value);
          }
        }
      else
        {
        cmXCodeObject* settings = (*i)->GetObject("buildSettings");
        this->AppendOrAddBuildSetting(settings, attribute, value);
        }
      }
    }
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator
::AddDependAndLinkInformation(cmXCodeObject* target)
{
  cmTarget* cmtarget = target->GetTarget();
  if(!cmtarget)
    {
    cmSystemTools::Error("Error no target on xobject\n");
    return;
    }

  // Add dependencies on other CMake targets.
  {
  // Keep track of dependencies already listed.
  std::set<cmStdString> emitted;

  // A target should not depend on itself.
  emitted.insert(cmtarget->GetName());

  // Loop over all library dependencies.
  const cmTarget::LinkLibraryVectorType& tlibs = 
    cmtarget->GetLinkLibraries();
  for(cmTarget::LinkLibraryVectorType::const_iterator lib = tlibs.begin();
      lib != tlibs.end(); ++lib)
    {
    // Don't emit the same library twice for this target.
    if(emitted.insert(lib->first).second)
      {
      // Add this dependency.
      cmTarget* t = this->FindTarget(this->CurrentProject.c_str(),
                                     lib->first.c_str());
      cmXCodeObject* dptarget = this->FindXCodeTarget(t);
      if(dptarget)
        {
        this->AddDependTarget(target, dptarget);
        }
      }
    }
  }
  
  // write utility dependencies.
  for(std::set<cmStdString>::const_iterator i
        = cmtarget->GetUtilities().begin();
      i != cmtarget->GetUtilities().end(); ++i)
    {
    cmTarget* t = this->FindTarget(this->CurrentProject.c_str(),
                                   i->c_str());
    // if the target is in this project then make target depend
    // on it.  It may not be in this project if this is a sub
    // project from the top.
    if(t)
      {
      cmXCodeObject* dptarget = this->FindXCodeTarget(t);
      if(dptarget)
        {
        this->AddDependTarget(target, dptarget);
        }
      else
        {
        std::string m = "Error Utility: ";
        m += i->c_str();
        m += "\n";
        m += "cmtarget ";
        if(t)
          {
          m += t->GetName();
          }
        m += "\n";
        m += "Is on the target ";
        m += cmtarget->GetName();
        m += "\n";
        m += "But it has no xcode target created yet??\n";
        m += "Current project is ";
        m += this->CurrentProject.c_str();
        cmSystemTools::Error(m.c_str());
        }
      } 
    }

  // Skip link information for static libraries.
  if(cmtarget->GetType() == cmTarget::STATIC_LIBRARY)
    {
    return;
    }

  // Loop over configuration types and set per-configuration info.
  for(std::vector<std::string>::iterator i =
        this->CurrentConfigurationTypes.begin();
      i != this->CurrentConfigurationTypes.end(); ++i)
    {
    // Get the current configuration name.
    const char* configName = i->c_str();
    if(!*configName)
      {
      configName = 0;
      }

    // Compute the link library and directory information.
    cmComputeLinkInformation* pcli = cmtarget->GetLinkInformation(configName);
    if(!pcli)
      {
      continue;
      }
    cmComputeLinkInformation& cli = *pcli;

    // Add dependencies directly on library files.
    {
    std::vector<std::string> const& libDeps = cli.GetDepends();
    for(std::vector<std::string>::const_iterator j = libDeps.begin();
        j != libDeps.end(); ++j)
      {
      target->AddDependLibrary(configName, j->c_str());
      }
    }

    // add the library search paths
    {
    std::vector<std::string> const& libDirs = cli.GetDirectories();
    std::string linkDirs;
    for(std::vector<std::string>::const_iterator libDir = libDirs.begin();
        libDir != libDirs.end(); ++libDir)
      {
      if(libDir->size() && *libDir != "/usr/lib")
        {
        if(this->XcodeVersion > 15)
          {
          // now add the same one but append $(CONFIGURATION) to it:
          linkDirs += " ";
          linkDirs += this->XCodeEscapePath(
            (*libDir + "/$(CONFIGURATION)").c_str());
          }
        linkDirs += " ";
        linkDirs += this->XCodeEscapePath(libDir->c_str());
        }
      }
    this->AppendBuildSettingAttribute(target, "LIBRARY_SEARCH_PATHS",
                                      linkDirs.c_str(), configName);
    }

    // add the framework search paths
    {
    const char* sep = "";
    std::string fdirs;
    std::vector<std::string> const& fwDirs = cli.GetFrameworkPaths();
    for(std::vector<std::string>::const_iterator fdi = fwDirs.begin();
        fdi != fwDirs.end(); ++fdi)
      {
      fdirs += sep;
      sep = " ";
      fdirs += this->XCodeEscapePath(fdi->c_str());
      }
    if(!fdirs.empty())
      {
      this->AppendBuildSettingAttribute(target, "FRAMEWORK_SEARCH_PATHS",
                                        fdirs.c_str(), configName);
      }
    }

    // now add the link libraries
    {
    std::string linkLibs;
    const char* sep = "";
    typedef cmComputeLinkInformation::ItemVector ItemVector;
    ItemVector const& libNames = cli.GetItems();
    for(ItemVector::const_iterator li = libNames.begin();
        li != libNames.end(); ++li)
      {
      linkLibs += sep;
      sep = " ";
      if(li->IsPath)
        {
        linkLibs += this->XCodeEscapePath(li->Value.c_str());
        }
      else
        {
        linkLibs += li->Value;
        }
      }
    this->AppendBuildSettingAttribute(target, "OTHER_LDFLAGS",
                                      linkLibs.c_str(), configName);
    }
    }
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::CreateGroups(cmLocalGenerator* root,
                                          std::vector<cmLocalGenerator*>&
                                          generators)
{
  for(std::vector<cmLocalGenerator*>::iterator i = generators.begin();
      i != generators.end(); ++i)
    {
    if(this->IsExcluded(root, *i))
      {
      continue;
      }
    cmMakefile* mf = (*i)->GetMakefile();
    std::vector<cmSourceGroup> sourceGroups = mf->GetSourceGroups();
    cmTargets &tgts = mf->GetTargets();
    for(cmTargets::iterator l = tgts.begin(); l != tgts.end(); l++)
      { 
      cmTarget& cmtarget = l->second;

      // Same skipping logic here as in CreateXCodeTargets so that we do not
      // end up with (empty anyhow) ALL_BUILD and XCODE_DEPEND_HELPER source
      // groups:
      //
      if(cmtarget.GetType() == cmTarget::UTILITY ||
         cmtarget.GetType() == cmTarget::GLOBAL_TARGET)
        {
        continue;
        }

      // add the soon to be generated Info.plist file as a source for a
      // MACOSX_BUNDLE file
      if(cmtarget.GetPropertyAsBool("MACOSX_BUNDLE"))
        {
        std::string plist = this->ComputeInfoPListLocation(cmtarget);
        cmSourceFile* sf = mf->GetOrCreateSource(plist.c_str(), true);
        cmtarget.AddSourceFile(sf);
        }

      std::vector<cmSourceFile*>  classes = cmtarget.GetSourceFiles();

      for(std::vector<cmSourceFile*>::const_iterator s = classes.begin(); 
          s != classes.end(); s++)
        {
        cmSourceFile* sf = *s;
        // Add the file to the list of sources.
        std::string const& source = sf->GetFullPath();
        cmSourceGroup& sourceGroup = 
          mf->FindSourceGroup(source.c_str(), sourceGroups);
        cmXCodeObject* pbxgroup = 
          this->CreateOrGetPBXGroup(cmtarget, &sourceGroup);
        cmStdString key = GetGroupMapKey(cmtarget, sf);
        this->GroupMap[key] = pbxgroup;
        }
      }
    } 
}

//----------------------------------------------------------------------------
cmXCodeObject* cmGlobalXCodeGenerator
::CreateOrGetPBXGroup(cmTarget& cmtarget, cmSourceGroup* sg)
{
  cmStdString s = cmtarget.GetName();
  s += "/";
  s += sg->GetName();
  std::map<cmStdString, cmXCodeObject* >::iterator i =
    this->GroupNameMap.find(s);
  if(i != this->GroupNameMap.end())
    {
    return i->second;
    }
  i = this->TargetGroup.find(cmtarget.GetName());
  cmXCodeObject* tgroup = 0;
  if(i != this->TargetGroup.end())
    {
    tgroup = i->second;
    }
  else
    {
    tgroup = this->CreateObject(cmXCodeObject::PBXGroup);
    this->TargetGroup[cmtarget.GetName()] = tgroup;
    cmXCodeObject* tgroupChildren = 
      this->CreateObject(cmXCodeObject::OBJECT_LIST);
    tgroup->AddAttribute("name", this->CreateString(cmtarget.GetName()));
    tgroup->AddAttribute("children", tgroupChildren);
    if(this->XcodeVersion == 15)
      {
      tgroup->AddAttribute("refType", this->CreateString("4"));
      }
    tgroup->AddAttribute("sourceTree", this->CreateString("<group>"));
    this->SourcesGroupChildren->AddObject(tgroup);
    }

  // If it's the default source group (empty name) then put the source file
  // directly in the tgroup...
  //
  if (cmStdString(sg->GetName()) == "")
    {
    this->GroupNameMap[s] = tgroup;
    return tgroup;
    }

  cmXCodeObject* tgroupChildren = tgroup->GetObject("children");
  cmXCodeObject* group = this->CreateObject(cmXCodeObject::PBXGroup);
  cmXCodeObject* groupChildren = 
    this->CreateObject(cmXCodeObject::OBJECT_LIST);
  group->AddAttribute("name", this->CreateString(sg->GetName()));
  group->AddAttribute("children", groupChildren);
  if(this->XcodeVersion == 15)
    {
    group->AddAttribute("refType", this->CreateString("4"));
    }
  group->AddAttribute("sourceTree", this->CreateString("<group>"));
  tgroupChildren->AddObject(group);
  this->GroupNameMap[s] = group;
  return group;
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator
::CreateXCodeObjects(cmLocalGenerator* root,
                     std::vector<cmLocalGenerator*>&
                     generators)
{
  this->ClearXCodeObjects(); 
  this->RootObject = 0;
  this->SourcesGroupChildren = 0;
  this->ResourcesGroupChildren = 0;
  this->MainGroupChildren = 0;
  cmXCodeObject* group = this->CreateObject(cmXCodeObject::ATTRIBUTE_GROUP);
  group->AddAttribute("COPY_PHASE_STRIP", this->CreateString("NO"));
  cmXCodeObject* developBuildStyle = 
    this->CreateObject(cmXCodeObject::PBXBuildStyle);
  cmXCodeObject* listObjs = this->CreateObject(cmXCodeObject::OBJECT_LIST);
  if(this->XcodeVersion == 15)
    {
    developBuildStyle->AddAttribute("name", 
                                    this->CreateString("Development"));
    developBuildStyle->AddAttribute("buildSettings", group);
    listObjs->AddObject(developBuildStyle);
    group = this->CreateObject(cmXCodeObject::ATTRIBUTE_GROUP);
    group->AddAttribute("COPY_PHASE_STRIP", this->CreateString("YES"));
    cmXCodeObject* deployBuildStyle =
    this->CreateObject(cmXCodeObject::PBXBuildStyle);
    deployBuildStyle->AddAttribute("name", this->CreateString("Deployment"));
    deployBuildStyle->AddAttribute("buildSettings", group);
    listObjs->AddObject(deployBuildStyle);
    }
  else
    {
    for(unsigned int i = 0; i < this->CurrentConfigurationTypes.size(); ++i)
      {
      cmXCodeObject* buildStyle = 
        this->CreateObject(cmXCodeObject::PBXBuildStyle);
      const char* name = this->CurrentConfigurationTypes[i].c_str();
      buildStyle->AddAttribute("name", this->CreateString(name));
      buildStyle->SetComment(name);
      cmXCodeObject* sgroup =
        this->CreateObject(cmXCodeObject::ATTRIBUTE_GROUP);
      sgroup->AddAttribute("COPY_PHASE_STRIP", this->CreateString("NO"));
      buildStyle->AddAttribute("buildSettings", sgroup);
      listObjs->AddObject(buildStyle);
      }
    }

  cmXCodeObject* mainGroup = this->CreateObject(cmXCodeObject::PBXGroup);
  this->MainGroupChildren = 
    this->CreateObject(cmXCodeObject::OBJECT_LIST);
  mainGroup->AddAttribute("children", this->MainGroupChildren);
  if(this->XcodeVersion == 15)
    {
    mainGroup->AddAttribute("refType", this->CreateString("4"));
    }
  mainGroup->AddAttribute("sourceTree", this->CreateString("<group>"));

  cmXCodeObject* sourcesGroup = this->CreateObject(cmXCodeObject::PBXGroup);
  this->SourcesGroupChildren = 
    this->CreateObject(cmXCodeObject::OBJECT_LIST);
  sourcesGroup->AddAttribute("name", this->CreateString("Sources"));
  sourcesGroup->AddAttribute("children", this->SourcesGroupChildren);
  if(this->XcodeVersion == 15)
    {
    sourcesGroup->AddAttribute("refType", this->CreateString("4"));
    }
  sourcesGroup->AddAttribute("sourceTree", this->CreateString("<group>"));
  this->MainGroupChildren->AddObject(sourcesGroup);

  cmXCodeObject* resourcesGroup = this->CreateObject(cmXCodeObject::PBXGroup);
  this->ResourcesGroupChildren =
    this->CreateObject(cmXCodeObject::OBJECT_LIST);
  resourcesGroup->AddAttribute("name", this->CreateString("Resources"));
  resourcesGroup->AddAttribute("children", this->ResourcesGroupChildren);
  if(this->XcodeVersion == 15)
    {
    resourcesGroup->AddAttribute("refType", this->CreateString("4"));
    }
  resourcesGroup->AddAttribute("sourceTree", this->CreateString("<group>"));
  this->MainGroupChildren->AddObject(resourcesGroup);

  // now create the cmake groups 
  this->CreateGroups(root, generators);

  cmXCodeObject* productGroup = this->CreateObject(cmXCodeObject::PBXGroup);
  productGroup->AddAttribute("name", this->CreateString("Products"));
  if(this->XcodeVersion == 15)
    {
    productGroup->AddAttribute("refType", this->CreateString("4"));
    }
  productGroup->AddAttribute("sourceTree", this->CreateString("<group>"));
  cmXCodeObject* productGroupChildren = 
    this->CreateObject(cmXCodeObject::OBJECT_LIST);
  productGroup->AddAttribute("children", productGroupChildren);
  this->MainGroupChildren->AddObject(productGroup);
  
  
  this->RootObject = this->CreateObject(cmXCodeObject::PBXProject);
  this->RootObject->SetComment("Project object");
  group = this->CreateObject(cmXCodeObject::ATTRIBUTE_GROUP);
  this->RootObject->AddAttribute("mainGroup", 
                             this->CreateObjectReference(mainGroup));
  this->RootObject->AddAttribute("buildSettings", group);
  this->RootObject->AddAttribute("buildStyles", listObjs);
  this->RootObject->AddAttribute("hasScannedForEncodings",
                             this->CreateString("0"));
  // Point Xcode at the top of the source tree.
  {
  std::string proot = root->GetMakefile()->GetCurrentDirectory();
  proot = this->ConvertToRelativeForXCode(proot.c_str());
  this->RootObject->AddAttribute("projectRoot",
                                 this->CreateString(proot.c_str()));
  }
  cmXCodeObject* configlist = 
    this->CreateObject(cmXCodeObject::XCConfigurationList);
  cmXCodeObject* buildConfigurations =
    this->CreateObject(cmXCodeObject::OBJECT_LIST);
  std::vector<cmXCodeObject*> configs;
  if(this->XcodeVersion == 15)
    {
    cmXCodeObject* configDebug = 
      this->CreateObject(cmXCodeObject::XCBuildConfiguration);
    configDebug->AddAttribute("name", this->CreateString("Debug"));
    configs.push_back(configDebug);
    cmXCodeObject* configRelease = 
      this->CreateObject(cmXCodeObject::XCBuildConfiguration);
    configRelease->AddAttribute("name", this->CreateString("Release"));
    configs.push_back(configRelease);
    }
  else
    {
    for(unsigned int i = 0; i < this->CurrentConfigurationTypes.size(); ++i)
      {
      const char* name = this->CurrentConfigurationTypes[i].c_str();
      cmXCodeObject* config = 
        this->CreateObject(cmXCodeObject::XCBuildConfiguration);
      config->AddAttribute("name", this->CreateString(name));
      configs.push_back(config);
      }
    }
  for(std::vector<cmXCodeObject*>::iterator c = configs.begin();
      c != configs.end(); ++c)
    {
    buildConfigurations->AddObject(*c);
    }
  configlist->AddAttribute("buildConfigurations", buildConfigurations);

  std::string comment = "Build configuration list for PBXProject ";
  comment += " \"";
  comment += this->CurrentProject;
  comment += "\"";
  configlist->SetComment(comment.c_str());
  configlist->AddAttribute("defaultConfigurationIsVisible", 
                           this->CreateString("0"));
  configlist->AddAttribute("defaultConfigurationName", 
                           this->CreateString("Debug"));
  cmXCodeObject* buildSettings =
      this->CreateObject(cmXCodeObject::ATTRIBUTE_GROUP);
  const char* osxArch = 
      this->CurrentMakefile->GetDefinition("CMAKE_OSX_ARCHITECTURES");
  const char* sysroot = 
      this->CurrentMakefile->GetDefinition("CMAKE_OSX_SYSROOT");
  const char* sysrootDefault = 
    this->CurrentMakefile->GetDefinition("CMAKE_OSX_SYSROOT_DEFAULT");
  if(osxArch && sysroot)
    {
    bool flagsUsed = false;
    // recompute this as it may have been changed since enable language
    this->Architectures.clear();
    cmSystemTools::ExpandListArgument(std::string(osxArch),
                                      this->Architectures);
    bool addArchFlag = true;
    if(this->Architectures.size() == 1)
      {
      const char* archOrig = 
        this->
        CurrentMakefile->GetSafeDefinition("CMAKE_OSX_ARCHITECTURES_DEFAULT");
      if(this->Architectures[0] == archOrig)
        {
        addArchFlag = false;
        }
      }
    if(addArchFlag)
      {
      flagsUsed = true;
      buildSettings->AddAttribute("SDKROOT", 
                                  this->CreateString(sysroot));
      std::string archString;
      for( std::vector<std::string>::iterator i = 
             this->Architectures.begin();
           i != this->Architectures.end(); ++i)
        {
        archString += *i;
        archString += " ";
        }
      buildSettings->AddAttribute("ARCHS", 
                                  this->CreateString(archString.c_str()));
      }
    if(!flagsUsed && sysrootDefault &&
       strcmp(sysroot, sysrootDefault) != 0)
      {
      buildSettings->AddAttribute("SDKROOT", 
                                  this->CreateString(sysroot));
      }
    }
  for( std::vector<cmXCodeObject*>::iterator i = configs.begin();
       i != configs.end(); ++i)
    {
    (*i)->AddAttribute("buildSettings", buildSettings);
    }
  this->RootObject->AddAttribute("buildConfigurationList", 
                             this->CreateObjectReference(configlist));

  std::vector<cmXCodeObject*> targets;
  for(std::vector<cmLocalGenerator*>::iterator i = generators.begin();
      i != generators.end(); ++i)
    {
    if(!this->IsExcluded(root, *i))
      {
      this->CreateXCodeTargets(*i, targets);
      }
    }
  // loop over all targets and add link and depend info
  for(std::vector<cmXCodeObject*>::iterator i = targets.begin();
      i != targets.end(); ++i)
    {
    cmXCodeObject* t = *i;
    this->AddDependAndLinkInformation(t);
    }
  // now create xcode depend hack makefile
  this->CreateXCodeDependHackTarget(targets);
  // now add all targets to the root object
  cmXCodeObject* allTargets = this->CreateObject(cmXCodeObject::OBJECT_LIST);
  for(std::vector<cmXCodeObject*>::iterator i = targets.begin();
      i != targets.end(); ++i)
    { 
    cmXCodeObject* t = *i;
    allTargets->AddObject(t);
    cmXCodeObject* productRef = t->GetObject("productReference");
    if(productRef)
      {
      productGroupChildren->AddObject(productRef->GetObject());
      }
    }
  this->RootObject->AddAttribute("targets", allTargets);
}

//----------------------------------------------------------------------------
void 
cmGlobalXCodeGenerator::CreateXCodeDependHackTarget(
  std::vector<cmXCodeObject*>& targets)
{ 
  cmGeneratedFileStream 
    makefileStream(this->CurrentXCodeHackMakefile.c_str());
  if(!makefileStream)
    {
    cmSystemTools::Error("Could not create",
                         this->CurrentXCodeHackMakefile.c_str());
    return;
    }
  makefileStream.SetCopyIfDifferent(true);
  // one more pass for external depend information not handled
  // correctly by xcode
  makefileStream << "# DO NOT EDIT\n";
  makefileStream << "# This makefile makes sure all linkable targets are\n";
  makefileStream << "# up-to-date with anything they link to, avoiding a "
    "bug in XCode 1.5\n";
  for(std::vector<std::string>::const_iterator
        ct = this->CurrentConfigurationTypes.begin();
      ct != this->CurrentConfigurationTypes.end(); ++ct)
    {
    if(this->XcodeVersion < 21 || ct->empty())
      {
      makefileStream << "all: ";
      }
    else
      {
      makefileStream << "all." << *ct << ": ";
      }
    const char* configName = 0;
    if(!ct->empty())
      {
      configName = ct->c_str();
      }
    for(std::vector<cmXCodeObject*>::iterator i = targets.begin();
        i != targets.end(); ++i)
      {
      cmXCodeObject* target = *i;
      cmTarget* t =target->GetTarget();
      if(t->GetType() == cmTarget::EXECUTABLE ||
         t->GetType() == cmTarget::SHARED_LIBRARY ||
         t->GetType() == cmTarget::MODULE_LIBRARY)
        {
        std::string tfull = t->GetFullPath(configName);
        if(t->IsAppBundleOnApple())
          {
          tfull += ".app/Contents/MacOS/";
          tfull += t->GetFullName(configName);
          }
        makefileStream << "\\\n\t" <<
          this->ConvertToRelativeForMake(tfull.c_str());
        }
      }
    makefileStream << "\n\n"; 
    }
  makefileStream 
    << "# For each target create a dummy rule "
    "so the target does not have to exist\n";
  std::set<cmStdString> emitted;
  for(std::vector<cmXCodeObject*>::iterator i = targets.begin();
      i != targets.end(); ++i)
    {
    cmXCodeObject* target = *i;
    std::map<cmStdString, cmXCodeObject::StringVec> const& deplibs =
      target->GetDependLibraries();
    for(std::map<cmStdString, cmXCodeObject::StringVec>::const_iterator ci 
          = deplibs.begin(); ci != deplibs.end(); ++ci)
      {
      for(cmXCodeObject::StringVec::const_iterator d = ci->second.begin();
          d != ci->second.end(); ++d)
        {
        if(emitted.insert(*d).second)
          {
          makefileStream << 
            this->ConvertToRelativeForMake(d->c_str()) << ":\n";
          }
        }
      }
    }
  makefileStream << "\n\n";

  // Write rules to help Xcode relink things at the right time.
  makefileStream << 
    "# Rules to remove targets that are older than anything to which they\n"
    "# link.  This forces Xcode to relink the targets from scratch.  It\n"
    "# does not seem to check these dependencies itself.\n";  
  for(std::vector<std::string>::const_iterator
        ct = this->CurrentConfigurationTypes.begin();
      ct != this->CurrentConfigurationTypes.end(); ++ct)
    {
    const char* configName = 0;
    if(!ct->empty())
      {
      configName = ct->c_str();
      }
    for(std::vector<cmXCodeObject*>::iterator i = targets.begin();
        i != targets.end(); ++i)
      {
      cmXCodeObject* target = *i;
      cmTarget* t =target->GetTarget();
      if(t->GetType() == cmTarget::EXECUTABLE ||
         t->GetType() == cmTarget::SHARED_LIBRARY ||
         t->GetType() == cmTarget::MODULE_LIBRARY)
        {
        // Create a rule for this target.
        std::string tfull = t->GetFullPath(configName);
        if(t->IsAppBundleOnApple())
          {
          tfull += ".app/Contents/MacOS/";
          tfull += t->GetFullName(configName);
          }
        makefileStream << this->ConvertToRelativeForMake(tfull.c_str()) 
                       << ":";

        // List dependencies if any exist.
        std::map<cmStdString, cmXCodeObject::StringVec>::const_iterator
          x = target->GetDependLibraries().find(*ct);
        if(x != target->GetDependLibraries().end())
          {
          std::vector<cmStdString> const& deplibs = x->second;
          for(std::vector<cmStdString>::const_iterator d = deplibs.begin();
              d != deplibs.end(); ++d)
            {
            makefileStream << "\\\n\t" << 
              this->ConvertToRelativeForMake(d->c_str());
            }
          }
        // Write the action to remove the target if it is out of date.
        makefileStream << "\n";
        makefileStream << "\t/bin/rm -f "
                       << this->ConvertToRelativeForMake(tfull.c_str())
                       << "\n";
        // if building for more than one architecture
        // then remove those exectuables as well
        if(this->Architectures.size() > 1)
          {
          std::string universal = t->GetDirectory();
          universal += "/";
          universal += this->CurrentMakefile->GetProjectName();
          universal += ".build/";
          universal += configName;
          universal += "/";
          universal += t->GetName();
          universal += ".build/Objects-normal/";
          for( std::vector<std::string>::iterator arch = 
                 this->Architectures.begin();
               arch != this->Architectures.end(); ++arch)
            {
            std::string universalFile = universal;
            universalFile += *arch;
            universalFile += "/";
            universalFile += t->GetName();
            makefileStream << "\t/bin/rm -f "
                           << 
              this->ConvertToRelativeForMake(universalFile.c_str())
                           << "\n";
            }
          }
        makefileStream << "\n\n";
        }
      }
    }
}

//----------------------------------------------------------------------------
void
cmGlobalXCodeGenerator::OutputXCodeProject(cmLocalGenerator* root,
                                           std::vector<cmLocalGenerator*>& 
                                           generators)
{
  if(generators.size() == 0)
    {
    return;
    }
  // Skip local generators that are excluded from this project.
  for(std::vector<cmLocalGenerator*>::iterator g = generators.begin();
      g != generators.end(); ++g)
    {
    if(this->IsExcluded(root, *g))
      {
      continue;
      }
    }

  this->CreateXCodeObjects(root,
                           generators);
  std::string xcodeDir = root->GetMakefile()->GetStartOutputDirectory();
  xcodeDir += "/";
  xcodeDir += root->GetMakefile()->GetProjectName();
  xcodeDir += ".xcode";
  if(this->XcodeVersion > 20)
    {
    xcodeDir += "proj";
    }
  cmSystemTools::MakeDirectory(xcodeDir.c_str());
  std::string xcodeProjFile = xcodeDir + "/project.pbxproj";
  cmGeneratedFileStream fout(xcodeProjFile.c_str());
  fout.SetCopyIfDifferent(true);
  if(!fout)
    {
    return;
    }
  this->WriteXCodePBXProj(fout, root, generators);
  this->ClearXCodeObjects();
}

//----------------------------------------------------------------------------
void 
cmGlobalXCodeGenerator::WriteXCodePBXProj(std::ostream& fout,
                                          cmLocalGenerator* ,
                                          std::vector<cmLocalGenerator*>& )
{
  fout << "// !$*UTF8*$!\n";
  fout << "{\n";
  cmXCodeObject::Indent(1, fout);
  fout << "archiveVersion = 1;\n";
  cmXCodeObject::Indent(1, fout);
  fout << "classes = {\n";
  cmXCodeObject::Indent(1, fout);
  fout << "};\n";
  cmXCodeObject::Indent(1, fout);
  fout << "objectVersion = 39;\n";
  cmXCodeObject::PrintList(this->XCodeObjects, fout);
  cmXCodeObject::Indent(1, fout);
  fout << "rootObject = " << this->RootObject->GetId() << ";\n";
  fout << "}\n";
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::GetDocumentation(cmDocumentationEntry& entry)
  const
{
  entry.Name = this->GetName();
  entry.Brief = "Generate XCode project files.";
  entry.Full = "";
}

//----------------------------------------------------------------------------
std::string cmGlobalXCodeGenerator::ConvertToRelativeForMake(const char* p)
{
  if ( !this->CurrentMakefile->IsOn("CMAKE_USE_RELATIVE_PATHS") )
    {
    return cmSystemTools::ConvertToOutputPath(p);
    }
  else
    {
    std::string ret = 
      this->CurrentLocalGenerator->
        ConvertToRelativePath(this->CurrentOutputDirectoryComponents, p);
    return cmSystemTools::ConvertToOutputPath(ret.c_str());
    }
}

//----------------------------------------------------------------------------
std::string cmGlobalXCodeGenerator::ConvertToRelativeForXCode(const char* p)
{
  if ( !this->CurrentMakefile->IsOn("CMAKE_USE_RELATIVE_PATHS") )
    {
    return cmSystemTools::ConvertToOutputPath(p);
    }
  else
    {
    std::string ret = 
      this->CurrentLocalGenerator->
        ConvertToRelativePath(this->ProjectOutputDirectoryComponents, p);
    return cmSystemTools::ConvertToOutputPath(ret.c_str());
    }
}

//----------------------------------------------------------------------------
std::string cmGlobalXCodeGenerator::XCodeEscapePath(const char* p)
{
  std::string ret = p;
  if(ret.find(' ') != ret.npos)
    {
    std::string t = ret;
    ret = "\"";
    ret += t;
    ret += "\"";
    }
  return ret;
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::
GetTargetObjectFileDirectories(cmTarget* target,
                               std::vector<std::string>& 
                               dirs)
{
  std::string dir = this->CurrentMakefile->GetCurrentOutputDirectory();
  dir += "/";
  dir += this->CurrentMakefile->GetProjectName();
  dir += ".build/";
  dir += this->GetCMakeCFGInitDirectory();
  dir += "/";
  if(target->GetType() != cmTarget::EXECUTABLE)
    {
    dir += "lib";
    }
  dir += target->GetName();
  if(target->GetType() == cmTarget::STATIC_LIBRARY)
    {
    dir += ".a";
    }
  if(target->GetType() == cmTarget::SHARED_LIBRARY)
    {
    dir += ".dylib";
    }
  if(target->GetType() == cmTarget::MODULE_LIBRARY)
    {
    dir += ".so";
    }
  dir += ".build/Objects-normal/";
  std::string dirsave = dir;
  if(this->Architectures.size())
    {
    for(std::vector<std::string>::iterator i = this->Architectures.begin(); 
        i != this->Architectures.end(); ++i)
      {
      dir += *i;
      dirs.push_back(dir);
      dir = dirsave;
      }
    }
  else
    {
    dirs.push_back(dir);
    }
}

//----------------------------------------------------------------------------
void
cmGlobalXCodeGenerator
::AppendDirectoryForConfig(const char* prefix,
                           const char* config,
                           const char* suffix,
                           std::string& dir)
{
  if(this->XcodeVersion > 20)
    {
    if(config)
      {
      dir += prefix;
      dir += config;
      dir += suffix;
      }
    }
}

//----------------------------------------------------------------------------
std::string cmGlobalXCodeGenerator::LookupFlags(const char* varNamePrefix,
                                                const char* varNameLang,
                                                const char* varNameSuffix,
                                                const char* default_flags)
{
  if(varNameLang)
    {
    std::string varName = varNamePrefix;
    varName += varNameLang;
    varName += varNameSuffix;
    if(const char* varValue =
       this->CurrentMakefile->GetDefinition(varName.c_str()))
      {
      if(*varValue)
        {
        return varValue;
        }
      }
    }
  return default_flags;
}

//----------------------------------------------------------------------------
void cmGlobalXCodeGenerator::AppendDefines(std::string& defs,
                                           const char* defines_list,
                                           bool dflag)
{
  // Skip this if there are no definitions.
  if(!defines_list)
    {
    return;
    }

  // Expand the list of definitions.
  std::vector<std::string> defines;
  cmSystemTools::ExpandListArgument(defines_list, defines);

  // GCC_PREPROCESSOR_DEFINITIONS is a space-separated list of definitions.
  // We escape everything as follows:
  //   - Place each definition in single quotes ''
  //   - Escape a single quote as \\'
  //   - Escape a backslash as \\\\ since it itself is an escape
  // Note that in the code below we need one more level of escapes for
  // C string syntax in this source file.
  const char* sep = defs.empty()? "" : " ";
  for(std::vector<std::string>::const_iterator di = defines.begin();
      di != defines.end(); ++di)
    {
    // Separate from previous definition.
    defs += sep;
    sep = " ";

    // Open single quote.
    defs += "'";

    // Add -D flag if requested.
    if(dflag)
      {
      defs += "-D";
      }

    // Escaped definition string.
    for(const char* c = di->c_str(); *c; ++c)
      {
      if(*c == '\'')
        {
        defs += "\\\\'";
        }
      else if(*c == '\\')
        {
        defs += "\\\\\\\\";
        }
      else
        {
        defs += *c;
        }
      }

    // Close single quote.
    defs += "'";
    }
}

//----------------------------------------------------------------------------
std::string
cmGlobalXCodeGenerator::ComputeInfoPListLocation(cmTarget& target)
{
  std::string plist = target.GetMakefile()->GetCurrentOutputDirectory();
  plist += cmake::GetCMakeFilesDirectory();
  plist += "/";
  plist += target.GetName();
  plist += ".dir/Info.plist";
  return plist;
}
