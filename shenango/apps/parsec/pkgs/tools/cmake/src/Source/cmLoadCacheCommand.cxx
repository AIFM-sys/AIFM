/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmLoadCacheCommand.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmLoadCacheCommand.h"

#include <cmsys/RegularExpression.hxx>

// cmLoadCacheCommand
bool cmLoadCacheCommand
::InitialPass(std::vector<std::string> const& args, cmExecutionStatus &)
{
  if (args.size()< 1)
    {
    this->SetError("called with wrong number of arguments.");
    }

  if(args.size() >= 2 && args[1] == "READ_WITH_PREFIX")
    {
    return this->ReadWithPrefix(args);
    }
  
  // Cache entries to be excluded from the import list.
  // If this set is empty, all cache entries are brought in
  // and they can not be overridden.
  bool excludeFiles=false;
  unsigned int i;
  std::set<cmStdString> excludes;

  for(i=0; i<args.size(); i++)
    {
    if (excludeFiles)
      {
      excludes.insert(args[i]);
      }
    if (args[i] == "EXCLUDE")
      {
      excludeFiles=true;
      }
    if (excludeFiles && (args[i] == "INCLUDE_INTERNALS"))
      {
      break;
      }
    }

  // Internal cache entries to be imported.
  // If this set is empty, no internal cache entries are
  // brought in.
  bool includeFiles=false;
  std::set<cmStdString> includes;

  for(i=0; i<args.size(); i++)
    {
    if (includeFiles)
      {
      includes.insert(args[i]);
      }
    if (args[i] == "INCLUDE_INTERNALS")
      {
      includeFiles=true;
      }
    if (includeFiles && (args[i] == "EXCLUDE"))
      {
      break;
      }
    }

  // Loop over each build directory listed in the arguments.  Each
  // directory has a cache file.
  for(i=0; i<args.size(); i++)
    {
    if ((args[i] == "EXCLUDE") || (args[i] == "INCLUDE_INTERNALS"))
      {
      break;
      }
    this->Makefile->GetCacheManager()->LoadCache(args[i].c_str(), false,
                                             excludes, includes);
    }


  return true;
}

//----------------------------------------------------------------------------
bool cmLoadCacheCommand::ReadWithPrefix(std::vector<std::string> const& args)
{
  // Make sure we have a prefix.
  if(args.size() < 3)
    {
    this->SetError("READ_WITH_PREFIX form must specify a prefix.");
    return false;
    }
  
  // Make sure the cache file exists.
  std::string cacheFile = args[0]+"/CMakeCache.txt";
  if(!cmSystemTools::FileExists(cacheFile.c_str()))
    {
    std::string e = "Cannot load cache file from " + cacheFile;
    this->SetError(e.c_str());
    return false;
    }
  
  // Prepare the table of variables to read.
  this->Prefix = args[2];
  for(unsigned int i=3; i < args.size(); ++i)
    {
    this->VariablesToRead.insert(args[i]);
    }
  
  // Read the cache file.
  std::ifstream fin(cacheFile.c_str());  
  
  // This is a big hack read loop to overcome a buggy ifstream
  // implementation on HP-UX.  This should work on all platforms even
  // for small buffer sizes.
  const int bufferSize = 4096;
  char buffer[bufferSize];  
  std::string line;
  while(fin)
    {
    // Read a block of the file.
    fin.read(buffer, bufferSize);
    if(fin.gcount())
      {
      // Parse for newlines directly.
      const char* i = buffer;
      const char* end = buffer+fin.gcount();
      while(i != end)
        {
        const char* begin = i;
        while(i != end && *i != '\n') { ++i; }
        if(i == begin || *(i-1) != '\r')
          {
          // Include this portion of the line.
          line += std::string(begin, i-begin);
          }
        else
          {
          // Include this portion of the line.
          // Don't include the \r in a \r\n pair.
          line += std::string(begin, i-1-begin);
          }
        if(i != end)
          {
          // Completed a line.
          this->CheckLine(line.c_str());
          line = "";
          
          // Skip the newline character.
          ++i;
          }
        }
      }
    }
  if(line.length())
    {
    // Partial last line.
    this->CheckLine(line.c_str());
    }
  
  return true;
}

//----------------------------------------------------------------------------
void cmLoadCacheCommand::CheckLine(const char* line)
{
  // Check one line of the cache file.
  std::string var;
  std::string value;
  if(this->ParseEntry(line, var, value))
    {
    // Found a real entry.  See if this one was requested.
    if(this->VariablesToRead.find(var) != this->VariablesToRead.end())
      {
      // This was requested.  Set this variable locally with the given
      // prefix.
      var = this->Prefix + var;
      if(value.length())
        {
        this->Makefile->AddDefinition(var.c_str(), value.c_str());
        }
      else
        {
        this->Makefile->RemoveDefinition(var.c_str());
        }
      }
    }
}

//----------------------------------------------------------------------------
bool cmLoadCacheCommand::ParseEntry(const char* entry, std::string& var,
                                    std::string& value)
{
  // input line is:         key:type=value
  cmsys::RegularExpression reg("^([^:]*):([^=]*)=(.*[^\t ]|[\t ]*)[\t ]*$");
  // input line is:         "key":type=value
  cmsys::RegularExpression 
    regQuoted("^\"([^\"]*)\":([^=]*)=(.*[^\t ]|[\t ]*)[\t ]*$");
  bool flag = false;
  if(regQuoted.find(entry))
    {
    var = regQuoted.match(1);
    value = regQuoted.match(3);
    flag = true;
    }
  else if (reg.find(entry))
    {
    var = reg.match(1);
    value = reg.match(3);
    flag = true;
    }

  // if value is enclosed in single quotes ('foo') then remove them
  // it is used to enclose trailing space or tab
  if (flag && 
      value.size() >= 2 &&
      value[0] == '\'' && 
      value[value.size() - 1] == '\'') 
    {
    value = value.substr(1, value.size() - 2);
    }
  return flag;
}
