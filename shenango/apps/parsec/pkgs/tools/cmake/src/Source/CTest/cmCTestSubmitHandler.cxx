/*=========================================================================

Program:   CMake - Cross-Platform Makefile Generator
Module:    $RCSfile: cmCTestSubmitHandler.cxx,v $
Language:  C++
Date:      $Date: 2012/03/29 17:21:10 $
Version:   $Revision: 1.1.1.1 $

Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmCTestSubmitHandler.h"

#include "cmSystemTools.h"
#include "cmVersion.h"
#include "cmGeneratedFileStream.h"
#include "cmCTest.h"

#include <cmsys/Process.h>
#include <cmsys/Base64.h>

// For XML-RPC submission
#include "cm_xmlrpc.h"

// For curl submission
#include "cm_curl.h"

#include <sys/stat.h>

typedef std::vector<char> cmCTestSubmitHandlerVectorOfChar;

static size_t
cmCTestSubmitHandlerWriteMemoryCallback(void *ptr, size_t size, size_t nmemb,
  void *data)
{
  register int realsize = (int)(size * nmemb);

  cmCTestSubmitHandlerVectorOfChar *vec
    = static_cast<cmCTestSubmitHandlerVectorOfChar*>(data);
  const char* chPtr = static_cast<char*>(ptr);
  vec->insert(vec->end(), chPtr, chPtr + realsize);

  return realsize;
}

static size_t
cmCTestSubmitHandlerCurlDebugCallback(CURL *, curl_infotype, char *chPtr,
  size_t size, void *data)
{
  cmCTestSubmitHandlerVectorOfChar *vec
    = static_cast<cmCTestSubmitHandlerVectorOfChar*>(data);
  vec->insert(vec->end(), chPtr, chPtr + size);

  return size;
}

//----------------------------------------------------------------------------
cmCTestSubmitHandler::cmCTestSubmitHandler() : HTTPProxy(), FTPProxy()
{
  this->HTTPProxy = "";
  this->HTTPProxyType = 0;
  this->HTTPProxyAuth = "";

  this->FTPProxy = "";
  this->FTPProxyType = 0;
  this->CDash = false;
}

//----------------------------------------------------------------------------
void cmCTestSubmitHandler::Initialize()
{
  this->CDash = false;
  this->Superclass::Initialize();
  this->HTTPProxy = "";
  this->HTTPProxyType = 0;
  this->HTTPProxyAuth = "";
  this->FTPProxy = "";
  this->FTPProxyType = 0;
  this->LogFile = 0;
}

//----------------------------------------------------------------------------
bool cmCTestSubmitHandler::SubmitUsingFTP(const cmStdString& localprefix,
  const std::set<cmStdString>& files,
  const cmStdString& remoteprefix,
  const cmStdString& url)
{
  CURL *curl;
  CURLcode res;
  FILE* ftpfile;
  char error_buffer[1024];

  /* In windows, this will init the winsock stuff */
  ::curl_global_init(CURL_GLOBAL_ALL);

  cmCTest::SetOfStrings::const_iterator file;
  for ( file = files.begin(); file != files.end(); ++file )
    {
    /* get a curl handle */
    curl = curl_easy_init();
    if(curl)
      {
      // Using proxy
      if ( this->FTPProxyType > 0 )
        {
        curl_easy_setopt(curl, CURLOPT_PROXY, this->FTPProxy.c_str());
        switch (this->FTPProxyType)
          {
        case 2:
          curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4);
          break;
        case 3:
          curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
          break;
        default:
          curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
          }
        }

      // enable uploading
      ::curl_easy_setopt(curl, CURLOPT_UPLOAD, 1) ;

      cmStdString local_file = *file;
      if ( !cmSystemTools::FileExists(local_file.c_str()) )
        {
        local_file = localprefix + "/" + *file;
        }
      cmStdString upload_as
        = url + "/" + remoteprefix + cmSystemTools::GetFilenameName(*file);

      struct stat st;
      if ( ::stat(local_file.c_str(), &st) )
        {
        cmCTestLog(this->CTest, ERROR_MESSAGE, "   Cannot find file: "
          << local_file.c_str() << std::endl);
        ::curl_easy_cleanup(curl);
        ::curl_global_cleanup();
        return false;
        }

      ftpfile = ::fopen(local_file.c_str(), "rb");
      *this->LogFile << "\tUpload file: " << local_file.c_str() << " to "
          << upload_as.c_str() << std::endl;
      cmCTestLog(this->CTest, HANDLER_VERBOSE_OUTPUT, "   Upload file: "
        << local_file.c_str() << " to "
        << upload_as.c_str() << std::endl);

      ::curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

      // specify target
      ::curl_easy_setopt(curl,CURLOPT_URL, upload_as.c_str());

      // now specify which file to upload
      ::curl_easy_setopt(curl, CURLOPT_INFILE, ftpfile);

      // and give the size of the upload (optional)
      ::curl_easy_setopt(curl, CURLOPT_INFILESIZE,
        static_cast<long>(st.st_size));

      // and give curl the buffer for errors
      ::curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, &error_buffer);

      // specify handler for output
      ::curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        cmCTestSubmitHandlerWriteMemoryCallback);
      ::curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION,
        cmCTestSubmitHandlerCurlDebugCallback);

      /* we pass our 'chunk' struct to the callback function */
      cmCTestSubmitHandlerVectorOfChar chunk;
      cmCTestSubmitHandlerVectorOfChar chunkDebug;
      ::curl_easy_setopt(curl, CURLOPT_FILE, (void *)&chunk);
      ::curl_easy_setopt(curl, CURLOPT_DEBUGDATA, (void *)&chunkDebug);

      // Now run off and do what you've been told!
      res = ::curl_easy_perform(curl);

      if ( chunk.size() > 0 )
        {
        cmCTestLog(this->CTest, DEBUG, "CURL output: ["
          << cmCTestLogWrite(&*chunk.begin(), chunk.size()) << "]"
          << std::endl);
        }
      if ( chunkDebug.size() > 0 )
        {
        cmCTestLog(this->CTest, DEBUG, "CURL debug output: ["
          << cmCTestLogWrite(&*chunkDebug.begin(), chunkDebug.size()) << "]"
          << std::endl);
        }

      fclose(ftpfile);
      if ( res )
        {
        cmCTestLog(this->CTest, ERROR_MESSAGE,
          "   Error when uploading file: "
          << local_file.c_str() << std::endl);
        cmCTestLog(this->CTest, ERROR_MESSAGE, "   Error message was: "
          << error_buffer << std::endl);
        *this->LogFile << "   Error when uploading file: "
                       << local_file.c_str()
                       << std::endl
                       << "   Error message was: " 
                       << error_buffer << std::endl
                       << "   Curl output was: ";
        // avoid dereference of empty vector
        if(chunk.size())
          {
          *this->LogFile << cmCTestLogWrite(&*chunk.begin(), chunk.size());
          cmCTestLog(this->CTest, ERROR_MESSAGE, "CURL output: ["
                     << cmCTestLogWrite(&*chunk.begin(), chunk.size()) << "]"
                     << std::endl);
          }
        *this->LogFile << std::endl;
        ::curl_easy_cleanup(curl);
        ::curl_global_cleanup();
        return false;
        }
      // always cleanup
      ::curl_easy_cleanup(curl);
      cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Uploaded: " + local_file
        << std::endl);
      }
    }
  ::curl_global_cleanup();
  return true;
}

