/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmExecutionStatus.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmExecutionStatus_h
#define cmExecutionStatus_h

#include "cmObject.h"

/** \class cmExecutionStatus
 * \brief Superclass for all command status classes
 *
 * when a command is involked it may set values on a command status instance
 */
class cmExecutionStatus : public cmObject
{
public:
  cmTypeMacro(cmExecutionStatus, cmObject);
  
  cmExecutionStatus() { this->Clear();};
  
  virtual void SetReturnInvoked(bool val) 
  { this->ReturnInvoked = val; }
  virtual bool GetReturnInvoked()
  { return this->ReturnInvoked; }
                                 
  virtual void SetBreakInvoked(bool val) 
  { this->BreakInvoked = val; }
  virtual bool GetBreakInvoked()
  { return this->BreakInvoked; }
            
  virtual void Clear()
    {
    this->ReturnInvoked = false;
    this->BreakInvoked = false;
    this->NestedError = false;
    }
  virtual void SetNestedError(bool val) { this->NestedError = val; }
  virtual bool GetNestedError() { return this->NestedError; }

                                        
protected:
  bool ReturnInvoked;
  bool BreakInvoked;
  bool NestedError;
};

#endif
