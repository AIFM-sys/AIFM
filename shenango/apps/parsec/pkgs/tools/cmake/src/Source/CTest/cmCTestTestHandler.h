/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmCTestTestHandler.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc. All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#ifndef cmCTestTestHandler_h
#define cmCTestTestHandler_h


#include "cmCTestGenericHandler.h"
#include <cmsys/RegularExpression.hxx>

class cmMakefile;

/** \class cmCTestTestHandler
 * \brief A class that handles ctest -S invocations
 *
 */
class cmCTestTestHandler : public cmCTestGenericHandler
{
public:
  cmTypeMacro(cmCTestTestHandler, cmCTestGenericHandler);

  /**
   * The main entry point for this class
   */
  int ProcessHandler();

  /**
   * When both -R and -I are used should te resulting test list be the
   * intersection or the union of the lists. By default it is the
   * intersection.
   */
  void SetUseUnion(bool val) { this->UseUnion = val; }

  /**
   * This method is called when reading CTest custom file
   */
  void PopulateCustomVectors(cmMakefile *mf);

  ///! Control the use of the regular expresisons, call these methods to turn
  ///them on
  void UseIncludeRegExp();
  void UseExcludeRegExp();
  void SetIncludeRegExp(const char *);
  void SetExcludeRegExp(const char *);


  ///! pass the -I argument down
  void SetTestsToRunInformation(const char*);

  cmCTestTestHandler();

  /*
   * Add the test to the list of tests to be executed
   */
  bool AddTest(const std::vector<std::string>& args);

  /*
   * Set tests properties
   */
  bool SetTestsProperties(const std::vector<std::string>& args);

  void Initialize();

  struct cmCTestTestProperties
  {
    cmStdString Name;
    cmStdString Directory;
    std::vector<std::string> Args;
    std::vector<std::pair<cmsys::RegularExpression,
                          std::string> > ErrorRegularExpressions;
    std::vector<std::pair<cmsys::RegularExpression,
                          std::string> > RequiredRegularExpressions;
    std::map<cmStdString, cmStdString> Measurements;
    bool IsInBasedOnREOptions;
    bool WillFail;
    double Timeout;
  };

  struct cmCTestTestResult
  {
    std::string Name;
    std::string Path;
    std::string FullCommandLine;
    double      ExecutionTime;
    int         ReturnValue;
    int         Status;
    std::string CompletionStatus;
    std::string Output;
    std::string RegressionImages;
    int         TestCount;
    cmCTestTestProperties* Properties;
  };

  // add configuraitons to a search path for an executable
  static void AddConfigurations(cmCTest *ctest, 
                                std::vector<std::string> &attempted,
                                std::vector<std::string> &attemptedConfigs,
                                std::string filepath,
                                std::string &filename);

  // full signature static method to find an executable
  static std::string FindExecutable(cmCTest *ctest,
                                    const char *testCommand,
                                    std::string &resultingConfig,
                                    std::vector<std::string> &extraPaths,
                                    std::vector<std::string> &failed);

protected:
  virtual int PreProcessHandler();
  virtual int PostProcessHandler();
  virtual void GenerateTestCommand(std::vector<const char*>& args);
  int ExecuteCommands(std::vector<cmStdString>& vec);

  //! Clean test output to specified length
  bool CleanTestOutput(std::string& output, size_t length);

  double                  ElapsedTestingTime;

  typedef std::vector<cmCTestTestResult> TestResultsVector;
  TestResultsVector    TestResults;

  std::vector<cmStdString> CustomTestsIgnore;
  std::string             StartTest;
  std::string             EndTest;
  unsigned int            StartTestTime;
  unsigned int            EndTestTime;
  bool MemCheck;
  int CustomMaximumPassedTestOutputSize;
  int CustomMaximumFailedTestOutputSize;
protected:
  /**
   *  Run one test
   */
  virtual void ProcessOneTest(cmCTestTestProperties *props,
                              std::vector<cmStdString> &passed,
                              std::vector<cmStdString> &failed,
                              int count, int tmsize);



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


  /**
   * Generate the Dart compatible output
   */
  virtual void GenerateDartOutput(std::ostream& os);

  /**
   * Run the tests for a directory and any subdirectories
   */
  void ProcessDirectory(std::vector<cmStdString> &passed,
                        std::vector<cmStdString> &failed);

  typedef std::vector<cmCTestTestProperties> ListOfTests;
  /**
   * Get the list of tests in directory and subdirectories.
   */
  void GetListOfTests();

  /**
   * Find the executable for a test
   */
  std::string FindTheExecutable(const char *exe);

  const char* GetTestStatus(int status);
  void ExpandTestsToRunInformation(int numPossibleTests);

  std::vector<cmStdString> CustomPreTest;
  std::vector<cmStdString> CustomPostTest;

  std::vector<int>        TestsToRun;

  bool UseIncludeRegExpFlag;
  bool UseExcludeRegExpFlag;
  bool UseExcludeRegExpFirst;
  std::string IncludeRegExp;
  std::string ExcludeRegExp;
  cmsys::RegularExpression IncludeTestsRegularExpression;
  cmsys::RegularExpression ExcludeTestsRegularExpression;

  std::string GenerateRegressionImages(const std::string& xml);

  std::string TestsToRunString;
  bool UseUnion;
  ListOfTests TestList;
  cmsys::RegularExpression DartStuff;

  std::ostream* LogFile;
};

#endif
