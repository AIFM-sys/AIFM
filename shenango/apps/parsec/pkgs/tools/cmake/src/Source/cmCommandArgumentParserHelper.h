/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCommandArgumentParserHelper.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmCommandArgumentParserHelper_h
#define cmCommandArgumentParserHelper_h

#include "cmStandardIncludes.h"

#define YYSTYPE cmCommandArgumentParserHelper::ParserType
#define YYSTYPE_IS_DECLARED
#define YY_EXTRA_TYPE cmCommandArgumentParserHelper*
#define YY_DECL int cmCommandArgument_yylex(YYSTYPE* yylvalp,\
  yyscan_t yyscanner)

/** \class cmCommandArgumentParserHelper
 * \brief Helper class for parsing java source files
 *
 * Finds dependencies for java file and list of outputs
 */

class cmMakefile;

class cmCommandArgumentParserHelper
{
public:
  typedef struct {
    char* str;
  } ParserType;

  cmCommandArgumentParserHelper();
  ~cmCommandArgumentParserHelper();

  int ParseString(const char* str, int verb);

  // For the lexer:
  void AllocateParserType(cmCommandArgumentParserHelper::ParserType* pt,
    const char* str, int len = 0);
  bool HandleEscapeSymbol(cmCommandArgumentParserHelper::ParserType* pt,
    char symbol);

  int LexInput(char* buf, int maxlen);
  void Error(const char* str);

  // For yacc
  char* CombineUnions(char* in1, char* in2);

  char* ExpandSpecialVariable(const char* key, const char* var);
  char* ExpandVariable(const char* var);
  char* ExpandVariableForAt(const char* var);
  void SetResult(const char* value);

  void SetMakefile(const cmMakefile* mf);

  std::string& GetResult() { return this->Result; }

  void SetLineFile(long line, const char* file);
  void SetEscapeQuotes(bool b) { this->EscapeQuotes = b; }
  void SetNoEscapeMode(bool b) { this->NoEscapeMode = b; }
  void SetReplaceAtSyntax(bool b) { this->ReplaceAtSyntax = b; }
  void SetRemoveEmpty(bool b) { this->RemoveEmpty = b; }
  
  const char* GetError() { return this->ErrorString.c_str(); } 
  char EmptyVariable[1];
  char DCURLYVariable[3];
  char RCURLYVariable[3];
  char ATVariable[3];
  char DOLLARVariable[3];
  char LCURLYVariable[3];
  char BSLASHVariable[3];

private:
  cmStdString::size_type InputBufferPos;
  cmStdString InputBuffer;
  std::vector<char> OutputBuffer;
  int CurrentLine;
  int UnionsAvailable;
  int Verbose;

  void Print(const char* place, const char* str);
  void SafePrintMissing(const char* str, int line, int cnt);

  char* AddString(const char* str);

  void CleanupParser();

  std::vector<char*> Variables;
  const cmMakefile* Makefile;
  std::string Result;
  const char* FileName;
  long FileLine;
  bool EscapeQuotes;
  std::string ErrorString;
  bool NoEscapeMode;
  bool ReplaceAtSyntax;
  bool RemoveEmpty; 
};

#endif


