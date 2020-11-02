/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmMakeDepend.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmMakeDepend.h"
#include "cmSystemTools.h"

#include <cmsys/RegularExpression.hxx>

void cmDependInformation::AddDependencies(cmDependInformation* info)
{
  if(this != info)
    {
    this->DependencySet.insert(info);
    }
}

cmMakeDepend::cmMakeDepend()
{
  this->Verbose = false;
  this->IncludeFileRegularExpression.compile("^.*$");
  this->ComplainFileRegularExpression.compile("^$");
}


cmMakeDepend::~cmMakeDepend()
{ 
  for(DependInformationMapType::iterator i = 
        this->DependInformationMap.begin();
      i != this->DependInformationMap.end(); ++i)
    {
    delete i->second;
    }
}


// Set the makefile that depends will be made from.
// The pointer is kept so the cmSourceFile array can
// be updated with the depend information in the cmMakefile.

void cmMakeDepend::SetMakefile(cmMakefile* makefile)
{
  this->Makefile = makefile;

  // Now extract the include file regular expression from the makefile.
  this->IncludeFileRegularExpression.compile(
    this->Makefile->IncludeFileRegularExpression.c_str());
  this->ComplainFileRegularExpression.compile(
    this->Makefile->ComplainFileRegularExpression.c_str());
  
  // Now extract any include paths from the makefile flags
  const std::vector<std::string>& includes =
    this->Makefile->GetIncludeDirectories();
  for(std::vector<std::string>::const_iterator j = includes.begin();
      j != includes.end(); ++j)
    {
    std::string path = *j;
    this->Makefile->ExpandVariablesInString(path);
    this->AddSearchPath(path.c_str());
    }
}


const cmDependInformation* cmMakeDepend::FindDependencies(const char* file)
{
  cmDependInformation* info = this->GetDependInformation(file,0);
  this->GenerateDependInformation(info);
  return info;
}

void cmMakeDepend::GenerateDependInformation(cmDependInformation* info)
{
  // If dependencies are already done, stop now.
  if(info->DependDone)
    {
    return;
    }
  else
    {
    // Make sure we don't visit the same file more than once.
    info->DependDone = true;
    }
  const char* path = info->FullPath.c_str();
  if(!path)
    {
    cmSystemTools::Error(
      "Attempt to find dependencies for file without path!");
    return;
    }

  bool found = false;

  // If the file exists, use it to find dependency information.
  if(cmSystemTools::FileExists(path, true))
    {
    // Use the real file to find its dependencies.
    this->DependWalk(info);
    found = true;
    }


  // See if the cmSourceFile for it has any files specified as
  // dependency hints.
  if(info->SourceFile != 0)
    {

    // Get the cmSourceFile corresponding to this.
    const cmSourceFile& cFile = *(info->SourceFile);
    // See if there are any hints for finding dependencies for the missing
    // file.
    if(!cFile.GetDepends().empty())
      {
      // Dependency hints have been given.  Use them to begin the
      // recursion.
      for(std::vector<std::string>::const_iterator file =
            cFile.GetDepends().begin(); file != cFile.GetDepends().end();
          ++file)
        {
        this->AddDependency(info, file->c_str());
        }

      // Found dependency information.  We are done.
      found = true;
      }
    }

  if(!found)
    {
    // Try to find the file amongst the sources
    cmSourceFile *srcFile = this->Makefile->GetSource
      (cmSystemTools::GetFilenameWithoutExtension(path).c_str());
    if (srcFile)
      {
      if (srcFile->GetFullPath() == path)
        {
        found=true;
        }
      else
        {
        //try to guess which include path to use
        for(std::vector<std::string>::iterator t = 
              this->IncludeDirectories.begin();
            t != this->IncludeDirectories.end(); ++t)
          {
          std::string incpath = *t;
          if (incpath.size() && incpath[incpath.size() - 1] != '/')
            {
            incpath = incpath + "/";
            }
          incpath = incpath + path;
          if (srcFile->GetFullPath() == incpath)
            {
            // set the path to the guessed path
            info->FullPath = incpath; 
            found=true;
            }
          }
        }
      }
    }

  if(!found)
    {
    // Couldn't find any dependency information.
    if(this->ComplainFileRegularExpression.find(info->IncludeName.c_str()))
      {
      cmSystemTools::Error("error cannot find dependencies for ", path);
      }
    else
      {
      // Destroy the name of the file so that it won't be output as a
      // dependency.
      info->FullPath = "";
      }
    }
}

// This function actually reads the file specified and scans it for
// #include directives
void cmMakeDepend::DependWalk(cmDependInformation* info)
{
  cmsys::RegularExpression includeLine
    ("^[ \t]*#[ \t]*include[ \t]*[<\"]([^\">]+)[\">]");
  std::ifstream fin(info->FullPath.c_str());
  if(!fin)
    {
    cmSystemTools::Error("Cannot open ", info->FullPath.c_str());
    return;
    }

  // TODO: Write real read loop (see cmSystemTools::CopyFile).
  std::string line;
  while( cmSystemTools::GetLineFromStream(fin, line) )
    {
    if(includeLine.find(line.c_str()))
      {
      // extract the file being included
      std::string includeFile = includeLine.match(1);
      // see if the include matches the regular expression
      if(!this->IncludeFileRegularExpression.find(includeFile))
        {
        if(this->Verbose)
          {
          std::string message = "Skipping ";
          message += includeFile;
          message += " for file ";
          message += info->FullPath.c_str();
          cmSystemTools::Error(message.c_str(), 0);
          }
        continue;
        }

      // Add this file and all its dependencies.
      this->AddDependency(info, includeFile.c_str());
      }
    }
}


