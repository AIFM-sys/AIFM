/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: QCMakeCacheView.cxx,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:09 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include "QCMakeCacheView.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QEvent>
#include <QStyle>
#include <QKeyEvent>
#include <QSortFilterProxyModel>

#include "QCMakeWidgets.h"

// filter for searches
class QCMakeSearchFilter : public QSortFilterProxyModel
{
public:
  QCMakeSearchFilter(QObject* o) : QSortFilterProxyModel(o) {}
protected:
  bool filterAcceptsRow(int row, const QModelIndex& p) const
    {
    QStringList strs;
    const QAbstractItemModel* m = this->sourceModel();
    QModelIndex idx = m->index(row, 0, p);

    // if there are no children, get strings for column 0 and 1
    if(!m->hasChildren(idx))
      {
      strs.append(m->data(idx).toString());
      idx = m->index(row, 1, p);
      strs.append(m->data(idx).toString());
      }
    else
      {
      // get strings for children entries to compare with
      // instead of comparing with the parent
      int num = m->rowCount(idx);
      for(int i=0; i<num; i++)
        {
        QModelIndex tmpidx = m->index(i, 0, idx);
        strs.append(m->data(tmpidx).toString());
        tmpidx = m->index(i, 1, idx);
        strs.append(m->data(tmpidx).toString());
        }
      }

    // check all strings for a match
    foreach(QString str, strs)
      {
      if(str.contains(this->filterRegExp()))
        {
        return true;
        }
      }
    
    return false;
    }
};

// filter for searches
class QCMakeAdvancedFilter : public QSortFilterProxyModel
{
public:
  QCMakeAdvancedFilter(QObject* o) 
    : QSortFilterProxyModel(o), ShowAdvanced(false) {}

  void setShowAdvanced(bool f) 
  { 
    this->ShowAdvanced = f;
    this->invalidate();
  }
  bool showAdvanced() const { return this->ShowAdvanced; }

protected:

  bool ShowAdvanced;

  bool filterAcceptsRow(int row, const QModelIndex& p) const
    {
    const QAbstractItemModel* m = this->sourceModel();
    QModelIndex idx = m->index(row, 0, p);

    // if there are no children
    if(!m->hasChildren(idx))
      {
      bool adv = m->data(idx, QCMakeCacheModel::AdvancedRole).toBool();
      if(!adv || (adv && this->ShowAdvanced))
        {
        return true;
        }
      return false;
      }
    
    // check children
    int num = m->rowCount(idx);
    for(int i=0; i<num; i++)
      {
      bool accept = this->filterAcceptsRow(i, idx);
      if(accept)
        {
        return true;
        }
      }
    return false;
    }
};

QCMakeCacheView::QCMakeCacheView(QWidget* p)
  : QTreeView(p)
{
  // hook up our model and search/filter proxies
  this->CacheModel = new QCMakeCacheModel(this);
  this->AdvancedFilter = new QCMakeAdvancedFilter(this);
  this->AdvancedFilter->setSourceModel(this->CacheModel);
  this->AdvancedFilter->setDynamicSortFilter(true);
  this->SearchFilter = new QCMakeSearchFilter(this);
  this->SearchFilter->setSourceModel(this->AdvancedFilter);
  this->SearchFilter->setFilterCaseSensitivity(Qt::CaseInsensitive);
  this->SearchFilter->setDynamicSortFilter(true);
  this->setModel(this->SearchFilter);

  // our delegate for creating our editors
  QCMakeCacheModelDelegate* delegate = new QCMakeCacheModelDelegate(this);
  this->setItemDelegate(delegate);
  
  this->setEditTriggers(QAbstractItemView::DoubleClicked |
                        QAbstractItemView::SelectedClicked |
                        QAbstractItemView::EditKeyPressed |
                        QAbstractItemView::AnyKeyPressed);

  // tab, backtab doesn't step through items
  this->setTabKeyNavigation(false);

  this->setRootIsDecorated(false);
}

bool QCMakeCacheView::event(QEvent* e)
{
  if(e->type() == QEvent::Show)
    {
    this->header()->setDefaultSectionSize(this->viewport()->width()/2);
    }
  return QTreeView::event(e);
}
  
QCMakeCacheModel* QCMakeCacheView::cacheModel() const
{
  return this->CacheModel;
}
 
QModelIndex QCMakeCacheView::moveCursor(CursorAction act, 
  Qt::KeyboardModifiers mod)
{
  // want home/end to go to begin/end of rows, not columns
  if(act == MoveHome)
    {
    return this->model()->index(0, 1);
    }
  else if(act == MoveEnd)
    {
    return this->model()->index(this->model()->rowCount()-1, 1);
    }
  return QTreeView::moveCursor(act, mod);
}
  
