/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmSetPropertyCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmSetsPropertiesCommand_h
#define cmSetsPropertiesCommand_h

#include "cmCommand.h"

class cmSetPropertyCommand : public cmCommand
{
public:
  cmSetPropertyCommand();

  virtual cmCommand* Clone()
    {
      return new cmSetPropertyCommand;
    }

  /**
   * This is called when the command is first encountered in
   * the input file.
   */
  virtual bool InitialPass(std::vector<std::string> const& args,
                           cmExecutionStatus &status);

  /**
   * The name of the command as specified in CMakeList.txt.
   */
  virtual const char* GetName() { return "set_property";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation()
    {
    return "Set a named property in a given scope.";
    }

  /**
   * Longer documentation.
   */
  virtual const char* GetFullDocumentation()
    {
      return
        "  set_property(<GLOBAL                            |\n"
        "                DIRECTORY [dir]                   |\n"
        "                TARGET    [target1 [target2 ...]] |\n"
        "                SOURCE    [src1 [src2 ...]]       |\n"
        "                TEST      [test1 [test2 ...]]>\n"
        "               [APPEND]\n"
        "               PROPERTY <name> [value1 [value2 ...]])\n"
        "Set one property on zero or more objects of a scope.  "
        "The first argument determines the scope in which the property "
        "is set.  It must be one of the following:\n"
        "GLOBAL scope is unique and does not accept a name.\n"
        "DIRECTORY scope defaults to the current directory but another "
        "directory (already processed by CMake) may be named by full or "
        "relative path.\n"
        "TARGET scope may name zero or more existing targets.\n"
        "SOURCE scope may name zero or more source files.\n"
        "TEST scope may name zero or more existing tests.\n"
        "The required PROPERTY option is immediately followed by the name "
        "of the property to set.  Remaining arguments are used to "
        "compose the property value in the form of a semicolon-separated "
        "list.  "
        "If the APPEND option is given the list is appended to any "
        "existing property value."
        ;
    }

  /**
   * This determines if the command is invoked when in script mode.
   */
  virtual bool IsScriptable() { return true; }

  cmTypeMacro(cmSetPropertyCommand, cmCommand);

private:
  std::set<cmStdString> Names;
  std::string PropertyName;
  std::string PropertyValue;
  bool Remove;
  bool AppendMode;

  // Implementation of each property type.
  bool HandleGlobalMode();
  bool HandleDirectoryMode();
  bool HandleTargetMode();
  bool HandleTarget(cmTarget* target);
  bool HandleSourceMode();
  bool HandleSource(cmSourceFile* sf);
  bool HandleTestMode();
  bool HandleTest(cmTest* test);
};



#endif
