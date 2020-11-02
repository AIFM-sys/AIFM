/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmGlobalVisualStudio8Generator.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:07 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "windows.h" // this must be first to define GetCurrentDirectory
#include "cmGlobalVisualStudio8Generator.h"
#include "cmLocalVisualStudio7Generator.h"
#include "cmMakefile.h"
#include "cmake.h"
#include "cmGeneratedFileStream.h"

//----------------------------------------------------------------------------
cmGlobalVisualStudio8Generator::cmGlobalVisualStudio8Generator()
{
  this->FindMakeProgramFile = "CMakeVS8FindMake.cmake";
  this->ProjectConfigurationSectionName = "ProjectConfigurationPlatforms";
  this->PlatformName = "Win32";
}

//----------------------------------------------------------------------------
///! Create a local generator appropriate to this Global Generator
cmLocalGenerator *cmGlobalVisualStudio8Generator::CreateLocalGenerator()
{
  cmLocalVisualStudio7Generator *lg = new cmLocalVisualStudio7Generator;
  lg->SetVersion8();
  lg->SetExtraFlagTable(this->GetExtraFlagTableVS8());
  lg->SetGlobalGenerator(this);
  return lg;
}
  
//----------------------------------------------------------------------------
// ouput standard header for dsw file
void cmGlobalVisualStudio8Generator::WriteSLNHeader(std::ostream& fout)
{
  fout << "Microsoft Visual Studio Solution File, Format Version 9.00\n";
  fout << "# Visual Studio 2005\n";
}

//----------------------------------------------------------------------------
void cmGlobalVisualStudio8Generator
::GetDocumentation(cmDocumentationEntry& entry) const
{
  entry.Name = this->GetName();
  entry.Brief = "Generates Visual Studio .NET 2005 project files.";
  entry.Full = "";
}

//----------------------------------------------------------------------------
void cmGlobalVisualStudio8Generator::AddPlatformDefinitions(cmMakefile* mf)
{
  mf->AddDefinition("MSVC80", "1");
}

//----------------------------------------------------------------------------
void cmGlobalVisualStudio8Generator::Configure()
{
  this->cmGlobalVisualStudio7Generator::Configure();
  this->CreateGUID(CMAKE_CHECK_BUILD_SYSTEM_TARGET);
}

//----------------------------------------------------------------------------
std::string cmGlobalVisualStudio8Generator::GetUserMacrosDirectory()
{
  // Some VS8 sp0 versions cannot run macros.
  // See http://support.microsoft.com/kb/928209
  const char* vc8sp1Registry =
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\8.0\\"
    "InstalledProducts\\KB926601;";
  const char* vc8exSP1Registry =
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\8.0\\"
    "InstalledProducts\\KB926748;";
  std::string vc8sp1;
  if (!cmSystemTools::ReadRegistryValue(vc8sp1Registry, vc8sp1) &&
      !cmSystemTools::ReadRegistryValue(vc8exSP1Registry, vc8sp1))
    {
    return "";
    }

  std::string base;
  std::string path;

  // base begins with the VisualStudioProjectsLocation reg value...
  if (cmSystemTools::ReadRegistryValue(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\VisualStudio\\8.0;"
    "VisualStudioProjectsLocation",
    base))
    {
    cmSystemTools::ConvertToUnixSlashes(base);

    // 8.0 macros folder:
    path = base + "/VSMacros80";
    }

  // path is (correctly) still empty if we did not read the base value from
  // the Registry value
  return path;
}

//----------------------------------------------------------------------------
std::string cmGlobalVisualStudio8Generator::GetUserMacrosRegKeyBase()
{
  return "Software\\Microsoft\\VisualStudio\\8.0\\vsmacros";
}

