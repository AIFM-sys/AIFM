/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmSystemTools.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:08 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmSystemTools.h"   
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#include <cmsys/RegularExpression.hxx>
#include <cmsys/Directory.hxx>
#include <cmsys/System.h>
#if defined(CMAKE_BUILD_WITH_CMAKE)
# include <cmsys/Terminal.h>
#endif

#if defined(_WIN32)
# include <windows.h>
#else
# include <sys/types.h>
# include <unistd.h>
# include <utime.h>
# include <sys/wait.h>
#endif

#include <sys/stat.h>

#if defined(_WIN32) && \
   (defined(_MSC_VER) || defined(__WATCOMC__) || \
    defined(__BORLANDC__) || defined(__MINGW32__))
# include <io.h>
#endif

#if defined(CMAKE_BUILD_WITH_CMAKE)
#  include <libtar/libtar.h>
#  include <memory> // auto_ptr
#  include <fcntl.h>
#  include <cm_zlib.h>
#  include <cmsys/MD5.h>
#endif

#if defined(CMAKE_USE_ELF_PARSER)
# include "cmELF.h"
#endif

class cmSystemToolsFileTime
{
public:
#if defined(_WIN32) && !defined(__CYGWIN__)
  FILETIME timeCreation;
  FILETIME timeLastAccess;
  FILETIME timeLastWrite;
#else
  struct utimbuf timeBuf;
#endif
};

#if defined(__sgi) && !defined(__GNUC__)
# pragma set woff 1375 /* base class destructor not virtual */
#endif

#if !defined(HAVE_ENVIRON_NOT_REQUIRE_PROTOTYPE)
// For GetEnvironmentVariables
# if defined(_WIN32)
extern __declspec( dllimport ) char** environ;
# else
extern char** environ;
# endif
#endif

#ifdef _WIN32
class cmSystemToolsWindowsHandle
{
public:
  cmSystemToolsWindowsHandle(HANDLE h): handle_(h) {}
  ~cmSystemToolsWindowsHandle()
    {
    if(this->handle_ != INVALID_HANDLE_VALUE)
      {
      CloseHandle(this->handle_);
      }
    }
  operator bool() const { return this->handle_ != INVALID_HANDLE_VALUE; }
  bool operator !() const { return this->handle_ == INVALID_HANDLE_VALUE; }
  operator HANDLE() const { return this->handle_; }
private:
  HANDLE handle_;
};
#endif

bool cmSystemTools::s_RunCommandHideConsole = false;
bool cmSystemTools::s_DisableRunCommandOutput = false;
bool cmSystemTools::s_ErrorOccured = false;
bool cmSystemTools::s_FatalErrorOccured = false;
bool cmSystemTools::s_DisableMessages = false;
bool cmSystemTools::s_ForceUnixPaths = false;

std::string cmSystemTools::s_Windows9xComspecSubstitute = "command.com";
void cmSystemTools::SetWindows9xComspecSubstitute(const char* str)
{
  if ( str )
    {
    cmSystemTools::s_Windows9xComspecSubstitute = str;
    }
}
const char* cmSystemTools::GetWindows9xComspecSubstitute()
{
  return cmSystemTools::s_Windows9xComspecSubstitute.c_str();
}

void (*cmSystemTools::s_ErrorCallback)(const char*, const char*, 
                                       bool&, void*);
void (*cmSystemTools::s_StdoutCallback)(const char*, int len, void*);
void* cmSystemTools::s_ErrorCallbackClientData = 0;
void* cmSystemTools::s_StdoutCallbackClientData = 0;

// replace replace with with as many times as it shows up in source.
// write the result into source.
#if defined(_WIN32) && !defined(__CYGWIN__)
void cmSystemTools::ExpandRegistryValues(std::string& source, KeyWOW64 view)
{
  // Regular expression to match anything inside [...] that begins in HKEY.
  // Note that there is a special rule for regular expressions to match a
  // close square-bracket inside a list delimited by square brackets.
  // The "[^]]" part of this expression will match any character except
  // a close square-bracket.  The ']' character must be the first in the
  // list of characters inside the [^...] block of the expression.
  cmsys::RegularExpression regEntry("\\[(HKEY[^]]*)\\]");
  
  // check for black line or comment
  while (regEntry.find(source))
    {
    // the arguments are the second match
    std::string key = regEntry.match(1);
    std::string val;
    if (ReadRegistryValue(key.c_str(), val, view))
      {
      std::string reg = "[";
      reg += key + "]";
      cmSystemTools::ReplaceString(source, reg.c_str(), val.c_str());
      }
    else
      {
      std::string reg = "[";
      reg += key + "]";
      cmSystemTools::ReplaceString(source, reg.c_str(), "/registry");
      }
    }
}
#else
void cmSystemTools::ExpandRegistryValues(std::string& source, KeyWOW64)
{
  cmsys::RegularExpression regEntry("\\[(HKEY[^]]*)\\]");
  while (regEntry.find(source))
    {
    // the arguments are the second match
    std::string key = regEntry.match(1);
    std::string val;
    std::string reg = "[";
    reg += key + "]";
    cmSystemTools::ReplaceString(source, reg.c_str(), "/registry");
    }

}
#endif

std::string cmSystemTools::EscapeQuotes(const char* str)
{
  std::string result = "";
  for(const char* ch = str; *ch != '\0'; ++ch)
    {
    if(*ch == '"')
      {
      result += '\\';
      }
    result += *ch;
    }
  return result;
}

std::string cmSystemTools::EscapeSpaces(const char* str)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
  bool useDoubleQ = true;
#else
  bool useDoubleQ = false;
#endif
  if(cmSystemTools::s_ForceUnixPaths)
    {
    useDoubleQ = false;
    }
  
  if(useDoubleQ)
    {
    std::string result;
    
    // if there are spaces
    std::string temp = str;
    if (temp.find(" ") != std::string::npos && 
        temp.find("\"")==std::string::npos)
      {
      result = "\"";
      result += str;
      result += "\"";
      return result;
      }
    return str;
    }
  else
    {
    std::string result = "";
    for(const char* ch = str; *ch != '\0'; ++ch)
      {
      if(*ch == ' ')
        {
        result += '\\';
        }
      result += *ch;
      }
    return result;
    }
}


std::string cmSystemTools::RemoveEscapes(const char* s)
{
  std::string result = "";
  for(const char* ch = s; *ch; ++ch)
    {
    if(*ch == '\\' && *(ch+1) != ';')
      {
      ++ch;
      switch (*ch)
        {
        case '\\': result.insert(result.end(), '\\'); break;
        case '"': result.insert(result.end(), '"'); break;
        case ' ': result.insert(result.end(), ' '); break;
        case 't': result.insert(result.end(), '\t'); break;
        case 'n': result.insert(result.end(), '\n'); break;
        case 'r': result.insert(result.end(), '\r'); break;
        case '#': result.insert(result.end(), '#'); break;
        case '(': result.insert(result.end(), '('); break;
        case ')': result.insert(result.end(), ')'); break;
        case '0': result.insert(result.end(), '\0'); break;
        case '\0':
          {
          cmSystemTools::Error("Trailing backslash in argument:\n", s);
          return result;
          }
        default:
          {
          std::string chStr(1, *ch);
          cmSystemTools::Error("Invalid escape sequence \\", chStr.c_str(),
                               "\nin argument ", s);
          }
        }
      }
    else
      {
      result.insert(result.end(), *ch);
      }
    }
  return result;
}

void cmSystemTools::Error(const char* m1, const char* m2,
                          const char* m3, const char* m4)
{
  std::string message = "CMake Error: ";
  if(m1)
    {
    message += m1;
    }
  if(m2)
    {
    message += m2;
    }
  if(m3)
    {
    message += m3;
    }
  if(m4)
    {
    message += m4;
    }
  cmSystemTools::s_ErrorOccured = true;
  cmSystemTools::Message(message.c_str(),"Error");
}


void cmSystemTools::SetErrorCallback(ErrorCallback f, void* clientData)
{
  s_ErrorCallback = f;
  s_ErrorCallbackClientData = clientData;
}

void cmSystemTools::SetStdoutCallback(StdoutCallback f, void* clientData)
{
  s_StdoutCallback = f;
  s_StdoutCallbackClientData = clientData;
}

void cmSystemTools::Stdout(const char* s)
{
  if(s_StdoutCallback)
    {
    (*s_StdoutCallback)(s, static_cast<int>(strlen(s)), 
                        s_StdoutCallbackClientData);
    }
  else
    {
    std::cout << s;
    std::cout.flush();
    }
}

void cmSystemTools::Stdout(const char* s, int length)
{
  if(s_StdoutCallback)
    {
    (*s_StdoutCallback)(s, length, s_StdoutCallbackClientData);
    }
  else
    {
    std::cout.write(s, length);
    std::cout.flush();
    }
}