void QCMakeCacheView::setShowAdvanced(bool s)
{
#if QT_VERSION >= 040300
  // new 4.3 api that needs to be called.  what about an older Qt?
  this->SearchFilter->invalidate();
#endif

  this->AdvancedFilter->setShowAdvanced(s);
}

bool QCMakeCacheView::showAdvanced() const
{
  return this->AdvancedFilter->showAdvanced();
}

void QCMakeCacheView::setSearchFilter(const QString& s)
{
  this->SearchFilter->setFilterFixedString(s);
}

QCMakeCacheModel::QCMakeCacheModel(QObject* p)
  : QStandardItemModel(p),
    EditEnabled(true),
    NewPropertyCount(0),
    View(FlatView)
{
  QStringList labels;
  labels << tr("Name") << tr("Value");
  this->setHorizontalHeaderLabels(labels);
}

QCMakeCacheModel::~QCMakeCacheModel()
{
}

static uint qHash(const QCMakeProperty& p)
{
  return qHash(p.Key);
}

void QCMakeCacheModel::clear()
{
  this->QStandardItemModel::clear();
  this->NewPropertyCount = 0;
  
  QStringList labels;
  labels << tr("Name") << tr("Value");
  this->setHorizontalHeaderLabels(labels);
}

void QCMakeCacheModel::setProperties(const QCMakePropertyList& props)
{
  QSet<QCMakeProperty> newProps = props.toSet();
  QSet<QCMakeProperty> newProps2 = newProps;
  QSet<QCMakeProperty> oldProps = this->properties().toSet();
  
  oldProps.intersect(newProps);
  newProps.subtract(oldProps);
  newProps2.subtract(newProps);

  bool b = this->blockSignals(true);

  this->clear();
  this->NewPropertyCount = newProps.size();

  if(View == FlatView)
  {
    QCMakePropertyList newP = newProps.toList();
    QCMakePropertyList newP2 = newProps2.toList();
    qSort(newP);
    qSort(newP2);
    int rowCount = 0;
    foreach(QCMakeProperty p, newP)
    {
      this->insertRow(rowCount);
      this->setPropertyData(this->index(rowCount, 0), p, true);
      rowCount++;
    }
    foreach(QCMakeProperty p, newP2)
    {
      this->insertRow(rowCount);
      this->setPropertyData(this->index(rowCount, 0), p, false);
      rowCount++;
    }
  }
  else if(this->View == GroupView)
  {
    QMap<QString, QCMakePropertyList> newPropsTree;
    this->breakProperties(newProps, newPropsTree);
    QMap<QString, QCMakePropertyList> newPropsTree2;
    this->breakProperties(newProps2, newPropsTree2);

    QStandardItem* root = this->invisibleRootItem();
    
    foreach(QString key, newPropsTree.keys())
      {
      QCMakePropertyList props = newPropsTree[key];

      QList<QStandardItem*> parentItems;
      parentItems.append(
        new QStandardItem(key.isEmpty() ? tr("Ungrouped Entries") : key)
        );
      parentItems.append(new QStandardItem());
      parentItems[0]->setData(QBrush(QColor(255,100,100)), Qt::BackgroundColorRole);
      parentItems[1]->setData(QBrush(QColor(255,100,100)), Qt::BackgroundColorRole);
      root->appendRow(parentItems);

      foreach(QCMakeProperty prop, props)
        {
        QList<QStandardItem*> items;
        items.append(new QStandardItem());
        items.append(new QStandardItem());
        parentItems[0]->appendRow(items);
        this->setPropertyData(this->indexFromItem(items[0]), prop, true);
        }
      }
    
    foreach(QString key, newPropsTree2.keys())
      {
      QCMakePropertyList props = newPropsTree2[key];

      QStandardItem* parentItem = 
        new QStandardItem(key.isEmpty() ? tr("Ungrouped Entries") : key);
      root->appendRow(parentItem);

      foreach(QCMakeProperty prop, props)
        {
        QList<QStandardItem*> items;
        items.append(new QStandardItem());
        items.append(new QStandardItem());
        parentItem->appendRow(items);
        this->setPropertyData(this->indexFromItem(items[0]), prop, false);
        }
      }
  }
  
  this->blockSignals(b);
  this->reset();
}

QCMakeCacheModel::ViewType QCMakeCacheModel::viewType() const
{
  return this->View;
}

