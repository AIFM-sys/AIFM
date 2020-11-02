/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmIncludeRegularExpressionCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmIncludeRegularExpressionCommand_h
#define cmIncludeRegularExpressionCommand_h

#include "cmCommand.h"

/** \class cmIncludeRegularExpressionCommand
 * \brief Set the regular expression for following #includes.
 *
 * cmIncludeRegularExpressionCommand is used to specify the regular expression
 * that determines whether to follow a #include file in dependency checking.
 */
class cmIncludeRegularExpressionCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmIncludeRegularExpressionCommand;
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
  virtual const char* GetName() {return "include_regular_expression";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "Set the regular expression used for dependency checking.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  include_regular_expression(regex_match [regex_complain])\n"
      "Set the regular expressions used in dependency checking.  Only files "
      "matching regex_match will be traced as dependencies.  Only files "
      "matching regex_complain will generate warnings if they cannot be "
      "found "
      "(standard header paths are not searched).  The defaults are:\n"
      "  regex_match    = \"^.*$\" (match everything)\n"
      "  regex_complain = \"^$\" (match empty string only)";
    }
  
  cmTypeMacro(cmIncludeRegularExpressionCommand, cmCommand);
};



#endif
