%{
/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCommandArgumentParser.y,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
/*

This file must be translated to C and modified to build everywhere.

Run bison like this:

  bison --yacc --name-prefix=cmCommandArgument_yy --defines=cmCommandArgumentParserTokens.h -ocmCommandArgumentParser.cxx cmCommandArgumentParser.y

Modify cmCommandArgumentParser.cxx:
  - remove TABs

*/

#include "cmStandardIncludes.h"

/* Configure the parser to use a lexer object.  */
#define YYPARSE_PARAM yyscanner
#define YYLEX_PARAM yyscanner
#define YYERROR_VERBOSE 1
#define cmCommandArgument_yyerror(x) \
        cmCommandArgumentError(yyscanner, x)
#define yyGetParser (cmCommandArgument_yyget_extra(yyscanner))

/* Make sure malloc and free are available on QNX.  */
#ifdef __QNX__
# include <malloc.h>
#endif

/* Make sure the parser uses standard memory allocation.  The default
   generated parser malloc/free declarations do not work on all
   platforms.  */
#include <stdlib.h>
#define YYMALLOC malloc
#define YYFREE free

/*-------------------------------------------------------------------------*/
#include "cmCommandArgumentParserHelper.h" /* Interface to parser object.  */
#include "cmCommandArgumentLexer.h"  /* Interface to lexer object.  */
#include "cmCommandArgumentParserTokens.h" /* Need YYSTYPE for YY_DECL.  */

/* Forward declare the lexer entry point.  */
YY_DECL;

/* Internal utility functions.  */
static void cmCommandArgumentError(yyscan_t yyscanner, const char* message);

#define YYDEBUG 1
//#define YYMAXDEPTH 100000
//#define YYINITDEPTH 10000


/* Disable some warnings in the generated code.  */
#ifdef __BORLANDC__
# pragma warn -8004 /* Variable assigned a value that is not used.  */
# pragma warn -8008 /* condition always returns true */
# pragma warn -8060 /* possibly incorrect assignment */
# pragma warn -8066 /* unreachable code */
#endif
#ifdef _MSC_VER
# pragma warning (disable: 4102) /* Unused goto label.  */
# pragma warning (disable: 4065) /* Switch statement contains default but no
                                    case. */
#endif
%}

/* Generate a reentrant parser object.  */
%pure_parser

/*
%union {
  char* string;
}
*/

/*-------------------------------------------------------------------------*/
/* Tokens */
%token cal_NCURLY
%token cal_DCURLY
%token cal_DOLLAR "$"
%token cal_LCURLY "{"
%token cal_RCURLY "}"
%token cal_NAME
%token cal_BSLASH "\\"
%token cal_SYMBOL
%token cal_AT     "@"
%token cal_ERROR
%token cal_ATNAME

/*-------------------------------------------------------------------------*/
/* grammar */
%%


Start:
GoalWithOptionalBackSlash
{
  $<str>$ = 0;
  yyGetParser->SetResult($<str>1);
}

GoalWithOptionalBackSlash:
Goal
{
  $<str>$ = $<str>1;
}
|
Goal cal_BSLASH
{
  $<str>$ = yyGetParser->CombineUnions($<str>1, $<str>2);
}

Goal:
{
  $<str>$ = 0;
}
|
String Goal
{
  $<str>$ = yyGetParser->CombineUnions($<str>1, $<str>2);
}

String:
OuterText
{
  $<str>$ = $<str>1;
}
|
Variable
{
  $<str>$ = $<str>1;
}

OuterText:
cal_NAME
{
  $<str>$ = $<str>1;
}
|
cal_AT
{
  $<str>$ = $<str>1;
}
|
cal_DOLLAR
{
  $<str>$ = $<str>1;
}
|
cal_LCURLY
{
  $<str>$ = $<str>1;
}
|
cal_RCURLY
{
  $<str>$ = $<str>1;
}
|
cal_SYMBOL
{
  $<str>$ = $<str>1;
}

Variable:
cal_NCURLY MultipleIds cal_RCURLY
{
  $<str>$ = yyGetParser->ExpandSpecialVariable($<str>1,$<str>2);
  //std::cerr << __LINE__ << " here: [" << $<str>1 << "] [" << $<str>2 << "] [" << $<str>3 << "]" << std::endl;
}
|
cal_DCURLY MultipleIds cal_RCURLY
{
  $<str>$ = yyGetParser->ExpandVariable($<str>2);
  //std::cerr << __LINE__ << " here: [" << $<str>1 << "] [" << $<str>2 << "] [" << $<str>3 << "]" << std::endl;
}
|
cal_ATNAME
{
  $<str>$ = yyGetParser->ExpandVariableForAt($<str>1);
}

MultipleIds:
{
  $<str>$ = 0;
}
|
ID MultipleIds
{
  $<str>$ = yyGetParser->CombineUnions($<str>1, $<str>2);
}

ID:
cal_NAME
{
  $<str>$ = $<str>1;
}
|
Variable
{
  $<str>$ = $<str>1;
}


%%
/* End of grammar */

/*--------------------------------------------------------------------------*/
void cmCommandArgumentError(yyscan_t yyscanner, const char* message)
{
  yyGetParser->Error(message);
}

