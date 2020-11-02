/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: QCMake.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include "QCMake.h"

#include <QDir>
#include <QCoreApplication>

#include "cmake.h"
#include "cmCacheManager.h"
#include "cmSystemTools.h"
#include "cmExternalMakefileProjectGenerator.h"

QCMake::QCMake(QObject* p)
  : QObject(p)
{
  this->SuppressDevWarnings = false;
  qRegisterMetaType<QCMakeProperty>();
  qRegisterMetaType<QCMakePropertyList>();
  
  QDir execDir(QCoreApplication::applicationDirPath());
  
#if defined(Q_OS_MAC)
  if(execDir.exists("../bin/cmake"))
    {
    execDir.cd("../bin");
    }
  else
    {
    execDir.cd("../../../");  // path to cmake in build directory (need to fix for deployment)
    }
#endif
  
  QString cmakeCommand = QString("cmake")+cmSystemTools::GetExecutableExtension();
  cmakeCommand = execDir.filePath(cmakeCommand);

  cmSystemTools::DisableRunCommandOutput();
  cmSystemTools::SetRunCommandHideConsole(true);
  cmSystemTools::SetErrorCallback(QCMake::errorCallback, this);
  cmSystemTools::FindExecutableDirectory(cmakeCommand.toAscii().data());

  this->CMakeInstance = new cmake;
  this->CMakeInstance->SetCMakeCommand(cmakeCommand.toAscii().data());
#if defined(Q_OS_MAC)
  this->CMakeInstance->SetCMakeEditCommand("cmake-gui.app/Contents/MacOS/cmake-gui");
#else  
  this->CMakeInstance->SetCMakeEditCommand("cmake-gui");
#endif
  this->CMakeInstance->SetProgressCallback(QCMake::progressCallback, this);

  std::vector<std::string> generators;
  this->CMakeInstance->GetRegisteredGenerators(generators);
  std::vector<std::string>::iterator iter;
  for(iter = generators.begin(); iter != generators.end(); ++iter)
    {
    this->AvailableGenerators.append(iter->c_str());
    }
}

QCMake::~QCMake()
{
  delete this->CMakeInstance;
  //cmDynamicLoader::FlushCache();
}

void QCMake::loadCache(const QString& dir)
{
  this->setBinaryDirectory(dir);
}

void QCMake::setSourceDirectory(const QString& dir)
{
  if(this->SourceDirectory != dir)
    {
    this->SourceDirectory = QDir::fromNativeSeparators(dir);
    emit this->sourceDirChanged(this->SourceDirectory);
    }
}

void QCMake::setBinaryDirectory(const QString& dir)
{
  if(this->BinaryDirectory != dir)
    {
    this->BinaryDirectory = QDir::fromNativeSeparators(dir);
    emit this->binaryDirChanged(this->BinaryDirectory);
    cmCacheManager *cachem = this->CMakeInstance->GetCacheManager();
    this->setGenerator(QString());
    if(!this->CMakeInstance->GetCacheManager()->LoadCache(
      this->BinaryDirectory.toLocal8Bit().data()))
      {
      QDir testDir(this->BinaryDirectory);
      if(testDir.exists("CMakeCache.txt"))
        {
        cmSystemTools::Error("There is a CMakeCache.txt file for the current binary "
            "tree but cmake does not have permission to read it.  "
            "Please check the permissions of the directory you are trying to run CMake on.");
        }
      }
    
    QCMakePropertyList props = this->properties();
    emit this->propertiesChanged(props);
    cmCacheManager::CacheIterator itm = cachem->NewIterator();
    if ( itm.Find("CMAKE_HOME_DIRECTORY"))
      {
      setSourceDirectory(itm.GetValue());
      }
    if ( itm.Find("CMAKE_GENERATOR"))
      {
      const char* extraGen = cachem->GetCacheValue("CMAKE_EXTRA_GENERATOR");
      std::string curGen = cmExternalMakefileProjectGenerator::
                              CreateFullGeneratorName(itm.GetValue(), extraGen);
      this->setGenerator(curGen.c_str());
      }
    }
}


