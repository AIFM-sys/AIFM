/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCommandArgumentParserHelper.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmCommandArgumentParserHelper.h"

#include "cmSystemTools.h"
#include "cmCommandArgumentLexer.h"

#include "cmMakefile.h"

int cmCommandArgument_yyparse( yyscan_t yyscanner );
//
cmCommandArgumentParserHelper::cmCommandArgumentParserHelper()
{
  this->FileLine = -1;
  this->FileName = 0;
  this->RemoveEmpty = true;
  this->EmptyVariable[0] = 0;
  strcpy(this->DCURLYVariable, "${");
  strcpy(this->RCURLYVariable, "}");
  strcpy(this->ATVariable,     "@");
  strcpy(this->DOLLARVariable, "$");
  strcpy(this->LCURLYVariable, "{");
  strcpy(this->BSLASHVariable, "\\");

  this->NoEscapeMode = false;
  this->ReplaceAtSyntax = false;
}


cmCommandArgumentParserHelper::~cmCommandArgumentParserHelper()
{
  this->CleanupParser();
}

void cmCommandArgumentParserHelper::SetLineFile(long line, const char* file)
{
  this->FileLine = line;
  this->FileName = file;
}

char* cmCommandArgumentParserHelper::AddString(const char* str)
{
  if ( !str || !*str )
    {
    return this->EmptyVariable;
    }
  char* stVal = new char[strlen(str)+1];
  strcpy(stVal, str);
  this->Variables.push_back(stVal);
  return stVal;
}

char* cmCommandArgumentParserHelper::ExpandSpecialVariable(const char* key, 
                                                           const char* var)
{
  if ( !key )
    {
    return this->ExpandVariable(var);
    } 
  if ( strcmp(key, "ENV") == 0 )
    {
    char *ptr = getenv(var);
    if (ptr)
      {
      if (this->EscapeQuotes)
        {
        return this->AddString(cmSystemTools::EscapeQuotes(ptr).c_str());
        }
      else
        {
        return ptr;
        }
      }
    return this->EmptyVariable;
    }
  cmSystemTools::Error("Key ", key, 
                       " is not used yet. For now only $ENV{..} is allowed");
  return 0;
}

char* cmCommandArgumentParserHelper::ExpandVariable(const char* var)
{
  if(!var)
    {
    return 0;
    }
  if(this->FileName && strcmp(var, "CMAKE_CURRENT_LIST_FILE") == 0)
    {
    return this->AddString(this->FileName);
    }
  else if(this->FileLine >= 0 && strcmp(var, "CMAKE_CURRENT_LIST_LINE") == 0)
    {
    cmOStringStream ostr;
    ostr << this->FileLine;
    return this->AddString(ostr.str().c_str());
    } 
  const char* value = this->Makefile->GetDefinition(var);
  if(!value && !this->RemoveEmpty)
    {
    return 0;
    }
  if (this->EscapeQuotes && value)
    {
    return this->AddString(cmSystemTools::EscapeQuotes(value).c_str());
    }
  return this->AddString(value);
}

char* cmCommandArgumentParserHelper::ExpandVariableForAt(const char* var)
{
  if(this->ReplaceAtSyntax)
    {
    // try to expand the variable
    char* ret = this->ExpandVariable(var);
    // if the return was 0 and we want to replace empty strings
    // then return an empty string 
    if(!ret && this->RemoveEmpty)
      {
      return this->AddString(ret);
      }
    // if the ret was not 0, then return it
    if(ret)
      {
      return ret;
      }
    }
  // at this point we want to put it back because of one of these cases:
  // - this->ReplaceAtSyntax is false  
  // - this->ReplaceAtSyntax is true, but this->RemoveEmpty is false,
  //   and the variable was not defined
  std::string ref = "@";
  ref += var;
  ref += "@";
  return this->AddString(ref.c_str());
}

char* cmCommandArgumentParserHelper::CombineUnions(char* in1, char* in2)
{
  if ( !in1 )
    {
    return in2;
    }
  else if ( !in2 )
    {
    return in1;
    }
  size_t len = strlen(in1) + strlen(in2) + 1;
  char* out = new char [ len ];
  strcpy(out, in1);
  strcat(out, in2);
  this->Variables.push_back(out);
  return out;
}

