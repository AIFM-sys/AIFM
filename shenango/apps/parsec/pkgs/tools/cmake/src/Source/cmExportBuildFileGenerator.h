/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmExportBuildFileGenerator.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmExportBuildFileGenerator_h
#define cmExportBuildFileGenerator_h

#include "cmExportFileGenerator.h"

class cmExportCommand;

/** \class cmExportBuildFileGenerator
 * \brief Generate a file exporting targets from a build tree.
 *
 * cmExportBuildFileGenerator generates a file exporting targets from
 * a build tree.  A single file exports information for all
 * configurations built.
 *
 * This is used to implement the EXPORT() command.
 */
class cmExportBuildFileGenerator: public cmExportFileGenerator
{
public:
  cmExportBuildFileGenerator();

  /** Set the list of targets to export.  */
  void SetExports(std::vector<cmTarget*> const* exports)
    { this->Exports = exports; }

  /** Set whether to append generated code to the output file.  */
  void SetAppendMode(bool append) { this->AppendMode = append; }

  /** Set the command instance through which errors should be reported.  */
  void SetCommand(cmExportCommand* cmd) { this->ExportCommand = cmd; }
protected:
  // Implement virtual methods from the superclass.
  virtual bool GenerateMainFile(std::ostream& os);
  virtual void GenerateImportTargetsConfig(std::ostream& os,
                                           const char* config,
                                           std::string const& suffix);
  virtual void ComplainAboutMissingTarget(cmTarget* depender,
                                          cmTarget* dependee);

  /** Fill in properties indicating built file locations.  */
  void SetImportLocationProperty(const char* config,
                                 std::string const& suffix,
                                 cmTarget* target,
                                 ImportPropertyMap& properties);

  std::vector<cmTarget*> const* Exports;
  cmExportCommand* ExportCommand;
};

#endif
