/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: cmLocalGenerator.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef cmLocalGenerator_h
#define cmLocalGenerator_h

#include "cmStandardIncludes.h"

class cmMakefile;
class cmGlobalGenerator;
class cmTarget;
class cmTargetManifest;
class cmSourceFile;
class cmCustomCommand;

/** \class cmLocalGenerator
 * \brief Create required build files for a directory.
 *
 * Subclasses of this abstract class generate makefiles, DSP, etc for various
 * platforms. This class should never be constructued directly. A
 * GlobalGenerator will create it and invoke the appropriate commands on it.
 */
class cmLocalGenerator
{
public:
  cmLocalGenerator();
  virtual ~cmLocalGenerator();
  
  /**
   * Generate the makefile for this directory. 
   */
  virtual void Generate() {}

  /**
   * Process the CMakeLists files for this directory to fill in the
   * Makefile ivar 
   */
  virtual void Configure();

  /** 
   * Calls TraceVSDependencies() on all targets of this generator.
   */
  virtual void TraceDependencies();

  virtual void AddHelperCommands() {}

  /**
   * Perform any final calculations prior to generation
   */
  virtual void ConfigureFinalPass();

  /**
   * Generate the install rules files in this directory.
   */
  virtual void GenerateInstallRules();

  /**
   * Generate the test files for tests.
   */
  virtual void GenerateTestFiles();

  /**
   * Generate a manifest of target files that will be built.
   */
  virtual void GenerateTargetManifest();

  ///! Get the makefile for this generator
  cmMakefile *GetMakefile() {
    return this->Makefile; };
  
  ///! Get the makefile for this generator, const version
    const cmMakefile *GetMakefile() const {
      return this->Makefile; };
  
  ///! Get the GlobalGenerator this is associated with
  cmGlobalGenerator *GetGlobalGenerator() {
    return this->GlobalGenerator; };

  ///! Set the Global Generator, done on creation by the GlobalGenerator
  void SetGlobalGenerator(cmGlobalGenerator *gg);

  /** 
   * Convert something to something else. This is a centralized coversion
   * routine used by the generators to handle relative paths and the like.
   * The flags determine what is actually done. 
   *
   * relative: treat the argument as a directory and convert it to make it
   * relative or full or unchanged. If relative (HOME, START etc) then that
   * specifies what it should be relative to.
   *
   * output: make the result suitable for output to a...
   *
   * optional: should any relative path operation be controlled by the rel
   * path setting
   */
  enum RelativeRoot { NONE, FULL, HOME, START, HOME_OUTPUT, START_OUTPUT };
  enum OutputFormat { UNCHANGED, MAKEFILE, SHELL };
  std::string Convert(const char* source, 
                      RelativeRoot relative, 
                      OutputFormat output = UNCHANGED,
                      bool optional = false);
  
  /**
   * Convert the given path to an output path that is optionally
   * relative based on the cache option CMAKE_USE_RELATIVE_PATHS.  The
   * remote path must use forward slashes and not already be escaped
   * or quoted.
   */
  std::string ConvertToOptionallyRelativeOutputPath(const char* remote);

  ///! set/get the parent generator 
  cmLocalGenerator* GetParent(){return this->Parent;}
  void SetParent(cmLocalGenerator* g) { this->Parent = g; g->AddChild(this); }

  ///! set/get the children
  void AddChild(cmLocalGenerator* g) { this->Children.push_back(g); }
  std::vector<cmLocalGenerator*>& GetChildren() { return this->Children; };
    

  void AddLanguageFlags(std::string& flags, const char* lang,
                        const char* config);
  void AddSharedFlags(std::string& flags, const char* lang, bool shared);
  void AddConfigVariableFlags(std::string& flags, const char* var,
                              const char* config);
  virtual void AppendFlags(std::string& flags, const char* newFlags);
  ///! Get the include flags for the current makefile and language
  const char* GetIncludeFlags(const char* lang); 

  /**
   * Encode a list of preprocessor definitions for the compiler
   * command line.
   */
  void AppendDefines(std::string& defines, const char* defines_list,
                     const char* lang);

