/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmQTWrapUICommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmQTWrapUICommand_h
#define cmQTWrapUICommand_h

#include "cmCommand.h"

#include "cmSourceFile.h"

/** \class cmQTWrapUICommand
 * \brief Create .h and .cxx files rules for QT user interfaces files
 *
 * cmQTWrapUICommand is used to create wrappers for QT classes into normal C++
 */
class cmQTWrapUICommand : public cmCommand
{
public:
  cmTypeMacro(cmQTWrapUICommand, cmCommand);
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmQTWrapUICommand;
    }

  /**
   * This is called when the command is first encountered in
   * the CMakeLists.txt file.
   */
  virtual bool InitialPass(std::vector<std::string> const& args,
                           cmExecutionStatus &status);

  /**
   * The name of the command as specified in CMakeList.txt.
   */
  virtual const char* GetName() { return "qt_wrap_ui";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Create Qt user interfaces Wrappers.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  qt_wrap_ui(resultingLibraryName HeadersDestName\n"
      "             SourcesDestName SourceLists ...)\n"
      "Produce .h and .cxx files for all the .ui files listed "
      "in the SourceLists.  "
      "The .h files will be added to the library using the HeadersDestName"
      "source list.  "
      "The .cxx files will be added to the library using the SourcesDestName"
      "source list.";
    }
};



#endif