//----------------------------------------------------------------------------
void cmGlobalVisualStudio8Generator::Generate()
{
  // Add a special target on which all other targets depend that
  // checks the build system and optionally re-runs CMake.
  const char* no_working_directory = 0;
  std::vector<std::string> no_depends;
  std::map<cmStdString, std::vector<cmLocalGenerator*> >::iterator it;
  for(it = this->ProjectMap.begin(); it!= this->ProjectMap.end(); ++it)
    {
    std::vector<cmLocalGenerator*>& generators = it->second;
    if(!generators.empty())
      {
      // Add the build-system check target to the first local
      // generator of this project.
      cmLocalVisualStudio7Generator* lg =
        static_cast<cmLocalVisualStudio7Generator*>(generators[0]);
      cmMakefile* mf = lg->GetMakefile();

      // Skip the target if no regeneration is to be done.
      if(mf->IsOn("CMAKE_SUPPRESS_REGENERATION"))
        {
        continue;
        }

      std::string cmake_command = mf->GetRequiredDefinition("CMAKE_COMMAND");
      cmCustomCommandLines noCommandLines;
      mf->AddUtilityCommand(CMAKE_CHECK_BUILD_SYSTEM_TARGET, false,
                            no_working_directory, no_depends,
                            noCommandLines);
      cmTarget* tgt = mf->FindTarget(CMAKE_CHECK_BUILD_SYSTEM_TARGET);
      if(!tgt)
        {
        cmSystemTools::Error("Error adding target " 
                             CMAKE_CHECK_BUILD_SYSTEM_TARGET);
        continue;
        }

      // Create a list of all stamp files for this project.
      std::vector<std::string> stamps;
      std::string stampList = cmake::GetCMakeFilesDirectoryPostSlash();
      stampList += "generate.stamp.list";
      {
      std::string stampListFile =
        generators[0]->GetMakefile()->GetCurrentOutputDirectory();
      stampListFile += "/";
      stampListFile += stampList;
      std::string stampFile;
      cmGeneratedFileStream fout(stampListFile.c_str());
      for(std::vector<cmLocalGenerator*>::const_iterator
            gi = generators.begin(); gi != generators.end(); ++gi)
        {
        stampFile = (*gi)->GetMakefile()->GetCurrentOutputDirectory();
        stampFile += "/";
        stampFile += cmake::GetCMakeFilesDirectoryPostSlash();
        stampFile += "generate.stamp";
        stampFile = generators[0]->Convert(stampFile.c_str(),
                                           cmLocalGenerator::START_OUTPUT);
        fout << stampFile << "\n";
        stamps.push_back(stampFile);
        }
      }

      // Add a custom rule to re-run CMake if any input files changed.
      {
      // Collect the input files used to generate all targets in this
      // project.
      std::vector<std::string> listFiles;
      for(unsigned int j = 0; j < generators.size(); ++j)
        {
        cmMakefile* lmf = generators[j]->GetMakefile();
        listFiles.insert(listFiles.end(), lmf->GetListFiles().begin(),
                         lmf->GetListFiles().end());
        }
      // Sort the list of input files and remove duplicates.
      std::sort(listFiles.begin(), listFiles.end(),
                std::less<std::string>());
      std::vector<std::string>::iterator new_end =
        std::unique(listFiles.begin(), listFiles.end());
      listFiles.erase(new_end, listFiles.end());

      // Create a rule to re-run CMake.
      std::string stampName = cmake::GetCMakeFilesDirectoryPostSlash();
      stampName += "generate.stamp";
      const char* dsprule = mf->GetRequiredDefinition("CMAKE_COMMAND");
      cmCustomCommandLine commandLine;
      commandLine.push_back(dsprule);
      std::string argH = "-H";
      argH += lg->Convert(mf->GetHomeDirectory(),
                          cmLocalGenerator::START_OUTPUT,
                          cmLocalGenerator::UNCHANGED, true);
      commandLine.push_back(argH);
      std::string argB = "-B";
      argB += lg->Convert(mf->GetHomeOutputDirectory(),
                          cmLocalGenerator::START_OUTPUT,
                          cmLocalGenerator::UNCHANGED, true);
      commandLine.push_back(argB);
      commandLine.push_back("--check-stamp-list");
      commandLine.push_back(stampList.c_str());
      commandLine.push_back("--vs-solution-file");
      commandLine.push_back("\"$(SolutionPath)\"");
      cmCustomCommandLines commandLines;
      commandLines.push_back(commandLine);

      // Add the rule.  Note that we cannot use the CMakeLists.txt
      // file as the main dependency because it would get
      // overwritten by the CreateVCProjBuildRule.
      // (this could be avoided with per-target source files)
      const char* no_main_dependency = 0;
      const char* no_working_directory = 0;
      mf->AddCustomCommandToOutput(
        stamps, listFiles,
        no_main_dependency, commandLines, "Checking Build System",
        no_working_directory, true);
      std::string ruleName = stamps[0];
      ruleName += ".rule";
      if(cmSourceFile* file = mf->GetSource(ruleName.c_str()))
        {
        tgt->AddSourceFile(file);
        }
      else
        {
        cmSystemTools::Error("Error adding rule for ", stamps[0].c_str());
        }
      }
      }
    }

  // Now perform the main generation.
  this->cmGlobalVisualStudio7Generator::Generate();
}

