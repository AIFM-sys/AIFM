/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmTest.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmTest.h"
#include "cmSystemTools.h"

#include "cmake.h"
#include "cmMakefile.h"

cmTest::cmTest() 
{
  this->Makefile = 0;
}

cmTest::~cmTest()
{
}

void cmTest::SetName(const char* name)
{
  if ( !name )
    {
    name = "";
    }
  this->Name = name;
}

void cmTest::SetCommand(const char* command)
{
  if ( !command )
    {
    command = "";
    }
  this->Command = command;
  cmSystemTools::ConvertToUnixSlashes(this->Command);
}

void cmTest::SetArguments(const std::vector<cmStdString>& args)
{
  this->Args = args;
}

const char *cmTest::GetProperty(const char* prop) const
{
  bool chain = false;
  const char *retVal = 
    this->Properties.GetPropertyValue(prop, cmProperty::TEST, chain);
  if (chain)
    {
    return this->Makefile->GetProperty(prop,cmProperty::TEST);
    }
  return retVal;
}

bool cmTest::GetPropertyAsBool(const char* prop) const
{
  return cmSystemTools::IsOn(this->GetProperty(prop));
}

void cmTest::SetProperty(const char* prop, const char* value)
{
  if (!prop)
    {
    return;
    }

  this->Properties.SetProperty(prop, value, cmProperty::TEST);
}

//----------------------------------------------------------------------------
void cmTest::AppendProperty(const char* prop, const char* value)
{
  if (!prop)
    {
    return;
    }
  this->Properties.AppendProperty(prop, value, cmProperty::TEST);
}

//----------------------------------------------------------------------------
void cmTest::SetMakefile(cmMakefile* mf)
{
  // Set our makefile.
  this->Makefile = mf;
  this->Properties.SetCMakeInstance(mf->GetCMakeInstance());
}

// define properties
void cmTest::DefineProperties(cmake *cm)
{
  // define properties
  cm->DefineProperty
    ("FAIL_REGULAR_EXPRESSION", cmProperty::TEST, 
     "If the output matches this regular expression the test will fail.",
     "If set, if the output matches one of "
     "specified regular expressions, the test will fail."
     "For example: PASS_REGULAR_EXPRESSION \"[^a-z]Error;ERROR;Failed\"");

  cm->DefineProperty
    ("MEASUREMENT", cmProperty::TEST, 
     "Specify a DART measurement and value to be reported for a test.",
     "If set to a name then that name will be reported to DART as a "
     "named measurement with a value of 1. You may also specify a value "
     "by setting MEASUREMENT to \"measurement=value\".");

  cm->DefineProperty
    ("PASS_REGULAR_EXPRESSION", cmProperty::TEST, 
     "The output must match this regular expression for the test to pass.",
     "If set, the test output will be checked "
     "against the specified regular expressions and at least one of the"
     " regular expressions has to match, otherwise the test will fail.");

  cm->DefineProperty
    ("TIMEOUT", cmProperty::TEST, 
     "How many seconds to allow for this test.",
     "This property if set will limit a test to not take more than "
     "the specified number of seconds to run. If it exceeds that the "
     "test process will be killed and ctest will move to the next test. "
     "This setting takes precedence over DART_TESTING_TIMEOUT and "
     "CTEST_TESTING_TIMEOUT.");

  cm->DefineProperty
    ("WILL_FAIL", cmProperty::TEST, 
     "If set to true, this will invert the pass/fail flag of the test.",
     "This property can be used for tests that are expected to fail and "
     "return a non zero return code.");
}