//----------------------------------------------------------------------------
// Uploading files is simpler
bool cmCTestSubmitHandler::SubmitUsingHTTP(const cmStdString& localprefix,
  const std::set<cmStdString>& files,
  const cmStdString& remoteprefix,
  const cmStdString& url)
{
  CURL *curl;
  CURLcode res;
  FILE* ftpfile;
  char error_buffer[1024];

  /* In windows, this will init the winsock stuff */
  ::curl_global_init(CURL_GLOBAL_ALL);

  cmStdString::size_type kk;
  cmCTest::SetOfStrings::const_iterator file;
  for ( file = files.begin(); file != files.end(); ++file )
    {
    /* get a curl handle */
    curl = curl_easy_init();
    if(curl)
      {

      // Using proxy
      if ( this->HTTPProxyType > 0 )
        {
        curl_easy_setopt(curl, CURLOPT_PROXY, this->HTTPProxy.c_str());
        switch (this->HTTPProxyType)
          {
        case 2:
          curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4);
          break;
        case 3:
          curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
          break;
        default:
          curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
          if (this->HTTPProxyAuth.size() > 0)
            {
            curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD,
              this->HTTPProxyAuth.c_str());
            }
          }
        }

      /* enable uploading */
      curl_easy_setopt(curl, CURLOPT_UPLOAD, 1) ;

      /* HTTP PUT please */
      ::curl_easy_setopt(curl, CURLOPT_PUT, 1);
      ::curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

      cmStdString local_file = *file;
      if ( !cmSystemTools::FileExists(local_file.c_str()) )
        {
        local_file = localprefix + "/" + *file;
        }
      cmStdString remote_file
        = remoteprefix + cmSystemTools::GetFilenameName(*file);

      *this->LogFile << "\tUpload file: " << local_file.c_str() << " to "
          << remote_file.c_str() << std::endl;

      cmStdString ofile = "";
      for ( kk = 0; kk < remote_file.size(); kk ++ )
        {
        char c = remote_file[kk];
        char hexCh[4] = { 0, 0, 0, 0 };
        hexCh[0] = c;
        switch ( c )
          {
        case '+':
        case '?':
        case '/':
        case '\\':
        case '&':
        case ' ':
        case '=':
        case '%':
          sprintf(hexCh, "%%%02X", (int)c);
          ofile.append(hexCh);
          break;
        default:
          ofile.append(hexCh);
          }
        }
      cmStdString upload_as
        = url + ((url.find("?",0) == cmStdString::npos) ? "?" : "&")
        + "FileName=" + ofile;

      struct stat st;
      if ( ::stat(local_file.c_str(), &st) )
        {
        cmCTestLog(this->CTest, ERROR_MESSAGE, "   Cannot find file: "
          << local_file.c_str() << std::endl);
        ::curl_easy_cleanup(curl);
        ::curl_global_cleanup();
        return false;
        }

      ftpfile = ::fopen(local_file.c_str(), "rb");
      cmCTestLog(this->CTest, HANDLER_VERBOSE_OUTPUT, "   Upload file: "
        << local_file.c_str() << " to "
        << upload_as.c_str() << " Size: " << st.st_size << std::endl);


      // specify target
      ::curl_easy_setopt(curl,CURLOPT_URL, upload_as.c_str());

      // now specify which file to upload
      ::curl_easy_setopt(curl, CURLOPT_INFILE, ftpfile);

      // and give the size of the upload (optional)
      ::curl_easy_setopt(curl, CURLOPT_INFILESIZE,
        static_cast<long>(st.st_size));

      // and give curl the buffer for errors
      ::curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, &error_buffer);

      // specify handler for output
      ::curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        cmCTestSubmitHandlerWriteMemoryCallback);
      ::curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION,
        cmCTestSubmitHandlerCurlDebugCallback);

      /* we pass our 'chunk' struct to the callback function */
      cmCTestSubmitHandlerVectorOfChar chunk;
      cmCTestSubmitHandlerVectorOfChar chunkDebug;
      ::curl_easy_setopt(curl, CURLOPT_FILE, (void *)&chunk);
      ::curl_easy_setopt(curl, CURLOPT_DEBUGDATA, (void *)&chunkDebug);

      // Now run off and do what you've been told!
      res = ::curl_easy_perform(curl);

      if ( chunk.size() > 0 )
        {
        cmCTestLog(this->CTest, DEBUG, "CURL output: ["
          << cmCTestLogWrite(&*chunk.begin(), chunk.size()) << "]"
          << std::endl);
        }
      if ( chunkDebug.size() > 0 )
        {
        cmCTestLog(this->CTest, DEBUG, "CURL debug output: ["
          << cmCTestLogWrite(&*chunkDebug.begin(), chunkDebug.size()) << "]"
          << std::endl);
        }

      fclose(ftpfile);
      if ( res )
        {
        cmCTestLog(this->CTest, ERROR_MESSAGE,
          "   Error when uploading file: "
          << local_file.c_str() << std::endl);
        cmCTestLog(this->CTest, ERROR_MESSAGE, "   Error message was: "
          << error_buffer << std::endl);
        *this->LogFile << "   Error when uploading file: "
                       << local_file.c_str()
                       << std::endl
                       << "   Error message was: " << error_buffer 
                       << std::endl;
        // avoid deref of begin for zero size array
        if(chunk.size())
          {
          *this->LogFile << "   Curl output was: "
                         << cmCTestLogWrite(&*chunk.begin(), chunk.size())
                         << std::endl;
          cmCTestLog(this->CTest, ERROR_MESSAGE, "CURL output: ["
                     << cmCTestLogWrite(&*chunk.begin(), chunk.size()) << "]"
                     << std::endl);
          }
        ::curl_easy_cleanup(curl);
        ::curl_global_cleanup();
        return false;
        }
      // always cleanup
      ::curl_easy_cleanup(curl);
      cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Uploaded: " + local_file
        << std::endl);
      }
    }
  ::curl_global_cleanup();
  return true;
}