void cmSystemTools::Message(const char* m1, const char *title)
{
  if(s_DisableMessages)
    {
    return;
    }
  if(s_ErrorCallback)
    {
    (*s_ErrorCallback)(m1, title, s_DisableMessages, 
                       s_ErrorCallbackClientData);
    return;
    }
  else
    {
    std::cerr << m1 << std::endl << std::flush;
    }
  
}


void cmSystemTools::ReportLastSystemError(const char* msg)
{
  std::string m = msg;
  m += ": System Error: ";
  m += Superclass::GetLastSystemError();
  cmSystemTools::Error(m.c_str());
}

 
bool cmSystemTools::IsOn(const char* val)
{
  if (!val)
    {
    return false;
    }
  std::basic_string<char> v = val;
  
  for(std::basic_string<char>::iterator c = v.begin();
      c != v.end(); c++)
    {
    *c = toupper(*c);
    }
  return (v == "ON" || v == "1" || v == "YES" || v == "TRUE" || v == "Y");
}

bool cmSystemTools::IsNOTFOUND(const char* val)
{
  size_t len = strlen(val);
  const char* notfound = "-NOTFOUND";
  const size_t lenNotFound = 9;
  if(len < lenNotFound-1)
    {
    return false;
    }
  if(len == lenNotFound-1)
    {
    return ( strcmp(val, "NOTFOUND") == 0);
    }
  return ((strncmp((val + (len - lenNotFound)), notfound, lenNotFound) == 0));
}


bool cmSystemTools::IsOff(const char* val)
{
  if (!val || strlen(val) == 0)
    {
    return true;
    }
  std::basic_string<char> v = val;
  
  for(std::basic_string<char>::iterator c = v.begin();
      c != v.end(); c++)
    {
    *c = toupper(*c);
    }
  return (v == "OFF" || v == "0" || v == "NO" || v == "FALSE" || 
          v == "N" || cmSystemTools::IsNOTFOUND(v.c_str()) || v == "IGNORE");
}

//----------------------------------------------------------------------------
void cmSystemTools::ParseWindowsCommandLine(const char* command,
                                            std::vector<std::string>& args)
{
  // See the MSDN document "Parsing C Command-Line Arguments" at
  // http://msdn2.microsoft.com/en-us/library/a1y7w461.aspx for rules
  // of parsing the windows command line.

  bool in_argument = false;
  bool in_quotes = false;
  int backslashes = 0;
  std::string arg;
  for(const char* c = command;*c; ++c)
    {
    if(*c == '\\')
      {
      ++backslashes;
      in_argument = true;
      }
    else if(*c == '"')
      {
      int backslash_pairs  = backslashes >> 1;
      int backslash_escaped = backslashes & 1;
      arg.append(backslash_pairs, '\\');
      backslashes = 0;
      if(backslash_escaped)
        {
        /* An odd number of backslashes precede this quote.
           It is escaped.  */
        arg.append(1, '"');
        }
      else
        {
        /* An even number of backslashes precede this quote.
           It is not escaped.  */
        in_quotes = !in_quotes;
        }
      in_argument = true;
      }
    else
      {
      arg.append(backslashes, '\\');
      backslashes = 0;
      if(isspace(*c))
        {
        if(in_quotes)
          {
          arg.append(1, *c);
          }
        else if(in_argument)
          {
          args.push_back(arg);
          arg = "";
          in_argument = false;
          }
        }
      else
        {
        in_argument = true;
        arg.append(1, *c);
        }
      }
    }
  arg.append(backslashes, '\\');
  if(in_argument)
    {
    args.push_back(arg);
    }
}

std::string cmSystemTools::EscapeWindowsShellArgument(const char* arg,
                                                      int shell_flags)
{
  char local_buffer[1024];
  char* buffer = local_buffer;
  int size = cmsysSystem_Shell_GetArgumentSizeForWindows(arg, shell_flags);
  if(size > 1024)
    {
    buffer = new char[size];
    }
  cmsysSystem_Shell_GetArgumentForWindows(arg, buffer, shell_flags);
  std::string result(buffer);
  if(buffer != local_buffer)
    {
    delete [] buffer;
    }
  return result;
}

std::vector<cmStdString> cmSystemTools::ParseArguments(const char* command)
{
  std::vector<cmStdString> args;
  std::string arg;

  bool win_path = false;

  if ( command[0] != '/' && command[1] == ':' && command[2] == '\\' ||
       command[0] == '\"' && command[1] != '/' && command[2] == ':' 
       && command[3] == '\\' || 
       command[0] == '\'' && command[1] != '/' && command[2] == ':' 
       && command[3] == '\\' || 
       command[0] == '\\' && command[1] == '\\')
    {
    win_path = true;
    }
  // Split the command into an argv array.
  for(const char* c = command; *c;)
    {
    // Skip over whitespace.
    while(*c == ' ' || *c == '\t')
      {
      ++c;
      }
    arg = "";
    if(*c == '"')
      {
      // Parse a quoted argument.
      ++c;
      while(*c && *c != '"')
        {
        arg.append(1, *c);
        ++c;
        }
      if(*c)
        {
        ++c;
        }
      args.push_back(arg);
      }
    else if(*c == '\'')
      {
      // Parse a quoted argument.
      ++c;
      while(*c && *c != '\'')
        {
        arg.append(1, *c);
        ++c;
        }
      if(*c)
        {
        ++c;
        }
      args.push_back(arg);
      }
    else if(*c)
      {
      // Parse an unquoted argument.
      while(*c && *c != ' ' && *c != '\t')
        {
        if(*c == '\\' && !win_path)
          {
          ++c;
          if(*c)
            {
            arg.append(1, *c);
            ++c;
            }
          }
        else
          {
          arg.append(1, *c);
          ++c;
          }
        }
      args.push_back(arg);
      }
    }
  
  return args;
}


bool cmSystemTools::RunSingleCommand(std::vector<cmStdString>const& command,
                                     std::string* output ,
                                     int* retVal , const char* dir , 
                                     bool verbose ,
                                     double timeout )
{
  std::vector<const char*> argv;
  for(std::vector<cmStdString>::const_iterator a = command.begin();
      a != command.end(); ++a)
    {
    argv.push_back(a->c_str());
    }
  argv.push_back(0);
  if ( output )
    {
    *output = "";
    }

  cmsysProcess* cp = cmsysProcess_New();
  cmsysProcess_SetCommand(cp, &*argv.begin());
  cmsysProcess_SetWorkingDirectory(cp, dir);
  if(cmSystemTools::GetRunCommandHideConsole())
    {
    cmsysProcess_SetOption(cp, cmsysProcess_Option_HideWindow, 1);
    }
  cmsysProcess_SetTimeout(cp, timeout);
  cmsysProcess_Execute(cp);
  
  std::vector<char> tempOutput;
  char* data;
  int length;
  if ( output || verbose )
    {
  while(cmsysProcess_WaitForData(cp, &data, &length, 0))
    {
    if(output || verbose)
      {
      // Translate NULL characters in the output into valid text.
      // Visual Studio 7 puts these characters in the output of its
      // build process.
      for(int i=0; i < length; ++i)
        {
        if(data[i] == '\0')
          {
          data[i] = ' ';
          }
        }
      }
    if ( output )
      {
      tempOutput.insert(tempOutput.end(), data, data+length);
      }
    if(verbose)
      {
      cmSystemTools::Stdout(data, length);
      }
    }
    }
  
  cmsysProcess_WaitForExit(cp, 0);
  if ( output && tempOutput.begin() != tempOutput.end())
    {
    output->append(&*tempOutput.begin(), tempOutput.size());
    }
  
  bool result = true;
  if(cmsysProcess_GetState(cp) == cmsysProcess_State_Exited)
    {
    if ( retVal )
      {
      *retVal = cmsysProcess_GetExitValue(cp);
      }
    else
      {
      if ( cmsysProcess_GetExitValue(cp) !=  0 )
        {
        result = false;
        }
      }
    }
  else if(cmsysProcess_GetState(cp) == cmsysProcess_State_Exception)
    {
    const char* exception_str = cmsysProcess_GetExceptionString(cp);
    if ( verbose )
      {
      std::cerr << exception_str << std::endl;
      }
    if ( output )
      {
      output->append(exception_str, strlen(exception_str));
      }
    result = false;
    }
  else if(cmsysProcess_GetState(cp) == cmsysProcess_State_Error)
    {
    const char* error_str = cmsysProcess_GetErrorString(cp);
    if ( verbose )
      {
      std::cerr << error_str << std::endl;
      }
    if ( output )
      {
      output->append(error_str, strlen(error_str));
      }
    result = false;
    }
  else if(cmsysProcess_GetState(cp) == cmsysProcess_State_Expired)
    {
    const char* error_str = "Process terminated due to timeout\n";
    if ( verbose )
      {
      std::cerr << error_str << std::endl;
      }
    if ( output )
      {
      output->append(error_str, strlen(error_str));
      }
    result = false;
    }
  
  cmsysProcess_Delete(cp);
  return result;
}