void QCMakeCacheModel::setViewType(QCMakeCacheModel::ViewType t)
{
  this->View = t;

  QCMakePropertyList props = this->properties();
  QCMakePropertyList oldProps;
  int numNew = this->NewPropertyCount;
  int numTotal = props.count();
  for(int i=numNew; i<numTotal; i++)
  {
    oldProps.append(props[i]);
  }

  bool b = this->blockSignals(true);
  this->clear();
  this->setProperties(oldProps);
  this->setProperties(props);
  this->blockSignals(b);
  this->reset();
}

void QCMakeCacheModel::setPropertyData(const QModelIndex& idx1, 
    const QCMakeProperty& prop, bool isNew)
{
  QModelIndex idx2 = idx1.sibling(idx1.row(), 1);

  this->setData(idx1, prop.Key, Qt::DisplayRole);
  this->setData(idx1, prop.Help, QCMakeCacheModel::HelpRole);
  this->setData(idx1, prop.Type, QCMakeCacheModel::TypeRole);
  this->setData(idx1, prop.Advanced, QCMakeCacheModel::AdvancedRole);
  
  if(prop.Type == QCMakeProperty::BOOL)
  {
    int check = prop.Value.toBool() ? Qt::Checked : Qt::Unchecked;
    this->setData(idx2, check, Qt::CheckStateRole);
  }
  else
  {
    this->setData(idx2, prop.Value, Qt::DisplayRole);
  }
  this->setData(idx2, prop.Help, QCMakeCacheModel::HelpRole);

  if(isNew)
  {
    this->setData(idx1, QBrush(QColor(255,100,100)), Qt::BackgroundColorRole);
    this->setData(idx2, QBrush(QColor(255,100,100)), Qt::BackgroundColorRole);
  }
}

void QCMakeCacheModel::getPropertyData(const QModelIndex& idx1, 
    QCMakeProperty& prop) const
{
  QModelIndex idx2 = idx1.sibling(idx1.row(), 1);

  prop.Key = this->data(idx1, Qt::DisplayRole).toString();
  prop.Help = this->data(idx1, HelpRole).toString();
  prop.Type = static_cast<QCMakeProperty::PropertyType>(this->data(idx1, TypeRole).toInt());
  prop.Advanced = this->data(idx1, AdvancedRole).toBool();
  if(prop.Type == QCMakeProperty::BOOL)
  {
    int check = this->data(idx2, Qt::CheckStateRole).toInt();
    prop.Value = check == Qt::Checked;
  }
  else
  {
    prop.Value = this->data(idx2, Qt::DisplayRole).toString();
  }
}

QString QCMakeCacheModel::prefix(const QString& s)
{
  QString prefix = s.section('_', 0, 0);
  if(prefix == s)
    {
    prefix = QString();
    }
  return prefix;
}

void QCMakeCacheModel::breakProperties(const QSet<QCMakeProperty>& props,
                     QMap<QString, QCMakePropertyList>& result)
{
  QMap<QString, QCMakePropertyList> tmp;
  // return a map of properties grouped by prefixes, and sorted
  foreach(QCMakeProperty p, props)
    {
    QString prefix = QCMakeCacheModel::prefix(p.Key);
    tmp[prefix].append(p);
    }
  // sort it and re-org any properties with only one sub item
  QCMakePropertyList reorgProps;
  QMap<QString, QCMakePropertyList>::iterator iter;
  for(iter = tmp.begin(); iter != tmp.end();)
    {
    if(iter->count() == 1)
      {
      reorgProps.append((*iter)[0]);
      iter = tmp.erase(iter);
      }
    else
      {
      qSort(*iter);
      ++iter;
      }
    }
  if(reorgProps.count())
    {
    tmp[QString()] += reorgProps;
    }
  result = tmp;
}
  
QCMakePropertyList QCMakeCacheModel::properties() const
{
  QCMakePropertyList props;

  if(!this->rowCount())
    {
    return props;
    }

  QList<QModelIndex> idxs;
  idxs.append(this->index(0,0));

  // walk the entire model for property entries
  // this works regardless of a flat view or a tree view
  while(!idxs.isEmpty())
  {
    QModelIndex idx = idxs.last();
    if(this->hasChildren(idx) && this->rowCount(idx))
    {
      idxs.append(this->index(0,0, idx));
    }
    else
    {
      // get data
      QCMakeProperty prop;
      this->getPropertyData(idx, prop);
      props.append(prop);
      
      // go to the next in the tree
      while(!idxs.isEmpty() && !idxs.last().sibling(idxs.last().row()+1, 0).isValid())
      {
        idxs.removeLast();
      }
      if(!idxs.isEmpty())
      {
        idxs.last() = idxs.last().sibling(idxs.last().row()+1, 0);
      }
    }
  }

  return props;
}
  
