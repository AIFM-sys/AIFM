/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmInstallFilesCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmInstallFilesCommand_h
#define cmInstallFilesCommand_h

#include "cmCommand.h"

/** \class cmInstallFilesCommand
 * \brief Specifies where to install some files
 *
 * cmInstallFilesCommand specifies the relative path where a list of
 * files should be installed.  
 */
class cmInstallFilesCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmInstallFilesCommand;
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
  virtual const char* GetName() { return "install_files";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Deprecated.  Use the install(FILES ) command instead.";
    }
  
  /**
   * This is called at the end after all the information
   * specified by the command is accumulated. Most commands do
   * not implement this method.  At this point, reading and
   * writing to the cache can be done.
   */
  virtual void FinalPass();

  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "This command has been superceded by the install command.  It "
      "is provided for compatibility with older CMake code.  "
      "The FILES form is directly replaced by the FILES form of the "
      "install command.  The regexp form can be expressed "
      "more clearly using the GLOB form of the file command.\n"
      "  install_files(<dir> extension file file ...)\n"
      "Create rules to install the listed files with the given extension "
      "into the given directory.  "
      "Only files existing in the current source tree or its corresponding "
      "location in the binary tree may be listed.  "
      "If a file specified already has an extension, that extension will be "
      "removed first.  This is useful for providing lists of source files "
      "such as foo.cxx when you want the corresponding foo.h to be "
      "installed. A typical extension is '.h'.\n"
      "  install_files(<dir> regexp)\n"
      "Any files in the current source directory that match the regular "
      "expression will be installed.\n"
      "  install_files(<dir> FILES file file ...)\n"
      "Any files listed after the FILES keyword will be "
      "installed explicitly from the names given.  Full paths are allowed in "
      "this form.\n"
      "The directory <dir> is relative to the installation prefix, which "
      "is stored in the variable CMAKE_INSTALL_PREFIX.";
    }
  
  /** This command is kept for compatibility with older CMake versions. */
  virtual bool IsDiscouraged()
    {
    return true;
    }

  cmTypeMacro(cmInstallFilesCommand, cmCommand);

protected:
  void CreateInstallGenerator() const;
  std::string FindInstallSource(const char* name) const;
  
 private:
  std::vector<std::string> FinalArgs;
  bool IsFilesForm;
  std::string Destination;
  std::vector<std::string> Files;
};


#endif
