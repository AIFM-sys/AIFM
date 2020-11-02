/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmGlobalVisualStudio6Generator.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmGlobalVisualStudio6Generator.h"
#include "cmLocalVisualStudio6Generator.h"
#include "cmMakefile.h"
#include "cmake.h"

cmGlobalVisualStudio6Generator::cmGlobalVisualStudio6Generator()
{
  this->FindMakeProgramFile = "CMakeVS6FindMake.cmake";
}

void cmGlobalVisualStudio6Generator
::EnableLanguage(std::vector<std::string>const& lang, 
                 cmMakefile *mf, 
                 bool optional)
{
  mf->AddDefinition("CMAKE_GENERATOR_CC", "cl");
  mf->AddDefinition("CMAKE_GENERATOR_CXX", "cl");
  mf->AddDefinition("CMAKE_GENERATOR_RC", "rc"); 
  mf->AddDefinition("CMAKE_GENERATOR_NO_COMPILER_ENV", "1");
  mf->AddDefinition("CMAKE_GENERATOR_Fortran", "ifort");
  mf->AddDefinition("MSVC60", "1");
  this->GenerateConfigurations(mf);
  this->cmGlobalGenerator::EnableLanguage(lang, mf, optional);
}

void cmGlobalVisualStudio6Generator::GenerateConfigurations(cmMakefile* mf)
{
  std::string fname= mf->GetRequiredDefinition("CMAKE_ROOT");
  const char* def= mf->GetDefinition( "MSPROJECT_TEMPLATE_DIRECTORY");
  if(def)
    {
    fname = def;
    }
  else
    {
    fname += "/Templates";
    }
  fname += "/CMakeVisualStudio6Configurations.cmake";
  if(!mf->ReadListFile(mf->GetCurrentListFile(), fname.c_str()))
    {
    cmSystemTools::Error("Cannot open ", fname.c_str(),
                         ".  Please copy this file from the main "
                         "CMake/Templates directory and edit it for "
                         "your build configurations.");
    }
  else if(!mf->GetDefinition("CMAKE_CONFIGURATION_TYPES"))
    {
    cmSystemTools::Error("CMAKE_CONFIGURATION_TYPES not set by ",
                         fname.c_str(),
                         ".  Please copy this file from the main "
                         "CMake/Templates directory and edit it for "
                         "your build configurations.");
    }
}

std::string cmGlobalVisualStudio6Generator
::GenerateBuildCommand(const char* makeProgram,
                       const char *projectName, 
                       const char* additionalOptions, 
                       const char *targetName,
                       const char* config, 
                       bool ignoreErrors,
                       bool)
{
  // Ingoring errors is not implemented in visual studio 6
  (void) ignoreErrors;

  // now build the test
  std::vector<std::string> mp;
  mp.push_back("[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio"
               "\\6.0\\Setup;VsCommonDir]/MSDev98/Bin");
  cmSystemTools::ExpandRegistryValues(mp[0]);
  std::string originalCommand = makeProgram;
  std::string makeCommand = 
    cmSystemTools::FindProgram(makeProgram, mp);
  if(makeCommand.size() == 0)
    {
    std::string e = "Generator cannot find Visual Studio 6 msdev program \"";
    e += originalCommand;
    e += "\" specified by CMAKE_MAKE_PROGRAM cache entry.  ";
    e += "Please fix the setting.";
    cmSystemTools::Error(e.c_str());
    return "";
    }
  makeCommand = cmSystemTools::ConvertToOutputPath(makeCommand.c_str());

  // if there are spaces in the makeCommand, assume a full path
  // and convert it to a path with no spaces in it as the
  // RunSingleCommand does not like spaces
#if defined(_WIN32) && !defined(__CYGWIN__)      
  if(makeCommand.find(' ') != std::string::npos)
    {
    cmSystemTools::GetShortPath(makeCommand.c_str(), makeCommand);
    }
#endif
  makeCommand += " ";
  makeCommand += projectName;
  makeCommand += ".dsw /MAKE \"";
  bool clean = false;
  if ( targetName && strcmp(targetName, "clean") == 0 )
    {
    clean = true;
    targetName = "ALL_BUILD";
    }
  if (targetName && strlen(targetName))
    {
    makeCommand += targetName;
    }
  else
    {
    makeCommand += "ALL_BUILD";
    }
  makeCommand += " - ";
  if(config && strlen(config))
    {
    makeCommand += config;
    }
  else
    {
    makeCommand += "Debug";
    }
  if(clean)
    {
    makeCommand += "\" /CLEAN";
    }
  else
    {
    makeCommand += "\" /BUILD";
    }
  if ( additionalOptions )
    {
    makeCommand += " ";
    makeCommand += additionalOptions;
    }
  return makeCommand;
}

///! Create a local generator appropriate to this Global Generator
cmLocalGenerator *cmGlobalVisualStudio6Generator::CreateLocalGenerator()
{
  cmLocalGenerator *lg = new cmLocalVisualStudio6Generator;
  lg->SetGlobalGenerator(this);
  return lg;
}