bool cmSystemTools::RunSingleCommand(
  const char* command, 
  std::string* output,
  int *retVal, 
  const char* dir,
  bool verbose,
  double timeout)
{
  if(s_DisableRunCommandOutput)
    {
    verbose = false;
    }

  std::vector<cmStdString> args = cmSystemTools::ParseArguments(command);

  if(args.size() < 1)
    {
    return false;
    }
  return cmSystemTools::RunSingleCommand(args, output,retVal, 
                                         dir, verbose, timeout);
}
bool cmSystemTools::RunCommand(const char* command, 
                               std::string& output,
                               const char* dir,
                               bool verbose,
                               int timeout)
{
  int dummy;
  return cmSystemTools::RunCommand(command, output, dummy, 
                                   dir, verbose, timeout);
}

#if defined(WIN32) && !defined(__CYGWIN__)
#include "cmWin32ProcessExecution.h"
// use this for shell commands like echo and dir
bool RunCommandViaWin32(const char* command,
                        const char* dir,
                        std::string& output,
                        int& retVal,
                        bool verbose,
                        int timeout)
{
#if defined(__BORLANDC__)
  return 
    cmWin32ProcessExecution::
    BorlandRunCommand(command, dir, output, 
                      retVal, 
                      verbose, timeout, 
                      cmSystemTools::GetRunCommandHideConsole());
#else // Visual studio
  ::SetLastError(ERROR_SUCCESS);
  if ( ! command )
    {
    cmSystemTools::Error("No command specified");
    return false;
    }
  cmWin32ProcessExecution resProc;
  if(cmSystemTools::GetRunCommandHideConsole())
    {
    resProc.SetHideWindows(true);
    }
  
  if ( cmSystemTools::GetWindows9xComspecSubstitute() )
    {
    resProc.SetConsoleSpawn(cmSystemTools::GetWindows9xComspecSubstitute() );
    }
  if ( !resProc.StartProcess(command, dir, verbose) )
    {
    output = resProc.GetOutput();
    if(verbose)
      {
      cmSystemTools::Stdout(output.c_str());
      }
    return false;
    }
  resProc.Wait(timeout);
  output = resProc.GetOutput();
  retVal = resProc.GetExitValue();
  return true;
#endif
}

// use this for shell commands like echo and dir
bool RunCommandViaSystem(const char* command,
                         const char* dir,
                         std::string& output,
                         int& retVal,
                         bool verbose)
{  
  std::cout << "@@ " << command << std::endl;

  std::string commandInDir;
  if(dir)
    {
    commandInDir = "cd ";
    commandInDir += cmSystemTools::ConvertToOutputPath(dir);
    commandInDir += " && ";
    commandInDir += command;
    }
  else
    {
    commandInDir = command;
    }
  command = commandInDir.c_str();
  std::string commandToFile = command;
  commandToFile += " > ";
  std::string tempFile;
  tempFile += _tempnam(0, "cmake");

  commandToFile += tempFile;
  retVal = system(commandToFile.c_str());
  std::ifstream fin(tempFile.c_str());
  if(!fin)
    {
    if(verbose)
      {
      std::string errormsg = "RunCommand produced no output: command: \"";
      errormsg += command;
      errormsg += "\"";
      errormsg += "\nOutput file: ";
      errormsg += tempFile;
      cmSystemTools::Error(errormsg.c_str());
      }
    fin.close();
    cmSystemTools::RemoveFile(tempFile.c_str());
    return false;
    }
  bool multiLine = false;
  std::string line;
  while(cmSystemTools::GetLineFromStream(fin, line))
    {
    output += line;
    if(multiLine)
      {
      output += "\n";
      }
    multiLine = true;
    }
  fin.close();
  cmSystemTools::RemoveFile(tempFile.c_str());
  return true;
}

#else // We have popen

// BeOS seems to return from a successful pclose() before the process has
//  legitimately exited, or at least before SIGCHLD is thrown...the signal may
//  come quite some time after pclose returns! This causes havoc with later
//  parts of CMake that expect to catch the signal from other child processes,
//  so we explicitly wait to catch it here. This should be safe to do with
//  popen() so long as we don't actually collect the zombie process ourselves.
#ifdef __BEOS__
#include <signal.h>
#undef SIGBUS  // this is the same as SIGSEGV on BeOS and causes issues below.
static volatile bool beos_seen_signal = false;
static void beos_popen_workaround(int sig)
{
  beos_seen_signal = true;
}
#endif

bool RunCommandViaPopen(const char* command,
                        const char* dir,
                        std::string& output,
                        int& retVal,
                        bool verbose,
                        int /*timeout*/)
{
  // if only popen worked on windows.....
  std::string commandInDir;
  if(dir)
    {
    commandInDir = "cd \"";
    commandInDir += dir;
    commandInDir += "\" && ";
    commandInDir += command;
    }
  else
    {
    commandInDir = command;
    }
  commandInDir += " 2>&1";
  command = commandInDir.c_str();
  const int BUFFER_SIZE = 4096;
  char buffer[BUFFER_SIZE];
  if(verbose)
    {
    cmSystemTools::Stdout("running ");
    cmSystemTools::Stdout(command);
    cmSystemTools::Stdout("\n");
    }
  fflush(stdout);
  fflush(stderr);

#ifdef __BEOS__
  beos_seen_signal = false;
  signal(SIGCHLD, beos_popen_workaround);
#endif

  FILE* cpipe = popen(command, "r");
  if(!cpipe)
    {
#ifdef __BEOS__
    signal(SIGCHLD, SIG_DFL);
#endif
    return false;
    }
  fgets(buffer, BUFFER_SIZE, cpipe);
  while(!feof(cpipe))
    {
    if(verbose)
      {
      cmSystemTools::Stdout(buffer);
      }
    output += buffer;
    fgets(buffer, BUFFER_SIZE, cpipe);
    }

  retVal = pclose(cpipe);

#ifdef __BEOS__
  for (int i = 0; (!beos_seen_signal) && (i < 3); i++)
    {
    ::sleep(1);   // signals should interrupt this...
    }

  if (!beos_seen_signal)
    {
    signal(SIGCHLD, SIG_DFL);  // oh well, didn't happen. Go on anyhow.
    }
#endif

  if (WIFEXITED(retVal))
    {
    retVal = WEXITSTATUS(retVal);
    return true;
    }
  if (WIFSIGNALED(retVal))
    {
    retVal = WTERMSIG(retVal);
    cmOStringStream error;
    error << "\nProcess terminated due to ";
    switch (retVal)
      {
#ifdef SIGKILL
      case SIGKILL:
        error << "SIGKILL";
        break;
#endif
#ifdef SIGFPE
      case SIGFPE:
        error << "SIGFPE";
        break;
#endif
#ifdef SIGBUS
      case SIGBUS:
        error << "SIGBUS";
        break;
#endif
#ifdef SIGSEGV
      case SIGSEGV:
        error << "SIGSEGV";
        break;
#endif
      default:
        error << "signal " << retVal;
        break;
      }
    output += error.str();
    }
  return false;
}

#endif  // endif WIN32 not CYGWIN


// run a command unix uses popen (easy)
// windows uses system and ShortPath
bool cmSystemTools::RunCommand(const char* command, 
                               std::string& output,
                               int &retVal, 
                               const char* dir,
                               bool verbose,
                               int timeout)
{
  if(s_DisableRunCommandOutput)
    {
    verbose = false;
    }
  
#if defined(WIN32) && !defined(__CYGWIN__)
  // if the command does not start with a quote, then
  // try to find the program, and if the program can not be
  // found use system to run the command as it must be a built in
  // shell command like echo or dir
  int count = 0;
  if(command[0] == '\"')
    {
    // count the number of quotes
    for(const char* s = command; *s != 0; ++s)
      {
      if(*s == '\"')
        {
        count++;
        if(count > 2)
          {
          break;
          }
        }      
      }
    // if there are more than two double quotes use 
    // GetShortPathName, the cmd.exe program in windows which
    // is used by system fails to execute if there are more than
    // one set of quotes in the arguments
    if(count > 2)
      {
      cmsys::RegularExpression quoted("^\"([^\"]*)\"[ \t](.*)");
      if(quoted.find(command))
        {
        std::string shortCmd;
        std::string cmd = quoted.match(1);
        std::string args = quoted.match(2);
        if(! cmSystemTools::FileExists(cmd.c_str()) )
          {
          shortCmd = cmd;
          }
        else if(!cmSystemTools::GetShortPath(cmd.c_str(), shortCmd))
          {
         cmSystemTools::Error("GetShortPath failed for " , cmd.c_str());
          return false;
          }
        shortCmd += " ";
        shortCmd += args;

        //return RunCommandViaSystem(shortCmd.c_str(), dir, 
        //                           output, retVal, verbose);
        //return WindowsRunCommand(shortCmd.c_str(), dir, 
        //output, retVal, verbose);
        return RunCommandViaWin32(shortCmd.c_str(), dir, 
                                  output, retVal, verbose, timeout);
        }
      else
        {
        cmSystemTools::Error("Could not parse command line with quotes ", 
                             command);
        }
      }
    }
  // if there is only one set of quotes or no quotes then just run the command
  //return RunCommandViaSystem(command, dir, output, retVal, verbose);
  //return WindowsRunCommand(command, dir, output, retVal, verbose);
  return ::RunCommandViaWin32(command, dir, output, retVal, verbose, timeout);
#else
  return ::RunCommandViaPopen(command, dir, output, retVal, verbose, timeout);
#endif
}

