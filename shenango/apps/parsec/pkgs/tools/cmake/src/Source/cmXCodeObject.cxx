/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmXCodeObject.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:07 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmXCodeObject.h"
#include "cmSystemTools.h"

//----------------------------------------------------------------------------
const char* cmXCodeObject::PBXTypeNames[] = {
    "PBXGroup", "PBXBuildStyle", "PBXProject", "PBXHeadersBuildPhase", 
    "PBXSourcesBuildPhase", "PBXFrameworksBuildPhase", "PBXNativeTarget",
    "PBXFileReference", "PBXBuildFile", "PBXContainerItemProxy", 
    "PBXTargetDependency", "PBXShellScriptBuildPhase", 
    "PBXResourcesBuildPhase", "PBXApplicationReference",
    "PBXExecutableFileReference", "PBXLibraryReference", "PBXToolTarget",
    "PBXLibraryTarget", "PBXAggregateTarget", "XCBuildConfiguration", 
    "XCConfigurationList",
    "PBXCopyFilesBuildPhase",
    "None"
  };

//----------------------------------------------------------------------------
cmXCodeObject::~cmXCodeObject()
{
  this->Version = 15;
}

//----------------------------------------------------------------------------
cmXCodeObject::cmXCodeObject(PBXType ptype, Type type)
{
  this->Version = 15;
  this->PBXTargetDependencyValue = 0;
  this->Target = 0;
  this->Object =0;
  
  this->IsA = ptype;
  if(type == OBJECT)
    {
    cmOStringStream str;
    str << (void*)this;
    str << (void*)this;
    str << (void*)this;
    this->Id = str.str();
    }
  else
    {
    this->Id = 
      "Temporary cmake object, should not be refered to in xcode file";
    }
  cmSystemTools::ReplaceString(this->Id, "0x", "");
  this->Id = cmSystemTools::UpperCase(this->Id);
  if(this->Id.size() < 24)
    {
    int diff = 24 - this->Id.size();
    for(int i =0; i < diff; ++i)
      {
      this->Id += "0";
      }
    }
  if(this->Id.size() > 24)
    {
    this->Id = this->Id.substr(0,24);
    }
  this->TypeValue = type;
  if(this->TypeValue == OBJECT)
    {
    this->AddAttribute("isa", 0);
    }
}

//----------------------------------------------------------------------------
void cmXCodeObject::Indent(int level, std::ostream& out)
{
  while(level)
    {
    out << "       ";
    level--;
    }
}

//----------------------------------------------------------------------------
void cmXCodeObject::Print(std::ostream& out)
{
  std::string separator = "\n";
  int indentFactor = 1;
  if(this->Version > 15 
     && (this->IsA == PBXFileReference || this->IsA == PBXBuildFile))
    {
    separator = " ";
    indentFactor = 0;
    }
  cmXCodeObject::Indent(2*indentFactor, out);
  out << this->Id << " ";
  if(!(this->IsA == PBXGroup && this->Comment.size() == 0))
    {
    this->PrintComment(out);
    }
  out << " = {";
  if(separator == "\n")
    {
    out << separator;
    }
  std::map<cmStdString, cmXCodeObject*>::iterator i;
  cmXCodeObject::Indent(3*indentFactor, out);
  out << "isa = " << PBXTypeNames[this->IsA]  << ";" << separator;
  for(i = this->ObjectAttributes.begin(); 
      i != this->ObjectAttributes.end(); ++i)
    { 
    cmXCodeObject* object = i->second;
    if(i->first != "isa")
      { 
      cmXCodeObject::Indent(3*indentFactor, out);
      }
    else
      {
      continue;
      }
    if(object->TypeValue == OBJECT_LIST)
      {
      out << i->first << " = (" << separator;
      for(unsigned int k = 0; k < i->second->List.size(); k++)
        {
        cmXCodeObject::Indent(4*indentFactor, out);
        out << i->second->List[k]->Id << " ";
        i->second->List[k]->PrintComment(out);
        out << "," << separator;
        } 
      cmXCodeObject::Indent(3*indentFactor, out);
      out << ");" << separator;
      }
    else if(object->TypeValue == ATTRIBUTE_GROUP)
      {
      std::map<cmStdString, cmXCodeObject*>::iterator j;
      out << i->first << " = {" << separator;
      for(j = object->ObjectAttributes.begin(); j != 
            object->ObjectAttributes.end(); ++j)
        {
        cmXCodeObject::Indent(4 *indentFactor, out);

        if(j->second->TypeValue == STRING)
          {
          out << j->first << " = " << j->second->String << ";";
          }
        else if(j->second->TypeValue == OBJECT_LIST)
          {
          out << j->first << " = (";
          for(unsigned int k = 0; k < j->second->List.size(); k++)
            {
            if(j->second->List[k]->TypeValue == STRING)
              {
              out << j->second->List[k]->String << ", ";
              }
            else
              {
              out << "List_" << k << "_TypeValue_IS_NOT_STRING, ";
              }
            }
          out << ");";
          }
        else
          {
          out << j->first << " = error_unexpected_TypeValue_" <<
            j->second->TypeValue << ";";
          }

        out << separator;
        }
      cmXCodeObject::Indent(3 *indentFactor, out);
      out << "};" << separator;
      }
    else if(object->TypeValue == OBJECT_REF)
      {
      out << i->first << " = " << object->Object->Id;
      if(object->Object->HasComment() && i->first != "remoteGlobalIDString")
        {
        out << " ";
        object->Object->PrintComment(out);
        }
      out << ";" << separator;
      }
    else if(object->TypeValue == STRING)
      {
      out << i->first << " = " << object->String << ";" << separator;
      }
    else
      {
      out << "what is this?? " << i->first << "\n";
      }
    }
  cmXCodeObject::Indent(2*indentFactor, out);
  out << "};\n";
}
  
//----------------------------------------------------------------------------
void cmXCodeObject::PrintList(std::vector<cmXCodeObject*> const& objs,
                              std::ostream& out)
{ 
  cmXCodeObject::Indent(1, out);
  out << "objects = {\n";
  for(unsigned int i = 0; i < objs.size(); ++i)
    {
    if(objs[i]->TypeValue == OBJECT)
      {
      objs[i]->Print(out);
      }
    }
  cmXCodeObject::Indent(1, out);
  out << "};\n";
}

//----------------------------------------------------------------------------
void cmXCodeObject::CopyAttributes(cmXCodeObject* copy)
{
  this->ObjectAttributes = copy->ObjectAttributes;
  this->List = copy->List;
  this->String = copy->String;
  this->Object = copy->Object;
}

//----------------------------------------------------------------------------
void cmXCodeObject::SetString(const char* s)
{
  std::string ss = s;
  if(ss.size() == 0)
    {
    this->String = "\"\"";
    return;
    }
  // escape quotes
  cmSystemTools::ReplaceString(ss, "\"", "\\\"");
  bool needQuote = false;
  this->String = "";
  if(ss.find_first_of(" <>.+-=@") != ss.npos)
    {
    needQuote = true;
    }
  if(needQuote)
    {
    this->String = "\"";
    }
  this->String += ss;
  if(needQuote)
    {
    this->String += "\"";
    }
}
