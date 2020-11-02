/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmExprParserHelper.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmExprParserHelper_h 
#define cmExprParserHelper_h

#include "cmStandardIncludes.h"

#define YYSTYPE cmExprParserHelper::ParserType
#define YYSTYPE_IS_DECLARED
#define YY_EXTRA_TYPE cmExprParserHelper*
#define YY_DECL int cmExpr_yylex(YYSTYPE* yylvalp, yyscan_t yyscanner)

/** \class cmExprParserHelper
 * \brief Helper class for parsing java source files
 *
 * Finds dependencies for java file and list of outputs
 */

class cmMakefile;

class cmExprParserHelper
{
public:
  typedef struct {
    int Number;
  } ParserType;

  cmExprParserHelper();
  ~cmExprParserHelper();

  int ParseString(const char* str, int verb);

  int LexInput(char* buf, int maxlen);
  void Error(const char* str);

  void SetResult(int value);

  int GetResult() { return this->Result; }

  void SetLineFile(long line, const char* file);

  const char* GetError() { return this->ErrorString.c_str(); }

private:
  cmStdString::size_type InputBufferPos;
  cmStdString InputBuffer;
  std::vector<char> OutputBuffer;
  int CurrentLine;
  int UnionsAvailable;
  int Verbose;

  void Print(const char* place, const char* str);

  void CleanupParser();

  int Result;
  const char* FileName;
  long FileLine;
  std::string ErrorString;
};

#endif