void QCMake::setGenerator(const QString& gen)
{
  if(this->Generator != gen)
    {
    this->Generator = gen;
    emit this->generatorChanged(this->Generator);
    }
}

void QCMake::configure()
{
  this->CMakeInstance->SetHomeDirectory(this->SourceDirectory.toAscii().data());
  this->CMakeInstance->SetStartDirectory(this->SourceDirectory.toAscii().data());
  this->CMakeInstance->SetHomeOutputDirectory(this->BinaryDirectory.toAscii().data());
  this->CMakeInstance->SetStartOutputDirectory(this->BinaryDirectory.toAscii().data());
  this->CMakeInstance->SetGlobalGenerator(
    this->CMakeInstance->CreateGlobalGenerator(this->Generator.toAscii().data()));
  this->CMakeInstance->LoadCache();
  this->CMakeInstance->SetSuppressDevWarnings(this->SuppressDevWarnings);
  this->CMakeInstance->PreLoadCMakeFiles();

  cmSystemTools::ResetErrorOccuredFlag();

  int err = this->CMakeInstance->Configure();

  emit this->propertiesChanged(this->properties());
  emit this->configureDone(err);
}

void QCMake::generate()
{
  cmSystemTools::ResetErrorOccuredFlag();
  int err = this->CMakeInstance->Generate();
  emit this->generateDone(err);
}
  
void QCMake::setProperties(const QCMakePropertyList& newProps)
{
  QCMakePropertyList props = newProps;

  QStringList toremove;

  // set the value of properties
  cmCacheManager *cachem = this->CMakeInstance->GetCacheManager();
  for(cmCacheManager::CacheIterator i = cachem->NewIterator();
      !i.IsAtEnd(); i.Next())
    {

    if(i.GetType() == cmCacheManager::INTERNAL ||
       i.GetType() == cmCacheManager::STATIC)
      {
      continue;
      }

    QCMakeProperty prop;
    prop.Key = i.GetName();
    int idx = props.indexOf(prop);
    if(idx == -1)
      {
      toremove.append(i.GetName());
      }
    else
      {
      prop = props[idx];
      if(prop.Value.type() == QVariant::Bool)
        {
        i.SetValue(prop.Value.toBool() ? "ON" : "OFF");
        }
      else
        {
        i.SetValue(prop.Value.toString().toAscii().data());
        }
      props.removeAt(idx);
      }

    }

  // remove some properites
  foreach(QString s, toremove)
    {
    cachem->RemoveCacheEntry(s.toAscii().data());
    }
  
  // add some new properites
  foreach(QCMakeProperty s, props)
    {
    if(s.Type == QCMakeProperty::BOOL)
      {
      this->CMakeInstance->AddCacheEntry(s.Key.toAscii().data(),
                            s.Value.toBool() ? "ON" : "OFF",
                            s.Help.toAscii().data(),
                            cmCacheManager::BOOL);
      }
    else if(s.Type == QCMakeProperty::STRING)
      {
      this->CMakeInstance->AddCacheEntry(s.Key.toAscii().data(),
                            s.Value.toString().toAscii().data(),
                            s.Help.toAscii().data(),
                            cmCacheManager::STRING);
      }
    else if(s.Type == QCMakeProperty::PATH)
      {
      this->CMakeInstance->AddCacheEntry(s.Key.toAscii().data(),
                            s.Value.toString().toAscii().data(),
                            s.Help.toAscii().data(),
                            cmCacheManager::PATH);
      }
    else if(s.Type == QCMakeProperty::FILEPATH)
      {
      this->CMakeInstance->AddCacheEntry(s.Key.toAscii().data(),
                            s.Value.toString().toAscii().data(),
                            s.Help.toAscii().data(),
                            cmCacheManager::FILEPATH);
      }
    }
  
  cachem->SaveCache(this->BinaryDirectory.toAscii().data());
}