//----------------------------------------------------------------------------
bool cmCTestSubmitHandler::TriggerUsingHTTP(
  const std::set<cmStdString>& files,
  const cmStdString& remoteprefix,
  const cmStdString& url)
{
  CURL *curl;
  char error_buffer[1024];

  /* In windows, this will init the winsock stuff */
  ::curl_global_init(CURL_GLOBAL_ALL);

  cmCTest::SetOfStrings::const_iterator file;
  for ( file = files.begin(); file != files.end(); ++file )
    {
    /* get a curl handle */
    curl = curl_easy_init();
    if(curl)
      {
      // Using proxy
      if ( this->HTTPProxyType > 0 )
        {
        curl_easy_setopt(curl, CURLOPT_PROXY, this->HTTPProxy.c_str());
        switch (this->HTTPProxyType)
          {
        case 2:
          curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4);
          break;
        case 3:
          curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
          break;
        default:
          curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
          if (this->HTTPProxyAuth.size() > 0)
            {
            curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD,
              this->HTTPProxyAuth.c_str());
            }
          }
        }

      ::curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

      // and give curl the buffer for errors
      ::curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, &error_buffer);

      // specify handler for output
      ::curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        cmCTestSubmitHandlerWriteMemoryCallback);
      ::curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION,
        cmCTestSubmitHandlerCurlDebugCallback);

      /* we pass our 'chunk' struct to the callback function */
      cmCTestSubmitHandlerVectorOfChar chunk;
      cmCTestSubmitHandlerVectorOfChar chunkDebug;
      ::curl_easy_setopt(curl, CURLOPT_FILE, (void *)&chunk);
      ::curl_easy_setopt(curl, CURLOPT_DEBUGDATA, (void *)&chunkDebug);

      cmStdString rfile
        = remoteprefix + cmSystemTools::GetFilenameName(*file);
      cmStdString ofile = "";
      cmStdString::iterator kk;
      for ( kk = rfile.begin(); kk < rfile.end(); ++ kk)
        {
        char c = *kk;
        char hexCh[4] = { 0, 0, 0, 0 };
        hexCh[0] = c;
        switch ( c )
          {
        case '+':
        case '?':
        case '/':
        case '\\':
        case '&':
        case ' ':
        case '=':
        case '%':
          sprintf(hexCh, "%%%02X", (int)c);
          ofile.append(hexCh);
          break;
        default:
          ofile.append(hexCh);
          }
        }
      cmStdString turl
        = url + ((url.find("?",0) == cmStdString::npos) ? "?" : "&")
        + "xmlfile=" + ofile;
      *this->LogFile << "Trigger url: " << turl.c_str() << std::endl;
      cmCTestLog(this->CTest, HANDLER_VERBOSE_OUTPUT, "   Trigger url: "
        << turl.c_str() << std::endl);
      curl_easy_setopt(curl, CURLOPT_URL, turl.c_str());
      if ( curl_easy_perform(curl) )
        {
        cmCTestLog(this->CTest, ERROR_MESSAGE, "   Error when triggering: "
          << turl.c_str() << std::endl);
        cmCTestLog(this->CTest, ERROR_MESSAGE, "   Error message was: "
          << error_buffer << std::endl);
        *this->LogFile << "\tTrigerring failed with error: " << error_buffer
                       << std::endl
                       << "   Error message was: " << error_buffer 
                       << std::endl;
        if(chunk.size())
          {
          *this->LogFile
            << "   Curl output was: "
            << cmCTestLogWrite(&*chunk.begin(), chunk.size()) << std::endl;
          cmCTestLog(this->CTest, ERROR_MESSAGE, "CURL output: ["
                     << cmCTestLogWrite(&*chunk.begin(), chunk.size()) << "]"
                     << std::endl);
          }
        ::curl_easy_cleanup(curl);
        ::curl_global_cleanup();
        return false;
        }

      if ( chunk.size() > 0 )
        {
        cmCTestLog(this->CTest, DEBUG, "CURL output: ["
          << cmCTestLogWrite(&*chunk.begin(), chunk.size()) << "]"
          << std::endl);
        }
      if ( chunkDebug.size() > 0 )
        {
        cmCTestLog(this->CTest, DEBUG, "CURL debug output: ["
          << cmCTestLogWrite(&*chunkDebug.begin(), chunkDebug.size())
          << "]" << std::endl);
        }

      // always cleanup
      ::curl_easy_cleanup(curl);
      cmCTestLog(this->CTest, HANDLER_VERBOSE_OUTPUT, std::endl);
      }
    }
  ::curl_global_cleanup();
  cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Dart server triggered..."
    << std::endl);
  return true;
}

