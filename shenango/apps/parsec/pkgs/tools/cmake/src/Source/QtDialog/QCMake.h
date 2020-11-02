/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: QCMake.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#ifndef __QCMake_h
#define __QCMake_h
#ifdef _MSC_VER
#pragma warning ( disable : 4127 )
#pragma warning ( disable : 4512 )
#endif

#include <QObject>
#include <QString>
#include <QVariant>
#include <QList>
#include <QStringList>
#include <QMetaType>

class cmake;

/// struct to represent cmake properties in Qt
/// Value is of type String or Bool
struct QCMakeProperty
{
  enum PropertyType { BOOL, PATH, FILEPATH, STRING };
  QString Key;
  QVariant Value;
  QString Help;
  PropertyType Type;
  bool Advanced;
  bool operator==(const QCMakeProperty& other) const 
    { 
    return this->Key == other.Key;
    }
  bool operator<(const QCMakeProperty& other) const 
    { 
    return this->Key < other.Key;
    }
};

// list of properties
typedef QList<QCMakeProperty> QCMakePropertyList;

// allow QVariant to be a property or list of properties
Q_DECLARE_METATYPE(QCMakeProperty)
Q_DECLARE_METATYPE(QCMakePropertyList)

/// Qt API for CMake library.
/// Wrapper like class allows for easier integration with 
/// Qt features such as, signal/slot connections, multi-threading, etc..
class QCMake : public QObject
{
  Q_OBJECT
public:
  QCMake(QObject* p=0);
  ~QCMake();
public slots:
  /// load the cache file in a directory
  void loadCache(const QString& dir);
  /// set the source directory containing the source
  void setSourceDirectory(const QString& dir);
  /// set the binary directory to build in
  void setBinaryDirectory(const QString& dir);
  /// set the desired generator to use
  void setGenerator(const QString& generator);
  /// do the configure step
  void configure();
  /// generate the files
  void generate();
  /// set the property values
  void setProperties(const QCMakePropertyList&);
  /// interrupt the configure or generate process
  void interrupt();
  /// delete the cache in binary directory
  void deleteCache();
  /// reload the cache in binary directory
  void reloadCache();
  /// set whether to do debug output
  void setDebugOutput(bool);
  /// set whether to do suppress dev warnings
  void setSuppressDevWarnings(bool value);

public:
  /// get the list of cache properties
  QCMakePropertyList properties() const;
  /// get the current binary directory
  QString binaryDirectory() const;
  /// get the current source directory
  QString sourceDirectory() const;
  /// get the current generator
  QString generator() const;
  /// get the available generators
  QStringList availableGenerators() const;
  /// get whether to do debug output
  bool getDebugOutput() const;

signals:
  /// signal when properties change (during read from disk or configure process)
  void propertiesChanged(const QCMakePropertyList& vars);
  /// signal when the generator changes
  void generatorChanged(const QString& gen);
  /// signal when the source directory changes (binary directory already
  /// containing a CMakeCache.txt file)
  void sourceDirChanged(const QString& dir);
  /// signal when the binary directory changes
  void binaryDirChanged(const QString& dir);
  /// signal for progress events
  void progressChanged(const QString& msg, float percent);
  /// signal when configure is done
  void configureDone(int error);
  /// signal when generate is done
  void generateDone(int error);
  /// signal when there is an output message
  void outputMessage(const QString& msg);
  /// signal when there is an error message
  void errorMessage(const QString& msg);
  /// signal when debug output changes
  void debugOutputChanged(bool);

protected:
  cmake* CMakeInstance;

  static void progressCallback(const char* msg, float percent, void* cd);
  static void errorCallback(const char* msg, const char* title, 
                            bool&, void* cd);
  bool SuppressDevWarnings;
  QString SourceDirectory;
  QString BinaryDirectory;
  QString Generator;
  QStringList AvailableGenerators;
  QString CMakeExecutable;
};

#endif // __QCMake_h