QCMakePropertyList QCMake::properties() const
{
  QCMakePropertyList ret;

  cmCacheManager *cachem = this->CMakeInstance->GetCacheManager();
  for(cmCacheManager::CacheIterator i = cachem->NewIterator();
      !i.IsAtEnd(); i.Next())
    {

    if(i.GetType() == cmCacheManager::INTERNAL ||
       i.GetType() == cmCacheManager::STATIC ||
       i.GetType() == cmCacheManager::UNINITIALIZED)
      {
      continue;
      }

    QCMakeProperty prop;
    prop.Key = i.GetName();
    prop.Help = i.GetProperty("HELPSTRING");
    prop.Value = i.GetValue();
    prop.Advanced = i.GetPropertyAsBool("ADVANCED");

    if(i.GetType() == cmCacheManager::BOOL)
      {
      prop.Type = QCMakeProperty::BOOL;
      prop.Value = cmSystemTools::IsOn(i.GetValue());
      }
    else if(i.GetType() == cmCacheManager::PATH)
      {
      prop.Type = QCMakeProperty::PATH;
      }
    else if(i.GetType() == cmCacheManager::FILEPATH)
      {
      prop.Type = QCMakeProperty::FILEPATH;
      }
    else if(i.GetType() == cmCacheManager::STRING)
      {
      prop.Type = QCMakeProperty::STRING;
      }

    ret.append(prop);
    }

  return ret;
}
  
void QCMake::interrupt()
{
  cmSystemTools::SetFatalErrorOccured();
}

void QCMake::progressCallback(const char* msg, float percent, void* cd)
{
  QCMake* self = reinterpret_cast<QCMake*>(cd);
  if(percent >= 0)
    {
    emit self->progressChanged(msg, percent);
    }
  else
    {
    emit self->outputMessage(msg);
    }
  QCoreApplication::processEvents();
}

void QCMake::errorCallback(const char* msg, const char* /*title*/,
                           bool& /*stop*/, void* cd)
{
  QCMake* self = reinterpret_cast<QCMake*>(cd);
  emit self->errorMessage(msg);
  QCoreApplication::processEvents();
}

QString QCMake::binaryDirectory() const
{
  return this->BinaryDirectory;
}

QString QCMake::sourceDirectory() const
{
  return this->SourceDirectory;
}

QString QCMake::generator() const
{
  return this->Generator;
}

QStringList QCMake::availableGenerators() const
{
  return this->AvailableGenerators;
}

void QCMake::deleteCache()
{
  // delete cache
  this->CMakeInstance->GetCacheManager()->DeleteCache(this->BinaryDirectory.toAscii().data());
  // reload to make our cache empty
  this->CMakeInstance->GetCacheManager()->LoadCache(this->BinaryDirectory.toAscii().data());
  // emit no generator and no properties
  this->setGenerator(QString());
  QCMakePropertyList props = this->properties();
  emit this->propertiesChanged(props);
}

void QCMake::reloadCache()
{
  // emit that the cache was cleaned out
  QCMakePropertyList props;
  emit this->propertiesChanged(props);
  // reload
  this->CMakeInstance->GetCacheManager()->LoadCache(this->BinaryDirectory.toAscii().data());
  // emit new cache properties
  props = this->properties();
  emit this->propertiesChanged(props);
}
  
void QCMake::setDebugOutput(bool flag)
{
  if(flag != this->CMakeInstance->GetDebugOutput())
    {
    this->CMakeInstance->SetDebugOutputOn(flag);
    emit this->debugOutputChanged(flag);
    }
}

bool QCMake::getDebugOutput() const
{
  return this->CMakeInstance->GetDebugOutput();
}


void QCMake::setSuppressDevWarnings(bool value)
{
  this->SuppressDevWarnings = value;
}
