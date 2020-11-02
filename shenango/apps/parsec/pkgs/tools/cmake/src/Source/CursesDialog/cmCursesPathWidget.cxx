/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCursesPathWidget.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmCursesPathWidget.h"

#include "cmCursesMainForm.h"
#include "cmSystemTools.h"

cmCursesPathWidget::cmCursesPathWidget(int width, int height, 
                                           int left, int top) :
  cmCursesStringWidget(width, height, left, top)
{
  this->Type = cmCacheManager::PATH;
  this->Cycle = false;
  this->CurrentIndex = 0;
}

void cmCursesPathWidget::OnType(int& key, cmCursesMainForm* fm, WINDOW* w)
{
  this->Cycle = false;
  this->CurrentIndex = 0;
  this->LastGlob = "";
  this->cmCursesStringWidget::OnType(key, fm, w);
}

void cmCursesPathWidget::OnTab(cmCursesMainForm* fm, WINDOW* w)
{
  if ( !this->GetString() )
    {
    return;
    }
  FORM* form = fm->GetForm();
  form_driver(form, REQ_NEXT_FIELD);
  form_driver(form, REQ_PREV_FIELD);
  std::string cstr = this->GetString();
  cstr = cstr.substr(0, cstr.find_last_not_of(" \t\n\r")+1);
  if ( this->LastString != cstr )
    {
    this->Cycle = false;
    this->CurrentIndex = 0;
    this->LastGlob = "";
    }
  std::string glob;
  if ( this->Cycle )
    {
    glob = this->LastGlob;
    }
  else
    {
    glob = cstr + "*";
    }
  std::vector<cmStdString> dirs;

  cmSystemTools::SimpleGlob(glob.c_str(), dirs, (this->Type == cmCacheManager::PATH?-1:0));
  if ( this->CurrentIndex < dirs.size() )
    {
    cstr = dirs[this->CurrentIndex];
    }
  if ( cstr[cstr.size()-1] == '*' )
    {
    cstr = cstr.substr(0, cstr.size()-1);
    }

  if ( cmSystemTools::FileIsDirectory(cstr.c_str()) )
    {
    cstr += "/";
    }

  this->SetString(cstr.c_str());
  touchwin(w); 
  wrefresh(w); 
  form_driver(form, REQ_END_FIELD);
  this->LastGlob = glob;
  this->LastString = cstr;
  this->Cycle = true;
  this->CurrentIndex ++;
  if ( this->CurrentIndex >= dirs.size() )
    {
    this->CurrentIndex = 0;
    }
}

void cmCursesPathWidget::OnReturn(cmCursesMainForm* fm, WINDOW* w)
{
  this->cmCursesStringWidget::OnReturn(fm, w);
}
