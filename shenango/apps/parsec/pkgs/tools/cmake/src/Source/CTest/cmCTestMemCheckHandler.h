/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCTestMemCheckHandler.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc. All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#ifndef cmCTestMemCheckHandler_h
#define cmCTestMemCheckHandler_h


#include "cmCTestTestHandler.h"
#include "cmListFileCache.h"

class cmMakefile;

/** \class cmCTestMemCheckHandler
 * \brief A class that handles ctest -S invocations
 *
 */
class cmCTestMemCheckHandler : public cmCTestTestHandler
{
public:
  cmTypeMacro(cmCTestMemCheckHandler, cmCTestTestHandler);

  void PopulateCustomVectors(cmMakefile *mf);
  
  cmCTestMemCheckHandler();

  void Initialize();
protected:
  virtual int PreProcessHandler();
  virtual int PostProcessHandler();
  virtual void GenerateTestCommand(std::vector<const char*>& args);

private:

  enum { // Memory checkers
    UNKNOWN = 0,
    VALGRIND,
    PURIFY,
    BOUNDS_CHECKER
  };
public:
  enum { // Memory faults
    ABR = 0,
    ABW,
    ABWL,
    COR,
    EXU,
    FFM,
    FIM,
    FMM,
    FMR,
    FMW,
    FUM,
    IPR,
    IPW,
    MAF,
    MLK,
    MPK,
    NPR,
    ODS,
    PAR,
    PLK,
    UMC,
    UMR,
    NO_MEMORY_FAULT
  };
private:
  enum { // Program statuses
    NOT_RUN = 0,
    TIMEOUT,
    SEGFAULT,
    ILLEGAL,
    INTERRUPT,
    NUMERICAL,
    OTHER_FAULT,
    FAILED,
    BAD_COMMAND,
    COMPLETED
  };
  std::string              BoundsCheckerDPBDFile;
  std::string              BoundsCheckerXMLFile;
  std::string              MemoryTester;
  std::vector<cmStdString> MemoryTesterOptionsParsed;
  std::string              MemoryTesterOptions;
  int                      MemoryTesterStyle;
  std::string              MemoryTesterOutputFile;
  int                      MemoryTesterGlobalResults[NO_MEMORY_FAULT];

  ///! Initialize memory checking subsystem.
  bool InitializeMemoryChecking();

  /**
   * Generate the Dart compatible output
   */
  void GenerateDartOutput(std::ostream& os);

  std::vector<cmStdString> CustomPreMemCheck;
  std::vector<cmStdString> CustomPostMemCheck;

  //! Parse Valgrind/Purify/Bounds Checker result out of the output
  //string. After running, log holds the output and results hold the
  //different memmory errors.
  bool ProcessMemCheckOutput(const std::string& str, 
                             std::string& log, int* results);
  bool ProcessMemCheckValgrindOutput(const std::string& str, 
                                     std::string& log, int* results);
  bool ProcessMemCheckPurifyOutput(const std::string& str, 
                                   std::string& log, int* results);
  bool ProcessMemCheckBoundsCheckerOutput(const std::string& str, 
                                          std::string& log, int* results);
  /**
   *  Run one test
   */
  virtual void ProcessOneTest(cmCTestTestProperties *props,
                              std::vector<cmStdString> &passed,
                              std::vector<cmStdString> &failed,
                              int count, int tmsize);
  void PostProcessPurifyTest(cmCTestTestResult& res);
  void PostProcessBoundsCheckerTest(cmCTestTestResult& res);
};

#endif

