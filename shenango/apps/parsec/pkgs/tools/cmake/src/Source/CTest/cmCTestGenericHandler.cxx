/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCTestGenericHandler.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include "cmCTestGenericHandler.h"
#include "cmSystemTools.h"

#include "cmCTest.h"

//----------------------------------------------------------------------
cmCTestGenericHandler::cmCTestGenericHandler()
{
  this->HandlerVerbose = false;
  this->CTest = 0;
  this->SubmitIndex = 0;
}

//----------------------------------------------------------------------
cmCTestGenericHandler::~cmCTestGenericHandler()
{
}

//----------------------------------------------------------------------
void cmCTestGenericHandler::SetOption(const char* op, const char* value)
{
  if ( !op )
    {
    return;
    }
  if ( !value )
    {
    cmCTestGenericHandler::t_StringToString::iterator remit
      = this->Options.find(op);
    if ( remit != this->Options.end() )
      {
      this->Options.erase(remit);
      }
    return;
    }

  this->Options[op] = value;
}

//----------------------------------------------------------------------
void cmCTestGenericHandler::SetPersistentOption(const char* op, 
                                                const char* value)
{
  this->SetOption(op, value);
  if ( !op )
    {
    return;
    }
  if ( !value )
    {
    cmCTestGenericHandler::t_StringToString::iterator remit
      = this->PersistentOptions.find(op);
    if ( remit != this->PersistentOptions.end() )
      {
      this->PersistentOptions.erase(remit);
      }
    return;
    }

  this->PersistentOptions[op] = value;
}

//----------------------------------------------------------------------
void cmCTestGenericHandler::Initialize()
{
  this->Options.clear();
  t_StringToString::iterator it;
  for ( it = this->PersistentOptions.begin(); 
    it != this->PersistentOptions.end();
    ++ it )
    {
    this->Options[it->first.c_str()] = it->second.c_str();
    }
}

//----------------------------------------------------------------------
const char* cmCTestGenericHandler::GetOption(const char* op)
{
  cmCTestGenericHandler::t_StringToString::iterator remit
    = this->Options.find(op);
  if ( remit == this->Options.end() )
    {
    return 0;
    }
  return remit->second.c_str();
}

//----------------------------------------------------------------------
bool cmCTestGenericHandler::StartResultingXML(const char* name,
  cmGeneratedFileStream& xofs)
{
  if ( !name )
    {
    cmCTestLog(this->CTest, ERROR_MESSAGE,
      "Cannot create resulting XML file without providing the name"
      << std::endl;);
    return false;
    }
  cmOStringStream ostr;
  ostr << name;
  if ( this->SubmitIndex > 0 )
    {
    ostr << "_" << this->SubmitIndex;
    }
  ostr << ".xml"; 
  if(this->CTest->GetCurrentTag().empty())
    {
    cmCTestLog(this->CTest, ERROR_MESSAGE,
               "Current Tag empty, this may mean"
               " NightlyStartTime was not set correctly." << std::endl);
    cmSystemTools::SetFatalErrorOccured();
    return false;
    }
  if( !this->CTest->OpenOutputFile(this->CTest->GetCurrentTag(),
      ostr.str().c_str(), xofs, true) )
    {
    cmCTestLog(this->CTest, ERROR_MESSAGE,
      "Cannot create resulting XML file: " << ostr.str().c_str()
      << std::endl);
    return false;
    }
  this->CTest->AddSubmitFile(ostr.str().c_str());
  return true;
}

//----------------------------------------------------------------------
bool cmCTestGenericHandler::StartLogFile(const char* name,
  cmGeneratedFileStream& xofs)
{
  if ( !name )
    {
    cmCTestLog(this->CTest, ERROR_MESSAGE,
      "Cannot create log file without providing the name" << std::endl;);
    return false;
    }
  cmOStringStream ostr;
  ostr << "Last" << name;
  if ( this->SubmitIndex > 0 )
    {
    ostr << "_" << this->SubmitIndex;
    }
  if ( !this->CTest->GetCurrentTag().empty() )
    {
    ostr << "_" << this->CTest->GetCurrentTag();
    }
  ostr << ".log";
  if( !this->CTest->OpenOutputFile("Temporary", ostr.str().c_str(), xofs) )
    {
    cmCTestLog(this->CTest, ERROR_MESSAGE, "Cannot create log file: "
      << ostr.str().c_str() << std::endl);
    return false;
    }
  return true;
}