//----------------------------------------------------------------------------
bool cmCTestSubmitHandler::SubmitUsingSCP(
  const cmStdString& scp_command,
  const cmStdString& localprefix,
  const std::set<cmStdString>& files,
  const cmStdString& remoteprefix,
  const cmStdString& url)
{
  if ( !scp_command.size() || !localprefix.size() ||
    !files.size() || !remoteprefix.size() || !url.size() )
    {
    return 0;
    }
  std::vector<const char*> argv;
  argv.push_back(scp_command.c_str()); // Scp command
  argv.push_back(scp_command.c_str()); // Dummy string for file
  argv.push_back(scp_command.c_str()); // Dummy string for remote url
  argv.push_back(0);

  cmsysProcess* cp = cmsysProcess_New();
  cmsysProcess_SetOption(cp, cmsysProcess_Option_HideWindow, 1);
  //cmsysProcess_SetTimeout(cp, timeout);

  int problems = 0;

  cmCTest::SetOfStrings::const_iterator file;
  for ( file = files.begin(); file != files.end(); ++file )
    {
    int retVal;

    std::string lfname = localprefix;
    cmSystemTools::ConvertToUnixSlashes(lfname);
    lfname += "/" + *file;
    lfname = cmSystemTools::ConvertToOutputPath(lfname.c_str());
    argv[1] = lfname.c_str();
    std::string rfname = url + "/" + remoteprefix + *file;
    argv[2] = rfname.c_str();
    cmCTestLog(this->CTest, HANDLER_VERBOSE_OUTPUT, "Execute \"" << argv[0]
      << "\" \"" << argv[1] << "\" \""
      << argv[2] << "\"" << std::endl);
    *this->LogFile << "Execute \"" << argv[0] << "\" \"" << argv[1] << "\" \""
      << argv[2] << "\"" << std::endl;

    cmsysProcess_SetCommand(cp, &*argv.begin());
    cmsysProcess_Execute(cp);
    char* data;
    int length;

    while(cmsysProcess_WaitForData(cp, &data, &length, 0))
      {
      cmCTestLog(this->CTest, HANDLER_VERBOSE_OUTPUT,
        cmCTestLogWrite(data, length));
      }

    cmsysProcess_WaitForExit(cp, 0);

    int result = cmsysProcess_GetState(cp);

    if(result == cmsysProcess_State_Exited)
      {
      retVal = cmsysProcess_GetExitValue(cp);
      if ( retVal != 0 )
        {
        cmCTestLog(this->CTest, HANDLER_VERBOSE_OUTPUT, "\tSCP returned: "
          << retVal << std::endl);
        *this->LogFile << "\tSCP returned: " << retVal << std::endl;
        problems ++;
        }
      }
    else if(result == cmsysProcess_State_Exception)
      {
      retVal = cmsysProcess_GetExitException(cp);
      cmCTestLog(this->CTest, ERROR_MESSAGE, "\tThere was an exception: "
        << retVal << std::endl);
      *this->LogFile << "\tThere was an exception: " << retVal << std::endl;
      problems ++;
      }
    else if(result == cmsysProcess_State_Expired)
      {
      cmCTestLog(this->CTest, ERROR_MESSAGE, "\tThere was a timeout"
        << std::endl);
      *this->LogFile << "\tThere was a timeout" << std::endl;
      problems ++;
      }
    else if(result == cmsysProcess_State_Error)
      {
      cmCTestLog(this->CTest, ERROR_MESSAGE, "\tError executing SCP: "
        << cmsysProcess_GetErrorString(cp) << std::endl);
      *this->LogFile << "\tError executing SCP: "
        << cmsysProcess_GetErrorString(cp) << std::endl;
      problems ++;
      }
    }
  cmsysProcess_Delete(cp);
  if ( problems )
    {
    return false;
    }
  return true;
}