bool cmSystemTools::DoesFileExistWithExtensions(
  const char* name,
  const std::vector<std::string>& headerExts)
{
  std::string hname;

  for( std::vector<std::string>::const_iterator ext = headerExts.begin();
       ext != headerExts.end(); ++ext )
    {
    hname = name;
    hname += ".";
    hname += *ext;
    if(cmSystemTools::FileExists(hname.c_str()))
      {
      return true;
      }
    }
  return false;
}

bool cmSystemTools::cmCopyFile(const char* source, const char* destination)
{
  return Superclass::CopyFileAlways(source, destination);
}

bool cmSystemTools::CopyFileIfDifferent(const char* source, 
  const char* destination)
{
  return Superclass::CopyFileIfDifferent(source, destination);
}

bool cmSystemTools::ComputeFileMD5(const char* source, char* md5out)
{
#if defined(CMAKE_BUILD_WITH_CMAKE)
  if(!cmSystemTools::FileExists(source))
    {
    return false;
    }

  // Open files
#if defined(_WIN32) || defined(__CYGWIN__)
  cmsys_ios::ifstream fin(source, cmsys_ios::ios::binary | cmsys_ios::ios::in);
#else
  cmsys_ios::ifstream fin(source);
#endif
  if(!fin)
    {
    return false;
    }

  cmsysMD5* md5 = cmsysMD5_New();
  cmsysMD5_Initialize(md5);

  // Should be efficient enough on most system:
  const int bufferSize = 4096;
  char buffer[bufferSize];
  // This copy loop is very sensitive on certain platforms with
  // slightly broken stream libraries (like HPUX).  Normally, it is
  // incorrect to not check the error condition on the fin.read()
  // before using the data, but the fin.gcount() will be zero if an
  // error occurred.  Therefore, the loop should be safe everywhere.
  while(fin)
    {
    fin.read(buffer, bufferSize);
    if(fin.gcount())
      {
      cmsysMD5_Append(md5, reinterpret_cast<unsigned char const*>(buffer), 
                      fin.gcount());
      }
    }
  cmsysMD5_FinalizeHex(md5, md5out);
  cmsysMD5_Delete(md5);

  fin.close();
  return true;
#else
  (void)source;
  (void)md5out;
  cmSystemTools::Message("md5sum not supported in bootstrapping mode","Error");
  return false;
#endif
}

std::string cmSystemTools::ComputeStringMD5(const char* input)
{
#if defined(CMAKE_BUILD_WITH_CMAKE)
  char md5out[32];
  cmsysMD5* md5 = cmsysMD5_New();
  cmsysMD5_Initialize(md5);
  cmsysMD5_Append(md5, reinterpret_cast<unsigned char const*>(input), -1);
  cmsysMD5_FinalizeHex(md5, md5out);
  cmsysMD5_Delete(md5);
  return std::string(md5out, 32);
#else
  (void)input;
  cmSystemTools::Message("md5sum not supported in bootstrapping mode","Error");
  return "";
#endif
}

void cmSystemTools::Glob(const char *directory, const char *regexp,
                         std::vector<std::string>& files)
{
  cmsys::Directory d;
  cmsys::RegularExpression reg(regexp);
  
  if (d.Load(directory))
    {
    size_t numf;
        unsigned int i;
    numf = d.GetNumberOfFiles();
    for (i = 0; i < numf; i++)
      {
      std::string fname = d.GetFile(i);
      if (reg.find(fname))
        {
        files.push_back(fname);
        }
      }
    }
}


void cmSystemTools::GlobDirs(const char *fullPath,
                             std::vector<std::string>& files)
{
  std::string path = fullPath;
  std::string::size_type pos = path.find("/*");
  if(pos == std::string::npos)
    {
    files.push_back(fullPath);
    return;
    }
  std::string startPath = path.substr(0, pos);
  std::string finishPath = path.substr(pos+2);

  cmsys::Directory d;
  if (d.Load(startPath.c_str()))
    {
    for (unsigned int i = 0; i < d.GetNumberOfFiles(); ++i)
      {
      if((std::string(d.GetFile(i)) != ".")
         && (std::string(d.GetFile(i)) != ".."))
        {
        std::string fname = startPath;
        fname +="/";
        fname += d.GetFile(i);
        if(cmSystemTools::FileIsDirectory(fname.c_str()))
          {
          fname += finishPath;
          cmSystemTools::GlobDirs(fname.c_str(), files);
          }
        }
      }
    }
}


void cmSystemTools::ExpandList(std::vector<std::string> const& arguments, 
                               std::vector<std::string>& newargs)
{
  std::vector<std::string>::const_iterator i;
  for(i = arguments.begin();i != arguments.end(); ++i)
    {
    cmSystemTools::ExpandListArgument(*i, newargs);
    }
}

void cmSystemTools::ExpandListArgument(const std::string& arg,
                                       std::vector<std::string>& newargs,
                                       bool emptyArgs)
{
  // If argument is empty, it is an empty list.
  if(arg.length() == 0 && !emptyArgs)
    {
    return;
    }
  // if there are no ; in the name then just copy the current string
  if(arg.find(';') == std::string::npos)
    {
    newargs.push_back(arg);
    return;
    }
  std::vector<char> newArgVec;
  // Break the string at non-escaped semicolons not nested in [].
  int squareNesting = 0;
  for(const char* c = arg.c_str(); *c; ++c)
    {
    switch(*c)
      {
      case '\\':
        {
        // We only want to allow escaping of semicolons.  Other
        // escapes should not be processed here.
        ++c;
        if(*c == ';')
          {
          newArgVec.push_back(*c);
          }
        else
          {
          newArgVec.push_back('\\');
          if(*c)
            {
            newArgVec.push_back(*c);
            }
          else
            {
            // Terminate the loop properly.
            --c;
            }
          }
        } break;
      case '[':
        {
        ++squareNesting;
        newArgVec.push_back(*c);
        } break;
      case ']':
        {
        --squareNesting;
        newArgVec.push_back(*c);
        } break;
      case ';':
        {
        // Break the string here if we are not nested inside square
        // brackets.
        if(squareNesting == 0)
          {
          if ( newArgVec.size() || emptyArgs )
            {
            // Add the last argument if the string is not empty.
            newArgVec.push_back(0);
            newargs.push_back(&*newArgVec.begin());
            newArgVec.clear();
            }
          }
        else
          {
          newArgVec.push_back(*c);
          }
        } break;
      default:
        {
        // Just append this character.
        newArgVec.push_back(*c);
        } break;
      }
    }
  if ( newArgVec.size() || emptyArgs )
    {
    // Add the last argument if the string is not empty.
    newArgVec.push_back(0);
    newargs.push_back(&*newArgVec.begin());
    }
}

bool cmSystemTools::SimpleGlob(const cmStdString& glob, 
                               std::vector<cmStdString>& files, 
                               int type /* = 0 */)
{
  files.clear();
  if ( glob[glob.size()-1] != '*' )
    {
    return false;
    }
  std::string path = cmSystemTools::GetFilenamePath(glob);
  std::string ppath = cmSystemTools::GetFilenameName(glob);
  ppath = ppath.substr(0, ppath.size()-1);
  if ( path.size() == 0 )
    {
    path = "/";
    }

  bool res = false;
  cmsys::Directory d;
  if (d.Load(path.c_str()))
    {
    for (unsigned int i = 0; i < d.GetNumberOfFiles(); ++i)
      {
      if((std::string(d.GetFile(i)) != ".")
         && (std::string(d.GetFile(i)) != ".."))
        {
        std::string fname = path;
        if ( path[path.size()-1] != '/' )
          {
          fname +="/";
          }
        fname += d.GetFile(i);
        std::string sfname = d.GetFile(i);
        if ( type > 0 && cmSystemTools::FileIsDirectory(fname.c_str()) )
          {
          continue;
          }
        if ( type < 0 && !cmSystemTools::FileIsDirectory(fname.c_str()) )
          {
          continue;
          }
        if ( sfname.size() >= ppath.size() && 
             sfname.substr(0, ppath.size()) == 
             ppath )
          {
          files.push_back(fname);
          res = true;
          }
        }
      }
    }
  return res;
}

