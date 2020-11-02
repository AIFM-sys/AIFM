/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCursesForm.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmCursesForm.h"

std::ofstream cmCursesForm::DebugFile;
bool cmCursesForm::Debug = false;

cmCursesForm::cmCursesForm()
{
  this->Form = 0;
}

cmCursesForm::~cmCursesForm()
{
  if (this->Form)
    {
    unpost_form(this->Form);
    free_form(this->Form);
    this->Form = 0;
    }
}

void cmCursesForm::DebugStart()
{
  cmCursesForm::Debug = true;
  cmCursesForm::DebugFile.open("ccmakelog.txt");
}

void cmCursesForm::DebugEnd()
{
  if (!cmCursesForm::Debug)
    {
    return;
    }

  cmCursesForm::Debug = false;
  cmCursesForm::DebugFile.close();
}

void cmCursesForm::LogMessage(const char* msg)
{
  if (!cmCursesForm::Debug)
    {
    return;
    }

  cmCursesForm::DebugFile << msg << std::endl;
}
