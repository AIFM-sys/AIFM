/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmFLTKWrapUICommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmFLTKWrapUICommand_h
#define cmFLTKWrapUICommand_h

#include "cmCommand.h"

/** \class cmFLTKWrapUICommand
 * \brief Create .h and .cxx files rules for FLTK user interfaces files
 *
 * cmFLTKWrapUICommand is used to create wrappers for FLTK classes into
 * normal C++
 */
class cmFLTKWrapUICommand : public cmCommand
{
public:
  cmTypeMacro(cmFLTKWrapUICommand, cmCommand);

  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmFLTKWrapUICommand;
    }

  /**
   * This is called when the command is first encountered in
   * the CMakeLists.txt file.
   */
  virtual bool InitialPass(std::vector<std::string> const& args,
                           cmExecutionStatus &status);
  
  /**
   * This is called at the end after all the information
   * specified by the command is accumulated. Most commands do
   * not implement this method.  At this point, reading and
   * writing to the cache can be done.
   */
  virtual void FinalPass();

  /**
   * The name of the command as specified in CMakeList.txt.
   */
  virtual const char* GetName() { return "fltk_wrap_ui";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Create FLTK user interfaces Wrappers.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  fltk_wrap_ui(resultingLibraryName source1\n"
      "               source2 ... sourceN )\n"
      "Produce .h and .cxx files for all the .fl and .fld files listed.  "
      "The resulting .h and .cxx files will be added to a variable named "
      "resultingLibraryName_FLTK_UI_SRCS which should be added to your "
      "library.";
    }
  
private:
  /**
   * List of produced files.
   */
  std::vector<cmSourceFile *> GeneratedSourcesClasses;

  /**
   * List of Fluid files that provide the source 
   * generating .cxx and .h files
   */
  std::string Target;
};



#endif