void cmGlobalVisualStudio6Generator::Generate()
{
  // first do the superclass method
  this->cmGlobalVisualStudioGenerator::Generate();

  // Now write out the DSW
  this->OutputDSWFile();
}

// Write a DSW file to the stream
void cmGlobalVisualStudio6Generator
::WriteDSWFile(std::ostream& fout,cmLocalGenerator* root,
               std::vector<cmLocalGenerator*>& generators)
{
  // Write out the header for a DSW file
  this->WriteDSWHeader(fout);
  
  // Get the home directory with the trailing slash
  std::string homedir = root->GetMakefile()->GetStartOutputDirectory();
  homedir += "/";
    
  unsigned int i;
  bool doneAllBuild = false;
  bool doneRunTests = false;
  bool doneInstall = false;
  bool doneEditCache = false;
  bool doneRebuildCache = false;
  bool donePackage = false;

  for(i = 0; i < generators.size(); ++i)
    {
    if(this->IsExcluded(root, generators[i]))
      {
      continue;
      }
    cmMakefile* mf = generators[i]->GetMakefile();
    
    // Get the source directory from the makefile
    std::string dir = mf->GetStartOutputDirectory();
    // remove the home directory and / from the source directory
    // this gives a relative path 
    cmSystemTools::ReplaceString(dir, homedir.c_str(), "");

    // Get the list of create dsp files names from the LocalGenerator, more
    // than one dsp could have been created per input CMakeLists.txt file
    // for each target
    std::vector<std::string> dspnames = 
      static_cast<cmLocalVisualStudio6Generator *>(generators[i])
      ->GetCreatedProjectNames();
    cmTargets &tgts = generators[i]->GetMakefile()->GetTargets();
    std::vector<std::string>::iterator si = dspnames.begin(); 
    for(cmTargets::iterator l = tgts.begin(); l != tgts.end(); ++l)
      {
      // special handling for the current makefile
      if(mf == generators[0]->GetMakefile())
        {
        dir = "."; // no subdirectory for project generated
        // if this is the special ALL_BUILD utility, then
        // make it depend on every other non UTILITY project.
        // This is done by adding the names to the GetUtilities
        // vector on the makefile
        if(l->first == "ALL_BUILD" && !doneAllBuild)
          {
          unsigned int j;
          for(j = 0; j < generators.size(); ++j)
            {
            cmTargets &atgts = generators[j]->GetMakefile()->GetTargets();
            for(cmTargets::iterator al = atgts.begin();
                al != atgts.end(); ++al)
              {
              if (!al->second.GetPropertyAsBool("EXCLUDE_FROM_ALL"))
                {
                if (al->second.GetType() == cmTarget::UTILITY ||
                    al->second.GetType() == cmTarget::GLOBAL_TARGET)
                  {
                  l->second.AddUtility(al->first.c_str());
                  }
                else
                  {
                  l->second.AddLinkLibrary(al->first, cmTarget::GENERAL);
                  }
                }
              }
            }
          }
        }
      // Write the project into the DSW file
      if (strncmp(l->first.c_str(), "INCLUDE_EXTERNAL_MSPROJECT", 26) == 0)
        {
        cmCustomCommand cc = l->second.GetPostBuildCommands()[0];
        const cmCustomCommandLines& cmds = cc.GetCommandLines();
        std::string project = cmds[0][0];
        std::string location = cmds[0][1];
        this->WriteExternalProject(fout, project.c_str(), 
                                   location.c_str(), cc.GetDepends());
        }
      else 
        {
          bool skip = false;
          // skip ALL_BUILD and RUN_TESTS if they have already been added
          if(l->first == "ALL_BUILD" )
            {
            if(doneAllBuild)
              {
              skip = true;
              }
            else
              {
              doneAllBuild = true;
              }
            }
          if(l->first == "INSTALL")
            {
            if(doneInstall)
              {
              skip = true;
              }
            else
              {
              doneInstall = true;
              }
            }
          if(l->first == "RUN_TESTS")
            {
            if(doneRunTests)
              {
              skip = true;
              }
            else
              {
              doneRunTests = true;
              }
            }
          if(l->first == "EDIT_CACHE")
            {
            if(doneEditCache)
              {
              skip = true;
              }
            else
              {
              doneEditCache = true;
              }
            }
          if(l->first == "REBUILD_CACHE")
            {
            if(doneRebuildCache)
              {
              skip = true;
              }
            else
              {
              doneRebuildCache = true;
              }
            }
          if(l->first == "PACKAGE")
            {
            if(donePackage)
              {
              skip = true;
              }
            else
              {
              donePackage = true;
              }
            }
          if(!skip)
            {
            this->WriteProject(fout, si->c_str(), dir.c_str(),l->second);
            }
          ++si;
        }
      }
    }
  
  // Write the footer for the DSW file
  this->WriteDSWFooter(fout);
}

