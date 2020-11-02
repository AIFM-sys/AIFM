/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmTest.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmTest_h
#define cmTest_h

#include "cmCustomCommand.h"
#include "cmPropertyMap.h"
class cmMakefile;

/** \class cmTest
 * \brief Represent a test
 *
 * cmTest is representation of a test.
 */
class cmTest
{
public:
  /**
   */
  cmTest();
  ~cmTest();

  ///! Set the test name
  void SetName(const char* name);
  const char* GetName() const { return this->Name.c_str(); }
  void SetCommand(const char* command);
  const char* GetCommand() const { return this->Command.c_str(); }
  void SetArguments(const std::vector<cmStdString>& args);
  const std::vector<cmStdString>& GetArguments() const
    {
    return this->Args;
    }

  /**
   * Print the structure to std::cout.
   */
  void Print() const;

  ///! Set/Get a property of this source file
  void SetProperty(const char *prop, const char *value);
  void AppendProperty(const char* prop, const char* value);
  const char *GetProperty(const char *prop) const;
  bool GetPropertyAsBool(const char *prop) const;
  cmPropertyMap &GetProperties() { return this->Properties; };
    
  // Define the properties
  static void DefineProperties(cmake *cm);

  ///! Set the cmMakefile that owns this test
  void SetMakefile(cmMakefile *mf);
  cmMakefile *GetMakefile() { return this->Makefile;};

private:
  cmPropertyMap Properties;
  cmStdString Name;
  cmStdString Command;
  std::vector<cmStdString> Args;

  // The cmMakefile instance that owns this target.  This should
  // always be set.
  cmMakefile* Makefile;  
};

#endif