  /** Translate a dependency as given in CMake code to the name to
      appear in a generated build file.  If the given name is that of
      a CMake target it will be transformed to the real output
      location of that target for the given configuration.  If the
      given name is the full path to a file it will be returned.
      Otherwise the name is treated as a relative path with respect to
      the source directory of this generator.  This should only be
      used for dependencies of custom commands.  */
  std::string GetRealDependency(const char* name, const char* config);
  
  /** Translate a command as given in CMake code to the location of the 
      executable if the command is the name of a CMake executable target.
      If that's not the case, just return the original name. */
  std::string GetRealLocation(const char* inName, const char* config);

  ///! for existing files convert to output path and short path if spaces
  std::string ConvertToOutputForExisting(const char* p);
  
  /** Called from command-line hook to clear dependencies.  */
  virtual void ClearDependencies(cmMakefile* /* mf */, 
                                 bool /* verbose */) {}
  
  /** Called from command-line hook to update dependencies.  */
  virtual bool UpdateDependencies(const char* /* tgtInfo */,
                                  bool /*verbose*/,
                                  bool /*color*/)
    { return true; }

  /** Get the include flags for the current makefile and language.  */
  void GetIncludeDirectories(std::vector<std::string>& dirs,
                             bool filter_system_dirs = true);

  /** Compute the language used to compile the given source file.  */
  const char* GetSourceFileLanguage(const cmSourceFile& source);

  // Create a struct to hold the varibles passed into
  // ExpandRuleVariables
  struct RuleVariables
  {
    RuleVariables()
      {
        memset(this, 0,  sizeof(*this));
      }
    const char* TargetPDB;
    const char* TargetVersionMajor;
    const char* TargetVersionMinor;
    const char* Language;
    const char* Objects;
    const char* Target;
    const char* LinkLibraries;
    const char* Source;
    const char* AssemblySource;
    const char* PreprocessedSource;
    const char* Object;
    const char* ObjectDir;
    const char* Flags;
    const char* ObjectsQuoted;
    const char* TargetSOName;
    const char* TargetInstallNameDir;
    const char* LinkFlags;
    const char* LanguageCompileFlags;
    const char* Defines;
  };

  /** Set whether to treat conversions to SHELL as a link script shell.  */
  void SetLinkScriptShell(bool b) { this->LinkScriptShell = b; }

  /** Escape the given string to be used as a command line argument in
      the native build system shell.  Optionally allow the build
      system to replace make variable references.  Optionally adjust
      escapes for the special case of passing to the native echo
      command.  */
  std::string EscapeForShell(const char* str, bool makeVars = false,
                             bool forEcho = false);

  /** Backwards-compatibility version of EscapeForShell.  */
  std::string EscapeForShellOldStyle(const char* str);

  /** Escape the given string as an argument in a CMake script.  */
  std::string EscapeForCMake(const char* str);

  /** Return the directories into which object files will be put.
   *  There maybe more than one for fat binary systems like OSX.
   */
  virtual void 
  GetTargetObjectFileDirectories(cmTarget* target,
                                 std::vector<std::string>& 
                                 dirs);
  
  /**
   * Convert the given remote path to a relative path with respect to
   * the given local path.  The local path must be given in component
   * form (see SystemTools::SplitPath) without a trailing slash.  The
   * remote path must use forward slashes and not already be escaped
   * or quoted.
   */
  std::string ConvertToRelativePath(const std::vector<std::string>& local,
                                    const char* remote);

  /**
   * Get the relative path from the generator output directory to a
   * per-target support directory.
   */
  virtual std::string GetTargetDirectory(cmTarget const& target) const;

  /**
   * Get the level of backwards compatibility requested by the project
   * in this directory.  This is the value of the CMake variable
   * CMAKE_BACKWARDS_COMPATIBILITY whose format is
   * "major.minor[.patch]".  The returned integer is encoded as
   *
   *   CMake_VERSION_ENCODE(major, minor, patch)
   *
   * and is monotonically increasing with the CMake version.
   */
  unsigned int GetBackwardsCompatibility();

  /**
   * Test whether compatibility is set to a given version or lower.
   */
  bool NeedBackwardsCompatibility(unsigned int major,
                                  unsigned int minor,
                                  unsigned int patch = 0xFFu);

