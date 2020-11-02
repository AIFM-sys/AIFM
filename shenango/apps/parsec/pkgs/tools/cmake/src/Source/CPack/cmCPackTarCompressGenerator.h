/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCPackTarCompressGenerator.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc. All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#ifndef cmCPackTarCompressGenerator_h
#define cmCPackTarCompressGenerator_h

#include "cmCPackTGZGenerator.h"

/** \class cmCPackTarCompressGenerator
 * \brief A generator for TarCompress files
 */
class cmCPackTarCompressGenerator : public cmCPackTGZGenerator
{
public:
  friend class cmCPackTarCompressGeneratorForward;
  cmCPackTypeMacro(cmCPackTarCompressGenerator, cmCPackTGZGenerator);

  /**
   * Construct generator
   */
  cmCPackTarCompressGenerator();
  virtual ~cmCPackTarCompressGenerator();

protected:
  virtual int InitializeInternal();
  int CompressFiles(const char* outFileName, const char* toplevel,
    const std::vector<std::string>& files);
  virtual const char* GetOutputExtension() { return ".tar.Z"; }

  int RenameFile(const char* oldname, const char* newname);
  int GenerateHeader(std::ostream* os);
};

#endif
