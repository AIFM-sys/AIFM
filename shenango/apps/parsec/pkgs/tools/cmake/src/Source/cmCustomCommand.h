/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCustomCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:07 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmCustomCommand_h
#define cmCustomCommand_h

#include "cmStandardIncludes.h"

/** \class cmCustomCommand
 * \brief A class to encapsulate a custom command
 *
 * cmCustomCommand encapsulates the properties of a custom command
 */
class cmCustomCommand
{
public:
  /** Default and copy constructors for STL containers.  */
  cmCustomCommand();
  cmCustomCommand(const cmCustomCommand& r);

  /** Main constructor specifies all information for the command.  */
  cmCustomCommand(const std::vector<std::string>& outputs,
                  const std::vector<std::string>& depends,
                  const cmCustomCommandLines& commandLines,
                  const char* comment,
                  const char* workingDirectory);

  /** Get the output file produced by the command.  */
  const std::vector<std::string>& GetOutputs() const;

  /** Get the working directory.  */
  const char* GetWorkingDirectory() const;

  /** Get the vector that holds the list of dependencies.  */
  const std::vector<std::string>& GetDepends() const;

  /** Get the list of command lines.  */
  const cmCustomCommandLines& GetCommandLines() const;

  /** Get the comment string for the command.  */
  const char* GetComment() const;

  /** Append to the list of command lines.  */
  void AppendCommands(const cmCustomCommandLines& commandLines);

  /** Append to the list of dependencies.  */
  void AppendDepends(const std::vector<std::string>& depends);

  /** Set/Get whether old-style escaping should be used.  */
  bool GetEscapeOldStyle() const;
  void SetEscapeOldStyle(bool b);

  /** Set/Get whether the build tool can replace variables in
      arguments to the command.  */
  bool GetEscapeAllowMakeVars() const;
  void SetEscapeAllowMakeVars(bool b);

  typedef std::pair<cmStdString, cmStdString> ImplicitDependsPair;
  class ImplicitDependsList: public std::vector<ImplicitDependsPair> {};
  void SetImplicitDepends(ImplicitDependsList const&);
  void AppendImplicitDepends(ImplicitDependsList const&);
  ImplicitDependsList const& GetImplicitDepends() const;

private:
  std::vector<std::string> Outputs;
  std::vector<std::string> Depends;
  cmCustomCommandLines CommandLines;
  bool HaveComment;
  std::string Comment;
  std::string WorkingDirectory;
  bool EscapeAllowMakeVars;
  bool EscapeOldStyle;
  ImplicitDependsList ImplicitDepends;
};

#endif
