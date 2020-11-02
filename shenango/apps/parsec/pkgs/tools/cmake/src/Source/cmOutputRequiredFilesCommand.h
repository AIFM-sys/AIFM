/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmOutputRequiredFilesCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmOutputRequiredFilesCommand_h
#define cmOutputRequiredFilesCommand_h

#include "cmCommand.h"
#include "cmMakeDepend.h"

/** \class cmOutputRequiredFilesCommand
 * \brief Output a list of required files for a source file
 *
 */
class cmOutputRequiredFilesCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmOutputRequiredFilesCommand;
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
  virtual const char* GetName() { return "output_required_files";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return 
      "Output a list of required source files for a specified source file.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  output_required_files(srcfile outputfile)\n"
      "Outputs a list of all the source files that are required by the "
      "specified srcfile. This list is written into outputfile. This is "
      "similar to writing out the dependencies for srcfile except that it "
      "jumps from .h files into .cxx, .c and .cpp files if possible.";
    }
  
  cmTypeMacro(cmOutputRequiredFilesCommand, cmCommand);
  void ListDependencies(cmDependInformation const *info,
                        FILE *fout,
                        std::set<cmDependInformation const*> *visited);

private:
  std::string File;
  std::string OutputFile;
};



#endif