bool QCMakeCacheModel::insertProperty(QCMakeProperty::PropertyType t,
                      const QString& name, const QString& description,
                      const QVariant& value, bool advanced)
{
  QCMakeProperty prop;
  prop.Key = name;
  prop.Value = value;
  prop.Help = description;
  prop.Type = t;
  prop.Advanced = advanced;

  //insert at beginning
  this->insertRow(0);
  this->setPropertyData(this->index(0,0), prop, true);
  this->NewPropertyCount++;
  return true;
}

void QCMakeCacheModel::setEditEnabled(bool e)
{
  this->EditEnabled = e;
}

bool QCMakeCacheModel::editEnabled() const
{
  return this->EditEnabled;
}

int QCMakeCacheModel::newPropertyCount() const
{
  return this->NewPropertyCount;
}

Qt::ItemFlags QCMakeCacheModel::flags (const QModelIndex& idx) const
{
  Qt::ItemFlags f = QStandardItemModel::flags(idx);
  if(!this->EditEnabled)
    {
    f &= ~Qt::ItemIsEditable;
    }
  if(QCMakeProperty::BOOL == this->data(idx, TypeRole).toInt())
    {
    f |= Qt::ItemIsUserCheckable;
    }
  return f;
}

QModelIndex QCMakeCacheModel::buddy(const QModelIndex& idx) const
{
  if(!this->hasChildren(idx) && 
     this->data(idx, TypeRole).toInt() != QCMakeProperty::BOOL)
  {
    return this->index(idx.row(), 1, idx.parent());
  }
  return idx;
}

QCMakeCacheModelDelegate::QCMakeCacheModelDelegate(QObject* p)
  : QItemDelegate(p), FileDialogFlag(false)
{
}

void QCMakeCacheModelDelegate::setFileDialogFlag(bool f)
{
  this->FileDialogFlag = f;
}

QWidget* QCMakeCacheModelDelegate::createEditor(QWidget* p, 
    const QStyleOptionViewItem&, const QModelIndex& idx) const
{
  QModelIndex var = idx.sibling(idx.row(), 0);
  int type = var.data(QCMakeCacheModel::TypeRole).toInt();
  if(type == QCMakeProperty::BOOL)
    {
    return NULL;
    }
  else if(type == QCMakeProperty::PATH)
    {
    QCMakePathEditor* editor =
      new QCMakePathEditor(p, 
      var.data(Qt::DisplayRole).toString());
    QObject::connect(editor, SIGNAL(fileDialogExists(bool)), this,
        SLOT(setFileDialogFlag(bool)));
    return editor;
    }
  else if(type == QCMakeProperty::FILEPATH)
    {
    QCMakeFilePathEditor* editor =
      new QCMakeFilePathEditor(p, 
      var.data(Qt::DisplayRole).toString());
    QObject::connect(editor, SIGNAL(fileDialogExists(bool)), this,
        SLOT(setFileDialogFlag(bool)));
    return editor;
    }

  return new QLineEdit(p);
}
  
bool QCMakeCacheModelDelegate::editorEvent(QEvent* e, QAbstractItemModel* model, 
       const QStyleOptionViewItem& option, const QModelIndex& index)
{
  Qt::ItemFlags flags = model->flags(index);
  if (!(flags & Qt::ItemIsUserCheckable) || !(option.state & QStyle::State_Enabled)
      || !(flags & Qt::ItemIsEnabled))
    {
    return false;
    }

  QVariant value = index.data(Qt::CheckStateRole);
  if (!value.isValid())
    {
    return false;
    }

  if ((e->type() == QEvent::MouseButtonRelease)
      || (e->type() == QEvent::MouseButtonDblClick))
    {
    // eat the double click events inside the check rect
    if (e->type() == QEvent::MouseButtonDblClick)
      {
      return true;
      }
    } 
  else if (e->type() == QEvent::KeyPress)
    {
    if(static_cast<QKeyEvent*>(e)->key() != Qt::Key_Space &&
       static_cast<QKeyEvent*>(e)->key() != Qt::Key_Select)
      {
      return false;
      }
    } 
  else 
    {
    return false;
    }

  Qt::CheckState state = (static_cast<Qt::CheckState>(value.toInt()) == Qt::Checked
                          ? Qt::Unchecked : Qt::Checked);
  return model->setData(index, state, Qt::CheckStateRole);
}
  
bool QCMakeCacheModelDelegate::eventFilter(QObject* object, QEvent* event)
{
  // workaround for what looks like a bug in Qt on Mac OS X
  // where it doesn't create a QWidget wrapper for the native file dialog
  // so the Qt library ends up assuming the focus was lost to something else
  if(event->type() == QEvent::FocusOut && this->FileDialogFlag)
    {
    return false;
    }
  return QItemDelegate::eventFilter(object, event);
}

  
