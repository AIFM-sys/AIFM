/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCTestTestCommand.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmCTestTestCommand.h"

#include "cmCTest.h"
#include "cmCTestGenericHandler.h"

cmCTestTestCommand::cmCTestTestCommand()
{
  this->Arguments[ctt_START] = "START";
  this->Arguments[ctt_END] = "END";
  this->Arguments[ctt_STRIDE] = "STRIDE";
  this->Arguments[ctt_LAST] = 0;
  this->Last = ctt_LAST;
}

cmCTestGenericHandler* cmCTestTestCommand::InitializeHandler()
{
  const char* ctestTimeout = 
    this->Makefile->GetDefinition("CTEST_TEST_TIMEOUT");
  double timeout = this->CTest->GetTimeOut();
  if ( ctestTimeout )
    {
    timeout = atof(ctestTimeout);
    }
  else
    {
    if ( timeout <= 0 )
      {
      // By default use timeout of 10 minutes
      timeout = 600;
      }
    }
  this->CTest->SetTimeOut(timeout);
  cmCTestGenericHandler* handler = this->InitializeActualHandler();
  if ( this->Values[ctt_START] || this->Values[ctt_END] ||
    this->Values[ctt_STRIDE] )
    {
    cmOStringStream testsToRunString;
    if ( this->Values[ctt_START] )
      {
      testsToRunString << this->Values[ctt_START];
      }
    testsToRunString << ",";
    if ( this->Values[ctt_END] )
      {
      testsToRunString << this->Values[ctt_END];
      }
    testsToRunString << ",";
    if ( this->Values[ctt_STRIDE] )
      {
      testsToRunString << this->Values[ctt_STRIDE];
      }
    handler->SetOption("TestsToRunInformation",
      testsToRunString.str().c_str());
    }
  return handler;
}

cmCTestGenericHandler* cmCTestTestCommand::InitializeActualHandler()
{
  return this->CTest->GetInitializedHandler("test");
}

