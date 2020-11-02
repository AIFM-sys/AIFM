/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmStringCommand.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmStringCommand_h
#define cmStringCommand_h

#include "cmCommand.h"

class cmMakefile;
namespace cmsys
{
  class RegularExpression;
}

/** \class cmStringCommand
 * \brief Common string operations
 *
 */
class cmStringCommand : public cmCommand
{
public:
  /**
   * This is a virtual constructor for the command.
   */
  virtual cmCommand* Clone() 
    {
    return new cmStringCommand;
    }

  /**
   * This is called when the command is first encountered in
   * the CMakeLists.txt file.
   */
  virtual bool InitialPass(std::vector<std::string> const& args,
                           cmExecutionStatus &status);

  /**
   * This determines if the command is invoked when in script mode.
   */
  virtual bool IsScriptable() { return true; }

  /**
   * The name of the command as specified in CMakeList.txt.
   */
  virtual const char* GetName() { return "string";}

  /**
   * Succinct documentation.
   */
  virtual const char* GetTerseDocumentation() 
    {
    return "String operations.";
    }
  
  /**
   * More documentation.
   */
  virtual const char* GetFullDocumentation()
    {
    return
      "  string(REGEX MATCH <regular_expression>\n"
      "         <output variable> <input> [<input>...])\n"
      "  string(REGEX MATCHALL <regular_expression>\n"
      "         <output variable> <input> [<input>...])\n"
      "  string(REGEX REPLACE <regular_expression>\n"
      "         <replace_expression> <output variable>\n"
      "         <input> [<input>...])\n"
      "  string(REPLACE <match_string>\n"
      "         <replace_string> <output variable>\n"
      "         <input> [<input>...])\n"
      "  string(COMPARE EQUAL <string1> <string2> <output variable>)\n"
      "  string(COMPARE NOTEQUAL <string1> <string2> <output variable>)\n"
      "  string(COMPARE LESS <string1> <string2> <output variable>)\n"
      "  string(COMPARE GREATER <string1> <string2> <output variable>)\n"
      "  string(ASCII <number> [<number> ...] <output variable>)\n"
      "  string(CONFIGURE <string1> <output variable>\n"
      "         [@ONLY] [ESCAPE_QUOTES])\n"
      "  string(TOUPPER <string1> <output variable>)\n"
      "  string(TOLOWER <string1> <output variable>)\n"
      "  string(LENGTH <string> <output variable>)\n"
      "  string(SUBSTRING <string> <begin> <length> <output variable>)\n"
      "  string(STRIP <string> <output variable>)\n"
      "  string(RANDOM [LENGTH <length>] [ALPHABET <alphabet>]\n"
      "         <output variable>)\n"
      "REGEX MATCH will match the regular expression once and store the "
      "match in the output variable.\n"
      "REGEX MATCHALL will match the regular expression as many times as "
      "possible and store the matches in the output variable as a list.\n"
      "REGEX REPLACE will match the regular expression as many times as "
      "possible and substitute the replacement expression for the match "
      "in the output.  The replace expression may refer to paren-delimited "
      "subexpressions of the match using \\1, \\2, ..., \\9.  Note that "
      "two backslashes (\\\\1) are required in CMake code to get a "
      "backslash through argument parsing.\n"
      "REPLACE will replace all occurrences of match_string in the input with "
      "replace_string and store the result in the output.\n"
      "COMPARE EQUAL/NOTEQUAL/LESS/GREATER will compare the strings and "
      "store true or false in the output variable.\n"
      "ASCII will convert all numbers into corresponding ASCII characters.\n"
      "CONFIGURE will transform a string like CONFIGURE_FILE transforms "
      "a file.\n"
      "TOUPPER/TOLOWER will convert string to upper/lower characters.\n"
      "LENGTH will return a given string's length.\n"
      "SUBSTRING will return a substring of a given string.\n"
      "STRIP will return a substring of a given string with leading "
      "and trailing spaces removed.\n"
      "RANDOM will return a random string of given length consisting of "
      "characters from the given alphabet. Default length is 5 "
      "characters and default alphabet is all numbers and upper and "
      "lower case letters.\n"
      "The following characters have special meaning in regular expressions:\n"
      "   ^         Matches at beginning of a line\n"
      "   $         Matches at end of a line\n"
      "   .         Matches any single character\n"
      "   [ ]       Matches any character(s) inside the brackets\n"
      "   [^ ]      Matches any character(s) not inside the brackets\n"
      "    -        Matches any character in range on either side of a dash\n"
      "   *         Matches preceding pattern zero or more times\n"
      "   +         Matches preceding pattern one or more times\n"
      "   ?         Matches preceding pattern zero or once only\n"
      "   |         Matches a pattern on either side of the |\n"
      "   ()        Saves a matched subexpression, which can be referenced in "
      "the REGEX REPLACE operation. Additionally it is saved in the special "
      "CMake variables CMAKE_MATCH_(0..9).";
    }
  
  cmTypeMacro(cmStringCommand, cmCommand);
  static void ClearMatches(cmMakefile* mf);
  static void StoreMatches(cmMakefile* mf, cmsys::RegularExpression& re);
protected:
  bool HandleConfigureCommand(std::vector<std::string> const& args);
  bool HandleAsciiCommand(std::vector<std::string> const& args);
  bool HandleRegexCommand(std::vector<std::string> const& args);
  bool RegexMatch(std::vector<std::string> const& args);
  bool RegexMatchAll(std::vector<std::string> const& args);
  bool RegexReplace(std::vector<std::string> const& args);
  bool HandleToUpperLowerCommand(std::vector<std::string> const& args,
                                 bool toUpper);
  bool HandleCompareCommand(std::vector<std::string> const& args);
  bool HandleReplaceCommand(std::vector<std::string> const& args);
  bool HandleLengthCommand(std::vector<std::string> const& args);
  bool HandleSubstringCommand(std::vector<std::string> const& args);
  bool HandleStripCommand(std::vector<std::string> const& args);
  bool HandleRandomCommand(std::vector<std::string> const& args);

  class RegexReplacement
  {
  public:
    RegexReplacement(const char* s): number(-1), value(s) {}
    RegexReplacement(const std::string& s): number(-1), value(s) {}
    RegexReplacement(int n): number(n), value() {}
    RegexReplacement() {};
    int number;
    std::string value;
  };
};


#endif
