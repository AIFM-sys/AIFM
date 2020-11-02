/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCTestUpdateHandler.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc. All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#ifndef cmCTestUpdateHandler_h
#define cmCTestUpdateHandler_h


#include "cmCTestGenericHandler.h"
#include "cmListFileCache.h"

#if defined(__sgi) && !defined(__GNUC__)
# pragma set woff 1375 /* base class destructor not virtual */
#endif

/** \class cmCTestUpdateHandler
 * \brief A class that handles ctest -S invocations
 *
 */
class cmCTestUpdateHandler : public cmCTestGenericHandler
{
public:
  cmTypeMacro(cmCTestUpdateHandler, cmCTestGenericHandler);

  /*
   * The main entry point for this class
   */
  int ProcessHandler();

  cmCTestUpdateHandler();

  enum {
    e_UNKNOWN = 0,
    e_CVS,
    e_SVN,
    e_LAST
  };

  /**
   * Initialize handler
   */
  virtual void Initialize();

private:
  // Some structures needed for update
  struct StringPair :
    public std::pair<std::string, std::string>{};
  struct UpdateFiles : public std::vector<StringPair>{};
  struct AuthorsToUpdatesMap :
    public std::map<std::string, UpdateFiles>{};

  // Determine the type of version control
  int DetermineType(const char* cmd, const char* type);
};

#if defined(__sgi) && !defined(__GNUC__)
# pragma reset woff 1375 /* base class destructor not virtual */
#endif

#endif