void cmMakeDepend::AddDependency(cmDependInformation* info, const char* file)
{
  cmDependInformation* dependInfo = 
    this->GetDependInformation(file, info->PathOnly.c_str());
  this->GenerateDependInformation(dependInfo);
  info->AddDependencies(dependInfo);
}

cmDependInformation* cmMakeDepend::GetDependInformation(const char* file,
                                                        const char *extraPath)
{
  // Get the full path for the file so that lookup is unambiguous.
  std::string fullPath = this->FullPath(file, extraPath);

  // Try to find the file's instance of cmDependInformation.
  DependInformationMapType::const_iterator result =
    this->DependInformationMap.find(fullPath);
  if(result != this->DependInformationMap.end())
    {
    // Found an instance, return it.
    return result->second;
    }
  else
    {
    // Didn't find an instance.  Create a new one and save it.
    cmDependInformation* info = new cmDependInformation;
    info->FullPath = fullPath;
    info->PathOnly = cmSystemTools::GetFilenamePath(fullPath.c_str());
    info->IncludeName = file;
    this->DependInformationMap[fullPath] = info;
    return info;
    }
}


void cmMakeDepend::GenerateMakefileDependencies()
{
  // Now create cmDependInformation objects for files in the directory
  cmTargets &tgts = this->Makefile->GetTargets();
  for(cmTargets::iterator l = tgts.begin(); 
      l != tgts.end(); l++)
    {
    const std::vector<cmSourceFile*> &classes = l->second.GetSourceFiles();
    for(std::vector<cmSourceFile*>::const_iterator i = classes.begin();
        i != classes.end(); ++i)
      {
      if(!(*i)->GetPropertyAsBool("HEADER_FILE_ONLY"))
        {
        cmDependInformation* info =
          this->GetDependInformation((*i)->GetFullPath().c_str(),0);
        this->AddFileToSearchPath(info->FullPath.c_str());
        info->SourceFile = *i;
        this->GenerateDependInformation(info);
        }
      }
    }
}


// find the full path to fname by searching the this->IncludeDirectories array
std::string cmMakeDepend::FullPath(const char* fname, const char *extraPath)
{
  DirectoryToFileToPathMapType::iterator m;
  if(extraPath)
    {
    m = this->DirectoryToFileToPathMap.find(extraPath);
    }
  else
    {
    m = this->DirectoryToFileToPathMap.find("");
    }
  
  if(m != this->DirectoryToFileToPathMap.end())
    {
    FileToPathMapType& map = m->second;
    FileToPathMapType::iterator p = map.find(fname);
    if(p != map.end())
      {
      return p->second;
      }
    }

  if(cmSystemTools::FileExists(fname, true))
    {
    std::string fp = cmSystemTools::CollapseFullPath(fname);
    this->DirectoryToFileToPathMap[extraPath? extraPath: ""][fname] = fp;
    return fp;
    }
  
  for(std::vector<std::string>::iterator i = this->IncludeDirectories.begin();
      i != this->IncludeDirectories.end(); ++i)
    {
    std::string path = *i;
    if (path.size() && path[path.size() - 1] != '/')
      {
      path = path + "/";
      }
    path = path + fname;
    if(cmSystemTools::FileExists(path.c_str(), true)
       && !cmSystemTools::FileIsDirectory(path.c_str()))
      {
      std::string fp = cmSystemTools::CollapseFullPath(path.c_str());
      this->DirectoryToFileToPathMap[extraPath? extraPath: ""][fname] = fp;
      return fp;
      }
    }

  if (extraPath)
    {
    std::string path = extraPath;
    if (path.size() && path[path.size() - 1] != '/')
      {
      path = path + "/";
      }
    path = path + fname;
    if(cmSystemTools::FileExists(path.c_str(), true)
       && !cmSystemTools::FileIsDirectory(path.c_str()))
      {
      std::string fp = cmSystemTools::CollapseFullPath(path.c_str());
      this->DirectoryToFileToPathMap[extraPath][fname] = fp;
      return fp;
      }
    }

  // Couldn't find the file.
  return std::string(fname);
}

// Add a directory to the search path
void cmMakeDepend::AddSearchPath(const char* path)
{
  this->IncludeDirectories.push_back(path);
}

// Add a directory to the search path
void cmMakeDepend::AddFileToSearchPath(const char* file)
{
  std::string filepath = file;
  std::string::size_type pos = filepath.rfind('/');
  if(pos != std::string::npos)
    {
    std::string path = filepath.substr(0, pos);
    if(std::find(this->IncludeDirectories.begin(),
                 this->IncludeDirectories.end(), path)
       == this->IncludeDirectories.end())
      {
      this->IncludeDirectories.push_back(path);
      return;
      }
    }
}

const cmDependInformation*
cmMakeDepend::GetDependInformationForSourceFile(const cmSourceFile &sf) const
{
  for(DependInformationMapType::const_iterator i = 
        this->DependInformationMap.begin();
      i != this->DependInformationMap.end(); ++i)
    {
    const cmDependInformation* info = i->second;
    if(info->SourceFile == &sf)
      {
      return info;
      }
    }
  return 0;
}