//----------------------------------------------------------------------------
bool cmCTestSubmitHandler::SubmitUsingXMLRPC(const cmStdString& localprefix,
  const std::set<cmStdString>& files,
  const cmStdString& remoteprefix,
  const cmStdString& url)
{
  xmlrpc_env env;
  char ctestString[] = "CTest";
  std::string ctestVersionString = cmVersion::GetCMakeVersion();
  char* ctestVersion = const_cast<char*>(ctestVersionString.c_str());

  cmStdString realURL = url + "/" + remoteprefix + "/Command/";

  /* Start up our XML-RPC client library. */
  xmlrpc_client_init(XMLRPC_CLIENT_NO_FLAGS, ctestString, ctestVersion);

  /* Initialize our error-handling environment. */
  xmlrpc_env_init(&env);

  /* Call the famous server at UserLand. */
  cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Submitting to: "
    << realURL.c_str() << " (" << remoteprefix.c_str() << ")" << std::endl);
  cmCTest::SetOfStrings::const_iterator file;
  for ( file = files.begin(); file != files.end(); ++file )
    {
    xmlrpc_value *result;

    cmStdString local_file = *file;
    if ( !cmSystemTools::FileExists(local_file.c_str()) )
      {
      local_file = localprefix + "/" + *file;
      }
    cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Submit file: "
      << local_file.c_str() << std::endl);
    struct stat st;
    if ( ::stat(local_file.c_str(), &st) )
      {
      cmCTestLog(this->CTest, ERROR_MESSAGE, "  Cannot find file: "
        << local_file.c_str() << std::endl);
      return false;
      }

    // off_t can be bigger than size_t.  fread takes size_t.
    // make sure the file is not too big.
    if(static_cast<off_t>(static_cast<size_t>(st.st_size)) !=
       static_cast<off_t>(st.st_size))
      {
      cmCTestLog(this->CTest, ERROR_MESSAGE, "  File too big: "
        << local_file.c_str() << std::endl);
      return false;
      }
    size_t fileSize = static_cast<size_t>(st.st_size);
    FILE* fp = fopen(local_file.c_str(), "rb");
    if ( !fp )
      {
      cmCTestLog(this->CTest, ERROR_MESSAGE, "  Cannot open file: "
        << local_file.c_str() << std::endl);
      return false;
      }

    unsigned char *fileBuffer = new unsigned char[fileSize];
    if ( fread(fileBuffer, 1, fileSize, fp) != fileSize )
      {
      delete [] fileBuffer;
      fclose(fp);
      cmCTestLog(this->CTest, ERROR_MESSAGE, "  Cannot read file: "
        << local_file.c_str() << std::endl);
      return false;
      }
    fclose(fp);

    char remoteCommand[] = "Submit.put";
    char* pRealURL = const_cast<char*>(realURL.c_str());
    result = xmlrpc_client_call(&env, pRealURL, remoteCommand,
      "(6)", fileBuffer, (xmlrpc_int32)fileSize );

    delete [] fileBuffer;

    if ( env.fault_occurred )
      {
      cmCTestLog(this->CTest, ERROR_MESSAGE, " Submission problem: "
        << env.fault_string << " (" << env.fault_code << ")" << std::endl);
      xmlrpc_env_clean(&env);
      xmlrpc_client_cleanup();
      return false;
      }

    /* Dispose of our result value. */
    xmlrpc_DECREF(result);
    }

  /* Clean up our error-handling environment. */
  xmlrpc_env_clean(&env);

  /* Shutdown our XML-RPC client library. */
  xmlrpc_client_cleanup();
  return true;
}

