/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmCommand_h
#define cmCommand_h

#include "cmObject.h"
#include "cmListFileCache.h"
#include "cmMakefile.h"
#include "cmCommandArgumentsHelper.h"

/** \class cmCommand
 * \brief Superclass for all commands in CMake.
 *
 * cmCommand is the base class for all commands in CMake. A command
 * manifests as an entry in CMakeLists.txt and produces one or
 * more makefile rules. Commands are associated with a particular
 * makefile. This base class cmCommand defines the API for commands 
 * to support such features as enable/disable, inheritance, 
 * documentation, and construction.
 */
class cmCommand : public cmObject
{
public:
  cmTypeMacro(cmCommand, cmObject);

  /**
   * Construct the command. By default it is enabled with no makefile.
   */
  cmCommand()  
    {this->Makefile = 0; this->Enabled = true;}

  /**
   * Need virtual destructor to destroy real command type.
   */
  virtual ~cmCommand() {}
  
  /**
   * Specify the makefile.
   */
  void SetMakefile(cmMakefile*m) 
    {this->Makefile = m; }
  cmMakefile* GetMakefile() { return this->Makefile; }

  /**
   * This is called by the cmMakefile when the command is first
   * encountered in the CMakeLists.txt file.  It expands the command's
   * arguments and then invokes the InitialPass.
   */
  virtual bool InvokeInitialPass(const std::vector<cmListFileArgument>& args,
                                 cmExecutionStatus &status)
    {
    std::vector<std::string> expandedArguments;
    this->Makefile->ExpandArguments(args, expandedArguments);
    return this->InitialPass(expandedArguments,status);
    }

  /**
   * This is called when the command is first encountered in
   * the CMakeLists.txt file.
   */
  virtual bool InitialPass(std::vector<std::string> const& args,
                           cmExecutionStatus &) = 0;

  /**
   * This is called at the end after all the information
   * specified by the command is accumulated. Most commands do
   * not implement this method.  At this point, reading and
   * writing to the cache can be done.
   */
  virtual void FinalPass() {};
  
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() = 0;
  
  /**
   * This determines if the command is invoked when in script mode.
   */
  virtual bool IsScriptable()
    {
    return false;
    }

  /**
   * This determines if usage of the method is discouraged or not.
   * This is currently only used for generating the documentation.
   */
  virtual bool IsDiscouraged()
    {
    return false;
    }

  /**
   * The name of the command as specified in CMakeList.txt.
   */
  virtual const char* GetName() = 0;

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() = 0;

  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation() = 0;

  /**
   * Enable the command.
   */
  void EnabledOn() 
    {this->Enabled = true;}

  /**
   * Disable the command.
   */
  void EnabledOff() 
    {this->Enabled = false;}

  /**
   * Query whether the command is enabled.
   */
  bool GetEnabled()  
    {return this->Enabled;}

  /**
   * Disable or enable the command.
   */
  void SetEnabled(bool enabled)  
    {this->Enabled = enabled;}

  /**
   * Return the last error string.
   */
  const char* GetError() 
    {
      if(this->Error.length() == 0)
        {
        this->Error = this->GetName();
        this->Error += " unknown error.";
        }
      return this->Error.c_str();
    }

  /**
   * Set the error message
   */
  void SetError(const char* e)
    {
    this->Error = this->GetName();
    this->Error += " ";
    this->Error += e;
    }

protected:
  cmMakefile* Makefile;
  cmCommandArgumentsHelper Helper;

private:
  bool Enabled;
  std::string Error;
};

#endif
