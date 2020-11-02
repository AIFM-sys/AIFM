/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmGlobalVisualStudio6Generator.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmGlobalVisualStudio6Generator_h
#define cmGlobalVisualStudio6Generator_h

#include "cmGlobalVisualStudioGenerator.h"

class cmTarget;

/** \class cmGlobalVisualStudio6Generator
 * \brief Write a Unix makefiles.
 *
 * cmGlobalVisualStudio6Generator manages UNIX build process for a tree
 */
class cmGlobalVisualStudio6Generator : public cmGlobalVisualStudioGenerator
{
public:
  cmGlobalVisualStudio6Generator();
  static cmGlobalGenerator* New() { 
    return new cmGlobalVisualStudio6Generator; }
  
  ///! Get the name for the generator.
  virtual const char* GetName() const {
    return cmGlobalVisualStudio6Generator::GetActualName();}
  static const char* GetActualName() {return "Visual Studio 6";}

  /** Get the documentation entry for this generator.  */
  virtual void GetDocumentation(cmDocumentationEntry& entry) const;
  
  ///! Create a local generator appropriate to this Global Generator
  virtual cmLocalGenerator *CreateLocalGenerator();

  /**
   * Try to determine system infomation such as shared library
   * extension, pthreads, byte order etc.  
   */
  virtual void EnableLanguage(std::vector<std::string>const& languages, 
                              cmMakefile *, bool optional);

  /**
   * Try running cmake and building a file. This is used for dynalically
   * loaded commands, not as part of the usual build process.
   */
  virtual std::string GenerateBuildCommand(const char* makeProgram,
                                           const char *projectName,
                                           const char* additionalOptions, 
                                           const char *targetName, 
                                           const char* config,
                                           bool ignoreErrors,
                                           bool fast);

  /**
   * Generate the all required files for building this project/tree. This
   * basically creates a series of LocalGenerators for each directory and
   * requests that they Generate.  
   */
  virtual void Generate();

  /**
   * Generate the DSW workspace file.
   */
  virtual void OutputDSWFile();
  virtual void OutputDSWFile(cmLocalGenerator* root,
                             std::vector<cmLocalGenerator*>& generators);
  virtual void WriteDSWFile(std::ostream& fout,
                            cmLocalGenerator* root,
                            std::vector<cmLocalGenerator*>& generators);

  /** Append the subdirectory for the given configuration.  */
  virtual void AppendDirectoryForConfig(const char* prefix,
                                        const char* config,
                                        const char* suffix,
                                        std::string& dir);

  ///! What is the configurations directory variable called?
  virtual const char* GetCMakeCFGInitDirectory()  { return "$(IntDir)"; }
private:
  void GenerateConfigurations(cmMakefile* mf);
  void WriteDSWFile(std::ostream& fout);
  void WriteDSWHeader(std::ostream& fout);
  void WriteProject(std::ostream& fout, 
                    const char* name, const char* path, cmTarget &t);
  void WriteExternalProject(std::ostream& fout, 
                            const char* name, const char* path,
                            const std::vector<std::string>& dependencies);
  void WriteDSWFooter(std::ostream& fout);
};

#endif
