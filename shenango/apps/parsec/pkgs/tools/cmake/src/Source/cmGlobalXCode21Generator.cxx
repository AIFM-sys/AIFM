/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmGlobalXCode21Generator.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmGlobalXCode21Generator.h"
#include "cmXCode21Object.h"

//----------------------------------------------------------------------------
cmGlobalXCode21Generator::cmGlobalXCode21Generator()
{
  this->XcodeVersion = 21;
}

//----------------------------------------------------------------------------
void 
cmGlobalXCode21Generator::WriteXCodePBXProj(std::ostream& fout,
                                            cmLocalGenerator* ,
                                            std::vector<cmLocalGenerator*>& )
{
  fout << "// !$*UTF8*$!\n";
  fout << "{\n";
  cmXCode21Object::Indent(1, fout);
  fout << "archiveVersion = 1;\n";
  cmXCode21Object::Indent(1, fout);
  fout << "classes = {\n";
  cmXCode21Object::Indent(1, fout);
  fout << "};\n";
  cmXCode21Object::Indent(1, fout);
  fout << "objectVersion = 42;\n";
  cmXCode21Object::PrintList(this->XCodeObjects, fout);
  cmXCode21Object::Indent(1, fout);
  fout << "rootObject = " << this->RootObject->GetId() 
       << " /* Project object */;\n";
  fout << "}\n";
}