//----------------------------------------------------------------------------
void cmGlobalVisualStudio8Generator::WriteSLNFile(
  std::ostream& fout, cmLocalGenerator* root,
  std::vector<cmLocalGenerator*>& generators)
{
  // Make all targets depend on their respective project's build
  // system check target.
  unsigned int i;
  for(i = 0; i < generators.size(); ++i)
    {
    if(this->IsExcluded(root, generators[i]))
      {
      continue;
      }
    cmMakefile* mf = generators[i]->GetMakefile();
    cmTargets& tgts = mf->GetTargets();
    for(cmTargets::iterator l = tgts.begin(); l != tgts.end(); ++l)
      {
      if(l->first == CMAKE_CHECK_BUILD_SYSTEM_TARGET)
        {
        for(unsigned int j = 0; j < generators.size(); ++j)
          {
          // Every target in all generators should depend on this target.
          cmMakefile* lmf = generators[j]->GetMakefile();
          cmTargets &atgts = lmf->GetTargets();
          for(cmTargets::iterator al = atgts.begin(); al != atgts.end(); ++al)
            {
            al->second.AddUtility(l->first.c_str());
            }
          }
        }
      }
    }

  // Now write the solution file.
  this->cmGlobalVisualStudio71Generator::WriteSLNFile(fout, root, generators);
}

//----------------------------------------------------------------------------
void
cmGlobalVisualStudio8Generator
::WriteSolutionConfigurations(std::ostream& fout)
{
  fout << "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n";
  for(std::vector<std::string>::iterator i = this->Configurations.begin();
      i != this->Configurations.end(); ++i)
    {
    fout << "\t\t" << *i << "|" << this->PlatformName << " = " << *i << "|"
         << this->PlatformName << "\n";
    }
  fout << "\tEndGlobalSection\n";
}

//----------------------------------------------------------------------------
void
cmGlobalVisualStudio8Generator
::WriteProjectConfigurations(std::ostream& fout, const char* name,
                             bool partOfDefaultBuild)
{
  std::string guid = this->GetGUID(name);
  for(std::vector<std::string>::iterator i = this->Configurations.begin();
      i != this->Configurations.end(); ++i)
    {
    fout << "\t\t{" << guid << "}." << *i
         << "|" << this->PlatformName << ".ActiveCfg = "
         << *i << "|" << this->PlatformName << "\n";
    if(partOfDefaultBuild)
      {
      fout << "\t\t{" << guid << "}." << *i
           << "|" << this->PlatformName << ".Build.0 = "
           << *i << "|" << this->PlatformName << "\n";
      }
    }
}

//----------------------------------------------------------------------------
static cmVS7FlagTable cmVS8ExtraFlagTable[] =
{ 
  {"CallingConvention", "Gd", "cdecl", "0", 0 },
  {"CallingConvention", "Gr", "fastcall", "1", 0 },
  {"CallingConvention", "Gz", "stdcall", "2", 0 },

  {"Detect64BitPortabilityProblems", "Wp64",
   "Detect 64Bit Portability Problems", "true", 0 },
  {"ErrorReporting", "errorReport:prompt", "Report immediately", "1", 0 },
  {"ErrorReporting", "errorReport:queue", "Queue for next login", "2", 0 },
  // Precompiled header and related options.  Note that the
  // UsePrecompiledHeader entries are marked as "Continue" so that the
  // corresponding PrecompiledHeaderThrough entry can be found.
  {"UsePrecompiledHeader", "Yu", "Use Precompiled Header", "2",
   cmVS7FlagTable::UserValueIgnored | cmVS7FlagTable::Continue},
  {"PrecompiledHeaderThrough", "Yu", "Precompiled Header Name", "",
   cmVS7FlagTable::UserValueRequired},
  // There is no YX option in the VS8 IDE.

  // Exception handling mode.  If no entries match, it will be FALSE.
  {"ExceptionHandling", "GX", "enable c++ exceptions", "1", 0},
  {"ExceptionHandling", "EHsc", "enable c++ exceptions", "1", 0},
  {"ExceptionHandling", "EHa", "enable SEH exceptions", "2", 0},

  {0,0,0,0,0}
};
cmVS7FlagTable const* cmGlobalVisualStudio8Generator::GetExtraFlagTableVS8()
{
  return cmVS8ExtraFlagTable;
}
