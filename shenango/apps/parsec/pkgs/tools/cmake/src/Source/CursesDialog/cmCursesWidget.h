/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCursesWidget.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef __cmCursesWidget_h
#define __cmCursesWidget_h

#include "../cmCacheManager.h"
#include "cmCursesStandardIncludes.h"

class cmCursesMainForm;

class cmCursesWidget
{
public:
  cmCursesWidget(int width, int height, int left, int top);
  virtual ~cmCursesWidget();
  
  /**
   * Handle user input. Called by the container of this widget
   * when this widget has focus. Returns true if the input was
   * handled
   */
  virtual bool HandleInput(int& key, cmCursesMainForm* fm, WINDOW* w) = 0;

  /**
   * Change the position of the widget. Set isNewPage to true
   * if this widget marks the beginning of a new page.
   */
  virtual void Move(int x, int y, bool isNewPage);

  /**
   * Set/Get the value (setting the value also changes the contents
   * of the field buffer).
   */
  virtual void SetValue(const char* value);
  virtual const char* GetValue();

  /**
   * Get the type of the widget (STRING, PATH etc...)
   */
  cmCacheManager::CacheEntryType GetType()
    { return this->Type; }

  /**
   * If there are any, print the widget specific commands
   * in the toolbar and return true. Otherwise, return false
   * and the parent widget will print.
   */
  virtual bool PrintKeys()
    {
      return false;
    }

  /**
   * Set/Get the page this widget is in.
   */
  void SetPage(int page)
    {
      this->Page = page;
    }
  int GetPage()
    {
      return this->Page;
    }

  friend class cmCursesMainForm;

protected:
  cmCursesWidget(const cmCursesWidget& from);
  void operator=(const cmCursesWidget&);

  cmCacheManager::CacheEntryType Type;
  std::string Value;
  FIELD* Field;
  // The page in the main form this widget is in
  int Page;
};

#endif // __cmCursesWidget_h