void cmGlobalVisualStudio6Generator
::OutputDSWFile(cmLocalGenerator* root, 
                std::vector<cmLocalGenerator*>& generators)
{
  if(generators.size() == 0)
    {
    return;
    }
  std::string fname = root->GetMakefile()->GetStartOutputDirectory();
  fname += "/";
  fname += root->GetMakefile()->GetProjectName();
  fname += ".dsw";
  std::ofstream fout(fname.c_str());
  if(!fout)
    {
    cmSystemTools::Error("Error can not open DSW file for write: ",
                         fname.c_str());
    cmSystemTools::ReportLastSystemError("");
    return;
    }
  this->WriteDSWFile(fout, root, generators);
}

// output the DSW file
void cmGlobalVisualStudio6Generator::OutputDSWFile()
{ 
  std::map<cmStdString, std::vector<cmLocalGenerator*> >::iterator it;
  for(it = this->ProjectMap.begin(); it!= this->ProjectMap.end(); ++it)
    {
    this->OutputDSWFile(it->second[0], it->second);
    }
}

// Write a dsp file into the DSW file,
// Note, that dependencies from executables to 
// the libraries it uses are also done here
void cmGlobalVisualStudio6Generator::WriteProject(std::ostream& fout, 
                                                  const char* dspname,
                                                  const char* dir,
                                                  cmTarget& target)
{
  fout << "#########################################################"
    "######################\n\n";
  fout << "Project: \"" << dspname << "\"=" 
       << dir << "\\" << dspname << ".dsp - Package Owner=<4>\n\n";
  fout << "Package=<5>\n{{{\n}}}\n\n";
  fout << "Package=<4>\n";
  fout << "{{{\n";

  // insert Begin Project Dependency  Project_Dep_Name project stuff here 
  if (target.GetType() != cmTarget::STATIC_LIBRARY)
    {
    cmTarget::LinkLibraryVectorType::const_iterator j, jend;
    j = target.GetLinkLibraries().begin();
    jend = target.GetLinkLibraries().end();
    for(;j!= jend; ++j)
      {
      if(j->first != dspname)
        {
        // is the library part of this DSW ? If so add dependency
        if(this->FindTarget(0, j->first.c_str()))
          {
          fout << "Begin Project Dependency\n";
          fout << "Project_Dep_Name " << j->first.c_str() << "\n";
          fout << "End Project Dependency\n";
          }
        }
      }
    }

  std::set<cmStdString>::const_iterator i, end;
  // write utility dependencies.
  i = target.GetUtilities().begin();
  end = target.GetUtilities().end();
  for(;i!= end; ++i)
    {
    if(*i != dspname)
      {
      std::string depName = this->GetUtilityForTarget(target, i->c_str());
      fout << "Begin Project Dependency\n";
      fout << "Project_Dep_Name " << depName << "\n";
      fout << "End Project Dependency\n";
      }
    }
  fout << "}}}\n\n";
}


// Write a dsp file into the DSW file,
// Note, that dependencies from executables to 
// the libraries it uses are also done here
void cmGlobalVisualStudio6Generator::WriteExternalProject(std::ostream& fout, 
                               const char* name,
                               const char* location,
                               const std::vector<std::string>& dependencies)
{
 fout << "#########################################################"
    "######################\n\n";
  fout << "Project: \"" << name << "\"=" 
       << location << " - Package Owner=<4>\n\n";
  fout << "Package=<5>\n{{{\n}}}\n\n";
  fout << "Package=<4>\n";
  fout << "{{{\n";

  
  std::vector<std::string>::const_iterator i, end;
  // write dependencies.
  i = dependencies.begin();
  end = dependencies.end();
  for(;i!= end; ++i)
  {
    fout << "Begin Project Dependency\n";
    fout << "Project_Dep_Name " << *i << "\n";
    fout << "End Project Dependency\n";
  }
  fout << "}}}\n\n";
}



// Standard end of dsw file
void cmGlobalVisualStudio6Generator::WriteDSWFooter(std::ostream& fout)
{
  fout << "######################################################"
    "#########################\n\n";
  fout << "Global:\n\n";
  fout << "Package=<5>\n{{{\n}}}\n\n";
  fout << "Package=<3>\n{{{\n}}}\n\n";
  fout << "#####################################################"
    "##########################\n\n";
}

  
// ouput standard header for dsw file
void cmGlobalVisualStudio6Generator::WriteDSWHeader(std::ostream& fout)
{
  fout << "Microsoft Developer Studio Workspace File, Format Version 6.00\n";
  fout << "# WARNING: DO NOT EDIT OR DELETE THIS WORKSPACE FILE!\n\n";
}

//----------------------------------------------------------------------------
void cmGlobalVisualStudio6Generator
::GetDocumentation(cmDocumentationEntry& entry) const
{
  entry.Name = this->GetName();
  entry.Brief = "Generates Visual Studio 6 project files.";
  entry.Full = "";
}

//----------------------------------------------------------------------------
void
cmGlobalVisualStudio6Generator
::AppendDirectoryForConfig(const char* prefix,
                           const char* config,
                           const char* suffix,
                           std::string& dir)
{
  if(config)
    {
    dir += prefix;
    dir += config;
    dir += suffix;
    }
}