cmSystemTools::FileFormat cmSystemTools::GetFileFormat(const char* cext)
{
  if ( ! cext || *cext == 0 )
    {
    return cmSystemTools::NO_FILE_FORMAT;
    }
  //std::string ext = cmSystemTools::LowerCase(cext);
  std::string ext = cext;
  if ( ext == "c" || ext == ".c" ) { return cmSystemTools::C_FILE_FORMAT; }
  if ( 
    ext == "C" || ext == ".C" ||
    ext == "M" || ext == ".M" ||
    ext == "c++" || ext == ".c++" ||
    ext == "cc" || ext == ".cc" ||
    ext == "cpp" || ext == ".cpp" ||
    ext == "cxx" || ext == ".cxx" ||
    ext == "m" || ext == ".m" ||
    ext == "mm" || ext == ".mm"
    ) { return cmSystemTools::CXX_FILE_FORMAT; }
  if ( 
    ext == "f" || ext == ".f" ||
    ext == "F" || ext == ".F" ||
    ext == "f77" || ext == ".f77" ||
    ext == "f90" || ext == ".f90" ||
    ext == "for" || ext == ".for" ||
    ext == "f95" || ext == ".f95" 
    ) { return cmSystemTools::FORTRAN_FILE_FORMAT; }
  if ( ext == "java" || ext == ".java" )
    { return cmSystemTools::JAVA_FILE_FORMAT; }
  if ( 
    ext == "H" || ext == ".H" || 
    ext == "h" || ext == ".h" || 
    ext == "h++" || ext == ".h++" ||
    ext == "hm" || ext == ".hm" || 
    ext == "hpp" || ext == ".hpp" || 
    ext == "hxx" || ext == ".hxx" ||
    ext == "in" || ext == ".in" ||
    ext == "txx" || ext == ".txx"
    ) { return cmSystemTools::HEADER_FILE_FORMAT; }
  if ( ext == "rc" || ext == ".rc" )
    { return cmSystemTools::RESOURCE_FILE_FORMAT; }
  if ( ext == "def" || ext == ".def" )
    { return cmSystemTools::DEFINITION_FILE_FORMAT; }
  if ( ext == "lib" || ext == ".lib" ||
       ext == "a" || ext == ".a")
    { return cmSystemTools::STATIC_LIBRARY_FILE_FORMAT; }
  if ( ext == "o" || ext == ".o" ||
       ext == "obj" || ext == ".obj") 
    { return cmSystemTools::OBJECT_FILE_FORMAT; }
#ifdef __APPLE__
  if ( ext == "dylib" || ext == ".dylib" ) 
    { return cmSystemTools::SHARED_LIBRARY_FILE_FORMAT; }
  if ( ext == "so" || ext == ".so" || 
       ext == "bundle" || ext == ".bundle" ) 
    { return cmSystemTools::MODULE_FILE_FORMAT; } 
#else // __APPLE__
  if ( ext == "so" || ext == ".so" || 
       ext == "sl" || ext == ".sl" || 
       ext == "dll" || ext == ".dll" ) 
    { return cmSystemTools::SHARED_LIBRARY_FILE_FORMAT; }
#endif // __APPLE__
  return cmSystemTools::UNKNOWN_FILE_FORMAT;
}

bool cmSystemTools::Split(const char* s, std::vector<cmStdString>& l)
{
  std::vector<std::string> temp;
  bool res = Superclass::Split(s, temp);
  for(std::vector<std::string>::const_iterator i = temp.begin();
      i != temp.end(); ++i)
    {
    l.push_back(*i);
    }
  return res;
}

std::string cmSystemTools::ConvertToOutputPath(const char* path)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
  if(s_ForceUnixPaths)
    {
    return cmSystemTools::ConvertToUnixOutputPath(path);
    }
  return cmSystemTools::ConvertToWindowsOutputPath(path);
#else
  return cmSystemTools::ConvertToUnixOutputPath(path);
#endif
}

void cmSystemTools::ConvertToOutputSlashes(std::string& path)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
  if(!s_ForceUnixPaths)
    {
    // Convert to windows slashes.
    std::string::size_type pos = 0;
    while((pos = path.find('/', pos)) != std::string::npos)
      {
      path[pos++] = '\\';
      }
    }
#else
  static_cast<void>(path);
#endif
}

std::string cmSystemTools::ConvertToRunCommandPath(const char* path)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
  return cmSystemTools::ConvertToWindowsOutputPath(path);
#else
  return cmSystemTools::ConvertToUnixOutputPath(path);
#endif
}

bool cmSystemTools::StringEndsWith(const char* str1, const char* str2)
{
  if ( !str1 || !str2 || strlen(str1) < strlen(str2) )
    {
    return 0;
    }
  return !strncmp(str1 + (strlen(str1)-strlen(str2)), str2, strlen(str2));
}

// compute the relative path from here to there
std::string cmSystemTools::RelativePath(const char* local, const char* remote)
{
  if(!cmSystemTools::FileIsFullPath(local))
    {
    cmSystemTools::Error("RelativePath must be passed a full path to local: ",
                         local);
    }
  if(!cmSystemTools::FileIsFullPath(remote))
    {
    cmSystemTools::Error
      ("RelativePath must be passed a full path to remote: ", remote);
    }
  return cmsys::SystemTools::RelativePath(local, remote);
}

class cmDeletingCharVector : public std::vector<char*>
{
public:
  ~cmDeletingCharVector()
    {
      for(std::vector<char*>::iterator i = this->begin();
          i != this->end(); ++i)
        {
        delete []*i;
        }
    }
};

        
bool cmSystemTools::PutEnv(const char* value)
{ 
  static cmDeletingCharVector localEnvironment;
  char* envVar = new char[strlen(value)+1];
  strcpy(envVar, value);
  int ret = putenv(envVar);
  // save the pointer in the static vector so that it can
  // be deleted on exit
  localEnvironment.push_back(envVar);
  return ret == 0;
}

#ifdef CMAKE_BUILD_WITH_CMAKE
bool cmSystemTools::UnsetEnv(const char* value)
{
#if !defined(HAVE_UNSETENV)
  std::string var = value;
  var += "=";
  return cmSystemTools::PutEnv(var.c_str());
#else
  unsetenv(value);
  return true;
#endif
}

std::vector<std::string> cmSystemTools::GetEnvironmentVariables()
{
  std::vector<std::string> env;
  int cc;
  for ( cc = 0; environ[cc]; ++ cc )
    {
    env.push_back(environ[cc]);
    }
  return env;
}
#endif

void cmSystemTools::EnableVSConsoleOutput()
{
  // Visual Studio 8 2005 (devenv.exe or VCExpress.exe) will not
  // display output to the console unless this environment variable is
  // set.  We need it to capture the output of these build tools.
  // Note for future work that one could pass "/out \\.\pipe\NAME" to
  // either of these executables where NAME is created with
  // CreateNamedPipe.  This would bypass the internal buffering of the
  // output and allow it to be captured on the fly.
#ifdef _WIN32
  cmSystemTools::PutEnv("vsconsoleoutput=1");
#endif
}

std::string cmSystemTools::MakeXMLSafe(const char* str)
{
  std::vector<char> result;
  result.reserve(500);
  const char* pos = str;
  for ( ;*pos; ++pos)
    {
    char ch = *pos;
    if ( (ch > 126 || ch < 32) && ch != 9  && ch != 10 && ch != 13 
         && ch != '\r' )
      {
      char buffer[33];
      sprintf(buffer, "&lt;%d&gt;", static_cast<int>(ch));
      //sprintf(buffer, "&#x%0x;", (unsigned int)ch);
      result.insert(result.end(), buffer, buffer+strlen(buffer));
      }
    else
      {
      const char* const encodedChars[] = {
        "&amp;",
        "&lt;",
        "&gt;"
      };
      switch ( ch )
        {
        case '&':
          result.insert(result.end(), encodedChars[0], encodedChars[0]+5);
          break;
        case '<':
          result.insert(result.end(), encodedChars[1], encodedChars[1]+4);
          break;
        case '>':
          result.insert(result.end(), encodedChars[2], encodedChars[2]+4);
          break;
        case '\n':
          result.push_back('\n');
          break;
        case '\r': break; // Ignore \r
        default:
          result.push_back(ch);
        }
      }
    }
  if ( result.size() == 0 )
    {
    return "";
    }
  return std::string(&*result.begin(), result.size());
}

bool cmSystemTools::IsPathToFramework(const char* path)
{
  if(cmSystemTools::FileIsFullPath(path))
    {
    std::string libname = path;
    if(libname.find(".framework") == libname.size()+1-sizeof(".framework"))
      {
      return true;
      }
    }
  return false;
}

#if defined(CMAKE_BUILD_WITH_CMAKE)
struct cmSystemToolsGZStruct
{
  gzFile GZFile;
};

