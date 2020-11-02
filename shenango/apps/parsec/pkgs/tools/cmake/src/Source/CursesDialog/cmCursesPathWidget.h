/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCursesPathWidget.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef __cmCursesPathWidget_h
#define __cmCursesPathWidget_h

#include "cmCursesStringWidget.h"

class cmCursesPathWidget : public cmCursesStringWidget
{
public:
  cmCursesPathWidget(int width, int height, int left, int top);

  /**
   * This method is called when different keys are pressed. The
   * subclass can have a special implementation handler for this.
   */
  virtual void OnTab(cmCursesMainForm* fm, WINDOW* w);
  virtual void OnReturn(cmCursesMainForm* fm, WINDOW* w);
  virtual void OnType(int& key, cmCursesMainForm* fm, WINDOW* w);

protected:
  cmCursesPathWidget(const cmCursesPathWidget& from);
  void operator=(const cmCursesPathWidget&);

  std::string LastString;
  std::string LastGlob;
  bool Cycle;
  std::string::size_type CurrentIndex;
};

#endif // __cmCursesPathWidget_h
