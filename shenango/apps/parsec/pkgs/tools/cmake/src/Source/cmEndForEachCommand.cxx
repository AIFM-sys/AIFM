/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmEndForEachCommand.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmEndForEachCommand.h"

bool cmEndForEachCommand
::InvokeInitialPass(std::vector<cmListFileArgument> const&, 
                    cmExecutionStatus &)
{
  this->SetError("An ENDFOREACH command was found outside of a proper "
                 "FOREACH ENDFOREACH structure. Or its arguments did "
                 "not match the opening FOREACH command.");
  return false;
}