extern "C" {
  int cmSystemToolsGZStructOpen(void* call_data, const char *pathname, 
                                int oflags, mode_t mode);
  int cmSystemToolsGZStructClose(void* call_data);
  ssize_t cmSystemToolsGZStructRead(void* call_data, void* buf, size_t count);
  ssize_t cmSystemToolsGZStructWrite(void* call_data, const void* buf, 
                                     size_t count);
}

int cmSystemToolsGZStructOpen(void* call_data, const char *pathname, 
                              int oflags, mode_t mode)
{
  const char *gzoflags;
  int fd;

  cmSystemToolsGZStruct* gzf = static_cast<cmSystemToolsGZStruct*>(call_data);

  switch (oflags & O_ACCMODE)
  {
  case O_WRONLY:
    gzoflags = "wb";
    break;
  case O_RDONLY:
    gzoflags = "rb";
    break;
  default:
  case O_RDWR:
    errno = EINVAL;
    return -1;
  }

  fd = open(pathname, oflags, mode);
  if (fd == -1)
    {
    return -1;
    }

// no fchmod on BeOS 5...do pathname instead.
#if defined(__BEOS__) && !defined(__ZETA__) 
  if ((oflags & O_CREAT) && chmod(pathname, mode))
    {
    return -1;
    }
#elif !defined(_WIN32) || defined(__CYGWIN__)
  if ((oflags & O_CREAT) && fchmod(fd, mode))
    {
    return -1;
    }
#endif

  gzf->GZFile = gzdopen(fd, gzoflags);
  if (!gzf->GZFile)
  {
    errno = ENOMEM;
    return -1;
  }

  return fd;
}

int cmSystemToolsGZStructClose(void* call_data)
{
  cmSystemToolsGZStruct* gzf = static_cast<cmSystemToolsGZStruct*>(call_data);
  return gzclose(gzf->GZFile);
}

ssize_t cmSystemToolsGZStructRead(void* call_data, void* buf, size_t count)
{
  cmSystemToolsGZStruct* gzf = static_cast<cmSystemToolsGZStruct*>(call_data);
  return gzread(gzf->GZFile, buf, static_cast<int>(count));
}

ssize_t cmSystemToolsGZStructWrite(void* call_data, const void* buf,
                                   size_t count)
{
  cmSystemToolsGZStruct* gzf = static_cast<cmSystemToolsGZStruct*>(call_data);
  return gzwrite(gzf->GZFile, (void*)buf, static_cast<int>(count));
}

#endif

bool cmSystemTools::CreateTar(const char* outFileName, 
                              const std::vector<cmStdString>& files,
                              bool gzip, bool verbose)
{
#if defined(CMAKE_BUILD_WITH_CMAKE)
  TAR *t;
  char buf[TAR_MAXPATHLEN];
  char pathname[TAR_MAXPATHLEN];
  cmSystemToolsGZStruct gzs;

  tartype_t gztype = {
    (openfunc_t)cmSystemToolsGZStructOpen,
    (closefunc_t)cmSystemToolsGZStructClose,
    (readfunc_t)cmSystemToolsGZStructRead,
    (writefunc_t)cmSystemToolsGZStructWrite,
    &gzs
  };

  // Ok, this libtar is not const safe. for now use auto_ptr hack
  char* realName = new char[ strlen(outFileName) + 1 ];
  std::auto_ptr<char> realNamePtr(realName);
  strcpy(realName, outFileName);
  int options = 0;
  if(verbose)
    {
    options |= TAR_VERBOSE;
    }
#ifdef __CYGWIN__
  options |= TAR_GNU;
#endif 
  if (tar_open(&t, realName,
      (gzip? &gztype : NULL),
      O_WRONLY | O_CREAT, 0644,
      options) == -1)
    {
    cmSystemTools::Error("Problem with tar_open(): ", strerror(errno));
    return false;
    }

  std::vector<cmStdString>::const_iterator it;
  for (it = files.begin(); it != files.end(); ++ it )
    {
    strncpy(pathname, it->c_str(), sizeof(pathname));
    pathname[sizeof(pathname)-1] = 0;
    strncpy(buf, pathname, sizeof(buf));
    buf[sizeof(buf)-1] = 0;
    if (tar_append_tree(t, buf, pathname) != 0)
      {
      cmOStringStream ostr;
      ostr << "Problem with tar_append_tree(\"" << buf << "\", \"" 
           << pathname << "\"): "
        << strerror(errno);
      cmSystemTools::Error(ostr.str().c_str());
      tar_close(t);
      return false;
      }
    }

  if (tar_append_eof(t) != 0)
    {
    cmSystemTools::Error("Problem with tar_append_eof(): ", strerror(errno));
    tar_close(t);
    return false;
    }

  if (tar_close(t) != 0)
    {
    cmSystemTools::Error("Problem with tar_close(): ", strerror(errno));
    return false;
    }
  return true;
#else
  (void)outFileName;
  (void)files;
  (void)gzip;
  (void)verbose;
  return false;
#endif
}

bool cmSystemTools::ExtractTar(const char* outFileName, 
                               const std::vector<cmStdString>& files, 
                               bool gzip, bool verbose)
{
  (void)files;
#if defined(CMAKE_BUILD_WITH_CMAKE)
  TAR *t;
  cmSystemToolsGZStruct gzs;

  tartype_t gztype = {
    cmSystemToolsGZStructOpen,
    cmSystemToolsGZStructClose,
    cmSystemToolsGZStructRead,
    cmSystemToolsGZStructWrite,
    &gzs
  };

  // Ok, this libtar is not const safe. for now use auto_ptr hack
  char* realName = new char[ strlen(outFileName) + 1 ];
  std::auto_ptr<char> realNamePtr(realName);
  strcpy(realName, outFileName);
  if (tar_open(&t, realName,
      (gzip? &gztype : NULL),
      O_RDONLY
#ifdef _WIN32
      | O_BINARY
#endif
      , 0,
      (verbose?TAR_VERBOSE:0)
      | 0) == -1)
    {
    cmSystemTools::Error("Problem with tar_open(): ", strerror(errno));
    return false;
    }

  if (tar_extract_all(t, 0) != 0)
  {
    cmSystemTools::Error("Problem with tar_extract_all(): ", strerror(errno));
    return false;
  }

  if (tar_close(t) != 0)
    {
    cmSystemTools::Error("Problem with tar_close(): ", strerror(errno));
    return false;
    }
  return true;
#else
  (void)outFileName;
  (void)gzip;
  (void)verbose;
  return false;
#endif
}

bool cmSystemTools::ListTar(const char* outFileName, 
                            std::vector<cmStdString>& files, bool gzip,
                            bool verbose)
{
#if defined(CMAKE_BUILD_WITH_CMAKE)
  TAR *t;
  cmSystemToolsGZStruct gzs;

  tartype_t gztype = {
    cmSystemToolsGZStructOpen,
    cmSystemToolsGZStructClose,
    cmSystemToolsGZStructRead,
    cmSystemToolsGZStructWrite,
    &gzs
  };

  // Ok, this libtar is not const safe. for now use auto_ptr hack
  char* realName = new char[ strlen(outFileName) + 1 ];
  std::auto_ptr<char> realNamePtr(realName);
  strcpy(realName, outFileName);
  if (tar_open(&t, realName,
      (gzip? &gztype : NULL),
      O_RDONLY
#ifdef _WIN32
      | O_BINARY
#endif
      , 0,
      (verbose?TAR_VERBOSE:0)
      | 0) == -1)
    {
    cmSystemTools::Error("Problem with tar_open(): ", strerror(errno));
    return false;
    }

  while ((th_read(t)) == 0)
  {
    const char* filename = th_get_pathname(t);
    files.push_back(filename);
   
    if ( verbose )
      {
      th_print_long_ls(t);
      }
    else
      {
      std::cout << filename << std::endl;
      }

#ifdef DEBUG
    th_print(t);
#endif
    if (TH_ISREG(t) && tar_skip_regfile(t) != 0)
      {
      cmSystemTools::Error("Problem with tar_skip_regfile(): ", 
                           strerror(errno));
      return false;
      }
  }

  if (tar_close(t) != 0)
    {
    cmSystemTools::Error("Problem with tar_close(): ", strerror(errno));
    return false;
    }
  return true;
#else
  (void)outFileName;
  (void)files;
  (void)gzip;
  (void)verbose;
  return false;
#endif
}