//----------------------------------------------------------------------------
int cmCTestSubmitHandler::ProcessHandler()
{
  std::string iscdash = this->CTest->GetCTestConfiguration("IsCDash");
  // cdash does not need to trigger so just return true
  if(iscdash.size())
    {
    this->CDash = true;
    }

  const std::string &buildDirectory
    = this->CTest->GetCTestConfiguration("BuildDirectory");
  if ( buildDirectory.size() == 0 )
    {
    cmCTestLog(this->CTest, ERROR_MESSAGE,
      "Cannot find BuildDirectory  key in the DartConfiguration.tcl"
      << std::endl);
    return -1;
    }

  if ( getenv("HTTP_PROXY") )
    {
    this->HTTPProxyType = 1;
    this->HTTPProxy = getenv("HTTP_PROXY");
    if ( getenv("HTTP_PROXY_PORT") )
      {
      this->HTTPProxy += ":";
      this->HTTPProxy += getenv("HTTP_PROXY_PORT");
      }
    if ( getenv("HTTP_PROXY_TYPE") )
      {
      cmStdString type = getenv("HTTP_PROXY_TYPE");
      // HTTP/SOCKS4/SOCKS5
      if ( type == "HTTP" )
        {
        this->HTTPProxyType = 1;
        }
      else if ( type == "SOCKS4" )
        {
        this->HTTPProxyType = 2;
        }
      else if ( type == "SOCKS5" )
        {
        this->HTTPProxyType = 3;
        }
      }
    if ( getenv("HTTP_PROXY_USER") )
      {
      this->HTTPProxyAuth = getenv("HTTP_PROXY_USER");
      }
    if ( getenv("HTTP_PROXY_PASSWD") )
      {
      this->HTTPProxyAuth += ":";
      this->HTTPProxyAuth += getenv("HTTP_PROXY_PASSWD");
      }
    }

  if ( getenv("FTP_PROXY") )
    {
    this->FTPProxyType = 1;
    this->FTPProxy = getenv("FTP_PROXY");
    if ( getenv("FTP_PROXY_PORT") )
      {
      this->FTPProxy += ":";
      this->FTPProxy += getenv("FTP_PROXY_PORT");
      }
    if ( getenv("FTP_PROXY_TYPE") )
      {
      cmStdString type = getenv("FTP_PROXY_TYPE");
      // HTTP/SOCKS4/SOCKS5
      if ( type == "HTTP" )
        {
        this->FTPProxyType = 1;
        }
      else if ( type == "SOCKS4" )
        {
        this->FTPProxyType = 2;
        }
      else if ( type == "SOCKS5" )
        {
        this->FTPProxyType = 3;
        }
      }
    }

  if ( this->HTTPProxy.size() > 0 )
    {
    cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Use HTTP Proxy: "
      << this->HTTPProxy << std::endl);
    }
  if ( this->FTPProxy.size() > 0 )
    {
    cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Use FTP Proxy: "
      << this->FTPProxy << std::endl);
    }
  cmGeneratedFileStream ofs;
  this->StartLogFile("Submit", ofs);

  cmCTest::SetOfStrings files;
  std::string prefix = this->GetSubmitResultsPrefix();
  // TODO:
  // Check if test is enabled
  this->CTest->AddIfExists(files, "Update.xml");
  this->CTest->AddIfExists(files, "Configure.xml");
  this->CTest->AddIfExists(files, "Build.xml");
  this->CTest->AddIfExists(files, "Test.xml");
  if ( this->CTest->AddIfExists(files, "Coverage.xml") )
    {
    cmCTest::VectorOfStrings gfiles;
    std::string gpath
      = buildDirectory + "/Testing/" + this->CTest->GetCurrentTag();
    std::string::size_type glen = gpath.size() + 1;
    gpath = gpath + "/CoverageLog*";
    cmCTestLog(this->CTest, DEBUG, "Globbing for: " << gpath.c_str()
      << std::endl);
    if ( cmSystemTools::SimpleGlob(gpath, gfiles, 1) )
      {
      size_t cc;
      for ( cc = 0; cc < gfiles.size(); cc ++ )
        {
        gfiles[cc] = gfiles[cc].substr(glen);
        cmCTestLog(this->CTest, DEBUG, "Glob file: " << gfiles[cc].c_str()
          << std::endl);
        files.insert(gfiles[cc]);
        }
      }
    else
      {
      cmCTestLog(this->CTest, ERROR_MESSAGE, "Problem globbing" << std::endl);
      }
    }
  this->CTest->AddIfExists(files, "DynamicAnalysis.xml");
  this->CTest->AddIfExists(files, "Purify.xml");
  this->CTest->AddIfExists(files, "Notes.xml");

  cmCTest::SetOfStrings::iterator it;
  for ( it = this->CTest->GetSubmitFiles()->begin();
   it != this->CTest->GetSubmitFiles()->end();
   ++ it )
    {
    files.insert(files.end(), *it);
    }

  if ( ofs )
    {
    ofs << "Upload files:" << std::endl;
    int cnt = 0;
    for ( it = files.begin(); it != files.end(); ++ it )
      {
      ofs << cnt << "\t" << it->c_str() << std::endl;
      cnt ++;
      }
    }
  cmCTestLog(this->CTest, HANDLER_OUTPUT, "Submit files (using "
    << this->CTest->GetCTestConfiguration("DropMethod") << ")"
    << std::endl);
  const char* specificTrack = this->CTest->GetSpecificTrack();
  if ( specificTrack )
    {
    cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Send to track: "
      << specificTrack << std::endl);
    }
  this->SetLogFile(&ofs);
  if ( this->CTest->GetCTestConfiguration("DropMethod") == "" ||
    this->CTest->GetCTestConfiguration("DropMethod") ==  "ftp" )
    {
    ofs << "Using drop method: FTP" << std::endl;
    cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Using FTP submit method"
      << std::endl
      << "   Drop site: ftp://");
    std::string url = "ftp://";
    url += cmCTest::MakeURLSafe(
      this->CTest->GetCTestConfiguration("DropSiteUser")) + ":" +
      cmCTest::MakeURLSafe(this->CTest->GetCTestConfiguration(
          "DropSitePassword")) + "@" +
      this->CTest->GetCTestConfiguration("DropSite") +
      cmCTest::MakeURLSafe(
        this->CTest->GetCTestConfiguration("DropLocation"));
    if ( this->CTest->GetCTestConfiguration("DropSiteUser").size() > 0 )
      {
      cmCTestLog(this->CTest, HANDLER_OUTPUT,
        this->CTest->GetCTestConfiguration(
          "DropSiteUser").c_str());
      if ( this->CTest->GetCTestConfiguration("DropSitePassword").size() > 0 )
        {
        cmCTestLog(this->CTest, HANDLER_OUTPUT, ":******");
        }
      cmCTestLog(this->CTest, HANDLER_OUTPUT, "@");
      }
    cmCTestLog(this->CTest, HANDLER_OUTPUT,
      this->CTest->GetCTestConfiguration("DropSite")
      << this->CTest->GetCTestConfiguration("DropLocation") << std::endl);
    if ( !this->SubmitUsingFTP(buildDirectory + "/Testing/"
        + this->CTest->GetCurrentTag(),
        files, prefix, url) )
      {
      cmCTestLog(this->CTest, ERROR_MESSAGE,
        "   Problems when submitting via FTP"
        << std::endl);
      ofs << "   Problems when submitting via FTP" << std::endl;
      return -1;
      }
    if(!this->CDash)
      {
      cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Using HTTP trigger method"
                 << std::endl
                 << "   Trigger site: "
                 << this->CTest->GetCTestConfiguration("TriggerSite")
                 << std::endl);
      if ( !this->
           TriggerUsingHTTP(files, prefix,
                            this->CTest->GetCTestConfiguration("TriggerSite")))
        {
        cmCTestLog(this->CTest, ERROR_MESSAGE,
                   "   Problems when triggering via HTTP" << std::endl);
        ofs << "   Problems when triggering via HTTP" << std::endl;
        return -1;
        }
      cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Submission successful"
                 << std::endl);
      ofs << "   Submission successful" << std::endl;
      return 0;
      }
    }
  else if ( this->CTest->GetCTestConfiguration("DropMethod") == "http" )
    {
    ofs << "Using drop method: HTTP" << std::endl;
    cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Using HTTP submit method"
      << std::endl
      << "   Drop site: http://");
    std::string url = "http://";
    if ( this->CTest->GetCTestConfiguration("DropSiteUser").size() > 0 )
      {
      url += this->CTest->GetCTestConfiguration("DropSiteUser");
      cmCTestLog(this->CTest, HANDLER_OUTPUT,
        this->CTest->GetCTestConfiguration("DropSiteUser").c_str());
      if ( this->CTest->GetCTestConfiguration("DropSitePassword").size() > 0 )
        {
        url += ":" + this->CTest->GetCTestConfiguration("DropSitePassword");
        cmCTestLog(this->CTest, HANDLER_OUTPUT, ":******");
        }
      url += "@";
      cmCTestLog(this->CTest, HANDLER_OUTPUT, "@");
      }
    url += this->CTest->GetCTestConfiguration("DropSite") +
      this->CTest->GetCTestConfiguration("DropLocation");
    cmCTestLog(this->CTest, HANDLER_OUTPUT,
      this->CTest->GetCTestConfiguration("DropSite")
      << this->CTest->GetCTestConfiguration("DropLocation") << std::endl);
    if ( !this->SubmitUsingHTTP(buildDirectory + "/Testing/" +
        this->CTest->GetCurrentTag(), files, prefix, url) )
      {
      cmCTestLog(this->CTest, ERROR_MESSAGE,
        "   Problems when submitting via HTTP" << std::endl);
      ofs << "   Problems when submitting via HTTP" << std::endl;
      return -1;
      }
    if(!this->CDash)
      {
      cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Using HTTP trigger method"
                 << std::endl
                 << "   Trigger site: "
                 << this->CTest->GetCTestConfiguration("TriggerSite")
                 << std::endl);
      if ( !this->
           TriggerUsingHTTP(files, prefix,
                            this->CTest->GetCTestConfiguration("TriggerSite")))
        {
        cmCTestLog(this->CTest, ERROR_MESSAGE,
                   "   Problems when triggering via HTTP" << std::endl);
        ofs << "   Problems when triggering via HTTP" << std::endl;
        return -1;
        }
      }
    cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Submission successful"
               << std::endl);
    ofs << "   Submission successful" << std::endl;
    return 0;
    }
  else if ( this->CTest->GetCTestConfiguration("DropMethod") == "xmlrpc" )
    {
    ofs << "Using drop method: XML-RPC" << std::endl;
    cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Using XML-RPC submit method"
      << std::endl);
    std::string url = this->CTest->GetCTestConfiguration("DropSite");
    prefix = this->CTest->GetCTestConfiguration("DropLocation");
    if ( !this->SubmitUsingXMLRPC(buildDirectory + "/Testing/" +
        this->CTest->GetCurrentTag(), files, prefix, url) )
      {
      cmCTestLog(this->CTest, ERROR_MESSAGE,
        "   Problems when submitting via XML-RPC" << std::endl);
      ofs << "   Problems when submitting via XML-RPC" << std::endl;
      return -1;
      }
    cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Submission successful"
      << std::endl);
    ofs << "   Submission successful" << std::endl;
    return 0;
    }
  else if ( this->CTest->GetCTestConfiguration("DropMethod") == "scp" )
    {
    std::string url;
    std::string oldWorkingDirectory;
    if ( this->CTest->GetCTestConfiguration("DropSiteUser").size() > 0 )
      {
      url += this->CTest->GetCTestConfiguration("DropSiteUser") + "@";
      }
    url += this->CTest->GetCTestConfiguration("DropSite") + ":" +
      this->CTest->GetCTestConfiguration("DropLocation");

    // change to the build directory so that we can uses a relative path
    // on windows since scp dosn't support "c:" a drive in the path
    oldWorkingDirectory = cmSystemTools::GetCurrentWorkingDirectory();
    cmSystemTools::ChangeDirectory(buildDirectory.c_str());

    if ( !this->SubmitUsingSCP(
        this->CTest->GetCTestConfiguration("ScpCommand"),
        "Testing/"+this->CTest->GetCurrentTag(), files, prefix, url) )
      {
      cmSystemTools::ChangeDirectory(oldWorkingDirectory.c_str());
      cmCTestLog(this->CTest, ERROR_MESSAGE,
        "   Problems when submitting via SCP"
        << std::endl);
      ofs << "   Problems when submitting via SCP" << std::endl;
      return -1;
      }
    cmSystemTools::ChangeDirectory(oldWorkingDirectory.c_str());
    cmCTestLog(this->CTest, HANDLER_OUTPUT, "   Submission successful"
      << std::endl);
    ofs << "   Submission successful" << std::endl;
    return 0;
    }

  cmCTestLog(this->CTest, ERROR_MESSAGE, "   Unknown submission method: \""
    << this->CTest->GetCTestConfiguration("DropMethod") << "\"" << std::endl);
  return -1;
}

//----------------------------------------------------------------------------
std::string cmCTestSubmitHandler::GetSubmitResultsPrefix()
{
  std::string name = this->CTest->GetCTestConfiguration("Site") +
    "___" + this->CTest->GetCTestConfiguration("BuildName") +
    "___" + this->CTest->GetCurrentTag() + "-" +
    this->CTest->GetTestModelString() + "___XML___";
  return name;
}


