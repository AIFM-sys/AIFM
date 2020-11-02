/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCursesForm.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef __cmCursesForm_h
#define __cmCursesForm_h

#include "../cmStandardIncludes.h"
#include "cmCursesStandardIncludes.h"

class cmCursesForm
{
public:
  cmCursesForm();
  virtual ~cmCursesForm();
  
  // Description:
  // Handle user input.
  virtual void HandleInput() = 0;

  // Description:
  // Display form.
  virtual void Render(int left, int top, int width, int height) = 0;

  // Description:
  // This method should normally  called only by the form.
  // The only exception is during a resize.
  virtual void UpdateStatusBar() = 0;

  // Description:
  // During a CMake run, an error handle should add errors
  // to be displayed afterwards.
  virtual void AddError(const char*, const char*) {}

  // Description:
  // Turn debugging on. This will create ccmakelog.txt.
  static void DebugStart();

  // Description:
  // Turn debugging off. This will close ccmakelog.txt.
  static void DebugEnd();

  // Description:
  // Write a debugging message.
  static void LogMessage(const char* msg);

  // Description:
  // Return the FORM. Should be only used by low-level methods.
  FORM* GetForm()
    {
      return this->Form;
    }

  static cmCursesForm* CurrentForm;
  

protected:

  static std::ofstream DebugFile;
  static bool Debug;

  cmCursesForm(const cmCursesForm& form);
  void operator=(const cmCursesForm&);

  FORM* Form;
};

#endif // __cmCursesForm_h