  /**
   * Generate a Mac OS X application bundle Info.plist file.
   */
  void GenerateAppleInfoPList(cmTarget* target, const char* targetName,
                              const char* fname);
protected:
  /** Construct a comment for a custom command.  */
  std::string ConstructComment(const cmCustomCommand& cc,
                               const char* default_comment = "");

  /** Fill out these strings for the given target.  Libraries to link,
   *  flags, and linkflags. */
  void GetTargetFlags(std::string& linkLibs, 
                      std::string& flags,
                      std::string& linkFlags,
                      cmTarget&target);
  
  ///! put all the libraries for a target on into the given stream
  virtual void OutputLinkLibraries(std::ostream&, cmTarget&, bool relink);
  
  // Expand rule variables in CMake of the type found in language rules
  void ExpandRuleVariables(std::string& string,
                           const RuleVariables& replaceValues);
  // Expand rule variables in a single string
  std::string ExpandRuleVariable(std::string const& variable,
                                 const RuleVariables& replaceValues);
  
  /** Convert a target to a utility target for unsupported 
   *  languages of a generator */
  void AddBuildTargetRule(const char* llang, cmTarget& target);
  ///! add a custom command to build a .o file that is part of a target 
  void AddCustomCommandToCreateObject(const char* ofname, 
                                      const char* lang, 
                                      cmSourceFile& source,
                                      cmTarget& target);
  // Create Custom Targets and commands for unsupported languages
  // The set passed in should contain the languages supported by the
  // generator directly.  Any targets containing files that are not
  // of the types listed will be compiled as custom commands and added
  // to a custom target.
  void CreateCustomTargetsAndCommands(std::set<cmStdString> const&);

  // Handle old-style install rules stored in the targets.
  void GenerateTargetInstallRules(
    std::ostream& os, const char* config,
    std::vector<std::string> const& configurationTypes);

  // Compute object file names.
  std::string GetObjectFileNameWithoutTarget(const cmSourceFile& source,
                                             std::string::size_type dir_len,
                                             bool* hasSourceExtension = 0);
  std::string& CreateSafeUniqueObjectFileName(const char* sin,
                                              std::string::size_type dir_len);

  void ConfigureRelativePaths();
  std::string FindRelativePathTopSource();
  std::string FindRelativePathTopBinary();
  void SetupPathConversions();

  std::string ConvertToLinkReference(std::string const& lib);

  /** Check whether the native build system supports the given
      definition.  Issues a warning.  */
  virtual bool CheckDefinition(std::string const& define) const;

  cmMakefile *Makefile;
  cmGlobalGenerator *GlobalGenerator;
  // members used for relative path function ConvertToMakefilePath
  std::string RelativePathToSourceDir;
  std::string RelativePathToBinaryDir;
  std::vector<std::string> HomeDirectoryComponents;
  std::vector<std::string> StartDirectoryComponents;
  std::vector<std::string> HomeOutputDirectoryComponents;
  std::vector<std::string> StartOutputDirectoryComponents;
  cmLocalGenerator* Parent;
  std::vector<cmLocalGenerator*> Children;
  std::map<cmStdString, cmStdString> LanguageToIncludeFlags;
  std::map<cmStdString, cmStdString> UniqueObjectNamesMap;
  bool WindowsShell;
  bool WindowsVSIDE;
  bool WatcomWMake;
  bool MinGWMake;
  bool NMake;
  bool ForceUnixPath;
  bool MSYSShell;
  bool LinkScriptShell;
  bool UseRelativePaths;
  bool IgnoreLibPrefix;
  bool Configured;
  bool EmitUniversalBinaryFlags;
  // A type flag is not nice. It's used only in TraceDependencies().
  bool IsMakefileGenerator;
  // Hack for ExpandRuleVariable until object-oriented version is
  // committed.
  std::string TargetImplib;

  // The top-most directories for relative path conversion.  Both the
  // source and destination location of a relative path conversion
  // must be underneath one of these directories (both under source or
  // both under binary) in order for the relative path to be evaluated
  // safely by the build tools.
  std::string RelativePathTopSource;
  std::string RelativePathTopBinary;
  bool RelativePathsConfigured;
  bool PathConversionsSetup;

  unsigned int BackwardsCompatibility;
  bool BackwardsCompatibilityFinal;
};

#endif