void cmCommandArgumentParserHelper::AllocateParserType
(cmCommandArgumentParserHelper::ParserType* pt,const char* str, int len)
{
  pt->str = 0;
  if ( len == 0 )
    {
    len = static_cast<int>(strlen(str));
    }
  if ( len == 0 )
    {
    return;
    }
  pt->str = new char[ len + 1 ];
  strncpy(pt->str, str, len);
  pt->str[len] = 0;
  this->Variables.push_back(pt->str);
}

bool cmCommandArgumentParserHelper::HandleEscapeSymbol
(cmCommandArgumentParserHelper::ParserType* pt, char symbol)
{
  switch ( symbol )
    {
  case '\\':
  case '"':
  case ' ':
  case '#':
  case '(':
  case ')':
  case '$':
  case '@':
  case '^':
    this->AllocateParserType(pt, &symbol, 1);
    break;
  case ';':
    this->AllocateParserType(pt, "\\;", 2);
    break;
  case 't':
    this->AllocateParserType(pt, "\t", 1);
    break;
  case 'n':
    this->AllocateParserType(pt, "\n", 1);
    break;
  case 'r':
    this->AllocateParserType(pt, "\r", 1);
    break;
  case '0':
    this->AllocateParserType(pt, "\0", 1);
    break;
  default:
    char buffer[2];
    buffer[0] = symbol;
    buffer[1] = 0;
    cmSystemTools::Error("Invalid escape sequence \\", buffer);
    return false;
    }
  return true;
}

void cmCommandArgument_SetupEscapes(yyscan_t yyscanner, bool noEscapes);

int cmCommandArgumentParserHelper::ParseString(const char* str, int verb)
{
  if ( !str)
    {
    return 0;
    }
  this->Verbose = verb;
  this->InputBuffer = str;
  this->InputBufferPos = 0;
  this->CurrentLine = 0;
  
  this->Result = "";

  yyscan_t yyscanner;
  cmCommandArgument_yylex_init(&yyscanner);
  cmCommandArgument_yyset_extra(this, yyscanner);
  cmCommandArgument_SetupEscapes(yyscanner, this->NoEscapeMode);
  int res = cmCommandArgument_yyparse(yyscanner);
  cmCommandArgument_yylex_destroy(yyscanner);
  if ( res != 0 )
    {
    return 0;
    }

  this->CleanupParser();

  if ( Verbose )
    {
    std::cerr << "Expanding [" << str << "] produced: [" 
              << this->Result.c_str() << "]" << std::endl;
    }
  return 1;
}

void cmCommandArgumentParserHelper::CleanupParser()
{
  std::vector<char*>::iterator sit;
  for ( sit = this->Variables.begin();
    sit != this->Variables.end();
    ++ sit )
    {
    delete [] *sit;
    }
  this->Variables.erase(this->Variables.begin(), this->Variables.end());
}

int cmCommandArgumentParserHelper::LexInput(char* buf, int maxlen)
{
  if ( maxlen < 1 )
    {
    return 0;
    }
  if ( this->InputBufferPos < this->InputBuffer.size() )
    {
    buf[0] = this->InputBuffer[ this->InputBufferPos++ ];
    if ( buf[0] == '\n' )
      {
      this->CurrentLine ++;
      }
    return(1);
    }
  else
    {
    buf[0] = '\n';
    return( 0 );
    }
}

void cmCommandArgumentParserHelper::Error(const char* str)
{
  unsigned long pos = static_cast<unsigned long>(this->InputBufferPos);
  cmOStringStream ostr;
  ostr << str << " (" << pos << ")";
  this->ErrorString = ostr.str();
}

void cmCommandArgumentParserHelper::SetMakefile(const cmMakefile* mf)
{
  this->Makefile = mf;
}

void cmCommandArgumentParserHelper::SetResult(const char* value)
{
  if ( !value )
    {
    this->Result = "";
    return;
    }
  this->Result = value;
}