int cmSystemTools::WaitForLine(cmsysProcess* process, std::string& line,
                               double timeout,
                               std::vector<char>& out,
                               std::vector<char>& err)
{
  line = "";
  std::vector<char>::iterator outiter = out.begin();
  std::vector<char>::iterator erriter = err.begin();
  while(1)
    {
    // Check for a newline in stdout.
    for(;outiter != out.end(); ++outiter)
      {
      if((*outiter == '\r') && ((outiter+1) == out.end()))
        {
        break;
        }
      else if(*outiter == '\n' || *outiter == '\0')
        {
        int length = outiter-out.begin();
        if(length > 1 && *(outiter-1) == '\r')
          {
          --length;
          }
        if(length > 0)
          {
          line.append(&out[0], length);
          }
        out.erase(out.begin(), outiter+1);
        return cmsysProcess_Pipe_STDOUT;
        }
      }

    // Check for a newline in stderr.
    for(;erriter != err.end(); ++erriter)
      {
      if((*erriter == '\r') && ((erriter+1) == err.end()))
        {
        break;
        }
      else if(*erriter == '\n' || *erriter == '\0')
        {
        int length = erriter-err.begin();
        if(length > 1 && *(erriter-1) == '\r')
          {
          --length;
          }
        if(length > 0)
          {
          line.append(&err[0], length);
          }
        err.erase(err.begin(), erriter+1);
        return cmsysProcess_Pipe_STDERR;
        }
      }

    // No newlines found.  Wait for more data from the process.
    int length;
    char* data;
    int pipe = cmsysProcess_WaitForData(process, &data, &length, &timeout);
    if(pipe == cmsysProcess_Pipe_Timeout)
      {
      // Timeout has been exceeded.
      return pipe;
      }
    else if(pipe == cmsysProcess_Pipe_STDOUT)
      {
      // Append to the stdout buffer.
      std::vector<char>::size_type size = out.size();
      out.insert(out.end(), data, data+length);
      outiter = out.begin()+size;
      }
    else if(pipe == cmsysProcess_Pipe_STDERR)
      {
      // Append to the stderr buffer.
      std::vector<char>::size_type size = err.size();
      err.insert(err.end(), data, data+length);
      erriter = err.begin()+size;
      }
    else if(pipe == cmsysProcess_Pipe_None)
      {
      // Both stdout and stderr pipes have broken.  Return leftover data.
      if(!out.empty())
        {
        line.append(&out[0], outiter-out.begin());
        out.erase(out.begin(), out.end());
        return cmsysProcess_Pipe_STDOUT;
        }
      else if(!err.empty())
        {
        line.append(&err[0], erriter-err.begin());
        err.erase(err.begin(), err.end());
        return cmsysProcess_Pipe_STDERR;
        }
      else
        {
        return cmsysProcess_Pipe_None;
        }
      }
    }
}

void cmSystemTools::DoNotInheritStdPipes()
{   
#ifdef _WIN32  
  // Check to see if we are attached to a console
  // if so, then do not stop the inherited pipes
  // or stdout and stderr will not show up in dos
  // shell windows
  CONSOLE_SCREEN_BUFFER_INFO hOutInfo;
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if(GetConsoleScreenBufferInfo(hOut, &hOutInfo))
    {
    return;
    }
  {
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  DuplicateHandle(GetCurrentProcess(), out,
                  GetCurrentProcess(), &out, 0, FALSE,
                  DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
  SetStdHandle(STD_OUTPUT_HANDLE, out);
  }
  {
  HANDLE out = GetStdHandle(STD_ERROR_HANDLE);
  DuplicateHandle(GetCurrentProcess(), out,
                  GetCurrentProcess(), &out, 0, FALSE,
                  DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
  SetStdHandle(STD_ERROR_HANDLE, out);
  }
#endif
}

//----------------------------------------------------------------------------
bool cmSystemTools::CopyFileTime(const char* fromFile, const char* toFile)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
  cmSystemToolsWindowsHandle hFrom =
    CreateFile(fromFile, GENERIC_READ, FILE_SHARE_READ, 0,
               OPEN_EXISTING, 0, 0);
  cmSystemToolsWindowsHandle hTo =
    CreateFile(toFile, GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
  if(!hFrom || !hTo)
    {
    return false;
    }
  FILETIME timeCreation;
  FILETIME timeLastAccess;
  FILETIME timeLastWrite;
  if(!GetFileTime(hFrom, &timeCreation, &timeLastAccess, &timeLastWrite))
    {
    return false;
    }
  if(!SetFileTime(hTo, &timeCreation, &timeLastAccess, &timeLastWrite))
    {
    return false;
    }
#else
  struct stat fromStat;
  if(stat(fromFile, &fromStat) < 0)
    {
    return false;
    }

  struct utimbuf buf;
  buf.actime = fromStat.st_atime;
  buf.modtime = fromStat.st_mtime;
  if(utime(toFile, &buf) < 0)
    {
    return false;
    }
#endif
  return true;
}

//----------------------------------------------------------------------------
cmSystemToolsFileTime* cmSystemTools::FileTimeNew()
{
  return new cmSystemToolsFileTime;
}

//----------------------------------------------------------------------------
void cmSystemTools::FileTimeDelete(cmSystemToolsFileTime* t)
{
  delete t;
}

//----------------------------------------------------------------------------
bool cmSystemTools::FileTimeGet(const char* fname, cmSystemToolsFileTime* t)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
  cmSystemToolsWindowsHandle h =
    CreateFile(fname, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  if(!h)
    {
    return false;
    }
  if(!GetFileTime(h, &t->timeCreation, &t->timeLastAccess, &t->timeLastWrite))
    {
    return false;
    }
#else
  struct stat st;
  if(stat(fname, &st) < 0)
    {
    return false;
    }
  t->timeBuf.actime = st.st_atime;
  t->timeBuf.modtime = st.st_mtime;
#endif
  return true;
}

//----------------------------------------------------------------------------
bool cmSystemTools::FileTimeSet(const char* fname, cmSystemToolsFileTime* t)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
  cmSystemToolsWindowsHandle h =
    CreateFile(fname, GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
  if(!h)
    {
    return false;
    }
  if(!SetFileTime(h, &t->timeCreation, &t->timeLastAccess, &t->timeLastWrite))
    {
    return false;
    }
#else
  if(utime(fname, &t->timeBuf) < 0)
    {
    return false;
    }
#endif
  return true;
}

//----------------------------------------------------------------------------
static std::string cmSystemToolsExecutableDirectory;
void cmSystemTools::FindExecutableDirectory(const char* argv0)
{
  std::string errorMsg;
  std::string exe;
  if(cmSystemTools::FindProgramPath(argv0, exe, errorMsg))
    {
    // remove symlinks
    exe = cmSystemTools::GetRealPath(exe.c_str());
    cmSystemToolsExecutableDirectory =
      cmSystemTools::GetFilenamePath(exe.c_str());
    }
  else
    {
    // ???
    }
}

//----------------------------------------------------------------------------
const char* cmSystemTools::GetExecutableDirectory()
{
  return cmSystemToolsExecutableDirectory.c_str();
}

//----------------------------------------------------------------------------
#if defined(CMAKE_BUILD_WITH_CMAKE)
void cmSystemTools::MakefileColorEcho(int color, const char* message,
                                      bool newline, bool enabled)
{
  // On some platforms (an MSYS prompt) cmsysTerminal may not be able
  // to determine whether the stream is displayed on a tty.  In this
  // case it assumes no unless we tell it otherwise.  Since we want
  // color messages to be displayed for users we will assume yes.
  // However, we can test for some situations when the answer is most
  // likely no.
  int assumeTTY = cmsysTerminal_Color_AssumeTTY;
  if(cmSystemTools::GetEnv("DART_TEST_FROM_DART") ||
     cmSystemTools::GetEnv("DASHBOARD_TEST_FROM_CTEST") ||
     cmSystemTools::GetEnv("CTEST_INTERACTIVE_DEBUG_MODE"))
    {
    // Avoid printing color escapes during dashboard builds.
    assumeTTY = 0;
    }

  if(enabled)
    {
    cmsysTerminal_cfprintf(color | assumeTTY, stdout, "%s%s",
                           message, newline? "\n" : "");
    }
  else
    {
    // Color is disabled.  Print without color.
    fprintf(stdout, "%s%s", message, newline? "\n" : "");
    }
}
#endif

//----------------------------------------------------------------------------
bool cmSystemTools::GuessLibrarySOName(std::string const& fullPath,
                                       std::string& soname)
{
  // For ELF shared libraries use a real parser to get the correct
  // soname.
#if defined(CMAKE_USE_ELF_PARSER)
  cmELF elf(fullPath.c_str());
  if(elf)
    {
    return elf.GetSOName(soname);
    }
#endif

  // If the file is not a symlink we have no guess for its soname.
  if(!cmSystemTools::FileIsSymlink(fullPath.c_str()))
    {
    return false;
    }
  if(!cmSystemTools::ReadSymlink(fullPath.c_str(), soname))
    {
    return false;
    }

  // If the symlink has a path component we have no guess for the soname.
  if(!cmSystemTools::GetFilenamePath(soname).empty())
    {
    return false;
    }

  // If the symlink points at an extended version of the same name
  // assume it is the soname.
  std::string name = cmSystemTools::GetFilenameName(fullPath);
  if(soname.length() > name.length() &&
     soname.substr(0, name.length()) == name)
    {
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
#if defined(CMAKE_USE_ELF_PARSER)
std::string::size_type cmSystemToolsFindRPath(std::string const& have,
                                              std::string const& want)
{
  // Search for the desired rpath.
  std::string::size_type pos = have.find(want);

  // If the path is not present we are done.
  if(pos == std::string::npos)
    {
    return pos;
    }

  // Build a regex to match a properly separated path instance.
  std::string regex_str = "(^|:)(";
  for(std::string::const_iterator i = want.begin(); i != want.end(); ++i)
    {
    int ch = *i;
    if(!(('a' <= ch && ch <= 'z') ||
         ('A' <= ch && ch <= 'Z') ||
         ('0' <= ch && ch <= '9')))
      {
      // Escape the non-alphanumeric character.
      regex_str += "\\";
      }
    // Store the character.
    regex_str.append(1, static_cast<char>(ch));
    }
  regex_str += ")(:|$)";

  // Look for the separated path.
  cmsys::RegularExpression regex(regex_str.c_str());
  if(regex.find(have))
    {
    // Return the position of the path portion.
    return regex.start(2);
    }
  else
    {
    // The desired rpath was not found.
    return std::string::npos;
    }
}
#endif

//----------------------------------------------------------------------------
bool cmSystemTools::ChangeRPath(std::string const& file,
                                std::string const& oldRPath,
                                std::string const& newRPath,
                                std::string* emsg,
                                bool* changed)
{
#if defined(CMAKE_USE_ELF_PARSER)
  if(changed)
    {
    *changed = false;
    }
  unsigned long rpathPosition = 0;
  unsigned long rpathSize = 0;
  std::string rpathPrefix;
  std::string rpathSuffix;
  {
  // Parse the ELF binary.
  cmELF elf(file.c_str());

  // Get the RPATH or RUNPATH entry from it.
  cmELF::StringEntry const* se = elf.GetRPath();
  if(!se)
    {
    se = elf.GetRunPath();
    }

  if(se)
    {
    // Make sure the current rpath contains the old rpath.
    std::string::size_type pos = cmSystemToolsFindRPath(se->Value, oldRPath);
    if(pos == std::string::npos)
      {
      // If it contains the new rpath instead then it is okay.
      if(cmSystemToolsFindRPath(se->Value, newRPath) != std::string::npos)
        {
        return true;
        }
      if(emsg)
        {
        cmOStringStream e;
        e << "The current RPATH is:\n"
          << "  " << se->Value << "\n"
          << "which does not contain:\n"
          << "  " << oldRPath << "\n"
          << "as was expected.";
        *emsg = e.str();
        }
      return false;
      }

    // Store information about the entry.
    rpathPosition = se->Position;
    rpathSize = se->Size;

    // Store the part of the path we must preserve.
    rpathPrefix = se->Value.substr(0, pos);
    rpathSuffix = se->Value.substr(pos+oldRPath.length(), oldRPath.npos);
    }
  else if(newRPath.empty())
    {
    // The new rpath is empty and there is no rpath anyway so it is
    // okay.
    return true;
    }
  else
    {
    if(emsg)
      {
      *emsg = "No valid ELF RPATH entry exists in the file; ";
      *emsg += elf.GetErrorMessage();
      }
    return false;
    }
  }
  // Compute the full new rpath.
  std::string rpath = rpathPrefix;
  rpath += newRPath;
  rpath += rpathSuffix;

  // Make sure there is enough room to store the new rpath and at
  // least one null terminator.
  if(rpathSize < rpath.length()+1)
    {
    if(emsg)
      {
      *emsg = "The replacement RPATH is too long.";
      }
    return false;
    }

  // Open the file for update and seek to the RPATH position.
  std::ofstream f(file.c_str(),
                  std::ios::in | std::ios::out | std::ios::binary);
  if(!f)
    {
    if(emsg)
      {
      *emsg = "Error opening file for update.";
      }
    return false;
    }
  if(!f.seekp(rpathPosition))
    {
    if(emsg)
      {
      *emsg = "Error seeking to RPATH position.";
      }
    return false;
    }

  // Write the new rpath.  Follow it with enough null terminators to
  // fill the string table entry.
  f << rpath;
  for(unsigned long i=rpath.length(); i < rpathSize; ++i)
    {
    f << '\0';
    }

  // Make sure everything was okay.
  if(f)
    {
    if(changed)
      {
      *changed = true;
      }
    return true;
    }
  else
    {
    if(emsg)
      {
      *emsg = "Error writing the new rpath to the file.";
      }
    return false;
    }
#else
  (void)file;
  (void)oldRPath;
  (void)newRPath;
  (void)emsg;
  (void)changed;
  return false;
#endif
}

//----------------------------------------------------------------------------
bool cmSystemTools::RemoveRPath(std::string const& file, std::string* emsg)
{
#if defined(CMAKE_USE_ELF_PARSER)
  unsigned long rpathPosition = 0;
  unsigned long rpathSize = 0;
  unsigned long rpathEntryPosition = 0;
  std::vector<char> bytes;
  {
  // Parse the ELF binary.
  cmELF elf(file.c_str());

  // Get the RPATH or RUNPATH entry from it.
  cmELF::StringEntry const* se = elf.GetRPath();
  if(!se)
    {
    se = elf.GetRunPath();
    }

  if(se)
    {
    // Store information about the entry.
    rpathPosition = se->Position;
    rpathSize = se->Size;
    rpathEntryPosition = elf.GetDynamicEntryPosition(se->IndexInSection);

    // Get the file range containing the rest of the DYNAMIC table
    // after the RPATH entry.
    unsigned long nextEntryPosition =
      elf.GetDynamicEntryPosition(se->IndexInSection+1);
    unsigned int count = elf.GetDynamicEntryCount();
    if(count == 0)
      {
      // This should happen only for invalid ELF files where a DT_NULL
      // appears before the end of the table.
      if(emsg)
        {
        *emsg = "DYNAMIC section contains a DT_NULL before the end.";
        }
      return false;
      }
    unsigned long nullEntryPosition = elf.GetDynamicEntryPosition(count);

    // Allocate and fill a buffer with zeros.
    bytes.resize(nullEntryPosition - rpathEntryPosition, 0);

    // Read the part of the DYNAMIC section header that will move.
    // The remainder of the buffer will be left with zeros which
    // represent a DT_NULL entry.
    if(!elf.ReadBytes(nextEntryPosition,
                      nullEntryPosition - nextEntryPosition,
                      &bytes[0]))
      {
      if(emsg)
        {
        *emsg = "Failed to read DYNAMIC section header.";
        }
      return false;
      }
    }
  else
    {
    // There is no RPATH or RUNPATH anyway.
    return true;
    }
  }

  // Open the file for update.
  std::ofstream f(file.c_str(),
                  std::ios::in | std::ios::out | std::ios::binary);
  if(!f)
    {
    if(emsg)
      {
      *emsg = "Error opening file for update.";
      }
    return false;
    }

  // Write the new DYNAMIC table header.
  if(!f.seekp(rpathEntryPosition))
    {
    if(emsg)
      {
      *emsg = "Error seeking to DYNAMIC table header for RPATH.";
      }
    return false;
    }
  if(!f.write(&bytes[0], bytes.size()))
    {
    if(emsg)
      {
      *emsg = "Error replacing DYNAMIC table header.";
      }
    return false;
    }

  // Fill the RPATH string with zero bytes.
  if(!f.seekp(rpathPosition))
    {
    if(emsg)
      {
      *emsg = "Error seeking to RPATH position.";
      }
    return false;
    }
  for(unsigned long i=0; i < rpathSize; ++i)
    {
    f << '\0';
    }

  // Make sure everything was okay.
  if(f)
    {
    return true;
    }
  else
    {
    if(emsg)
      {
      *emsg = "Error writing the empty rpath to the file.";
      }
    return false;
    }
#else
  (void)file;
  (void)emsg;
  return false;
#endif
}

//----------------------------------------------------------------------------
bool cmSystemTools::CheckRPath(std::string const& file,
                               std::string const& newRPath)
{
#if defined(CMAKE_USE_ELF_PARSER)
  // Parse the ELF binary.
  cmELF elf(file.c_str());

  // Get the RPATH or RUNPATH entry from it.
  cmELF::StringEntry const* se = elf.GetRPath();
  if(!se)
    {
    se = elf.GetRunPath();
    }

  // Make sure the current rpath contains the new rpath.
  if(newRPath.empty())
    {
    if(!se)
      {
      return true;
      }
    }
  else
    {
    if(se &&
       cmSystemToolsFindRPath(se->Value, newRPath) != std::string::npos)
      {
      return true;
      }
    }
  return false;
#else
  (void)file;
  (void)newRPath;
  return false;
#endif
}
