/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: PropertyList.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef CPROPERTYLIST_H
#define CPROPERTYLIST_H



#include "../cmStandardIncludes.h"
class CMakeSetupDialog;

/////////////////////////////////////////////////////////////////////////////
//CPropertyList Items
class CPropertyItem
{
// Attributes
public:
  CString m_HelpString;
  CString m_propName;
  CString m_curValue;
  int m_nItemType;
  CString m_cmbItems;
  bool m_NewValue;
  bool m_Removed;
  bool m_Advanced;
  
public:
  CPropertyItem(CString propName, CString curValue,
                CString helpString,
                int nItemType, CString cmbItems)
    {
      m_NewValue = true;
      m_HelpString = helpString;
      m_Removed = false;
      m_propName = propName;
      m_curValue = curValue;
      m_nItemType = nItemType;
      m_cmbItems = cmbItems;
      m_Advanced = false;
    }
};

/////////////////////////////////////////////////////////////////////////////
// CPropertyList window

class CPropertyList : public CListBox
{
// Construction
public:
  enum ItemType 
    {
      COMBO = 0,
      EDIT,
      COLOR,
      FONT,
      FILE,
      CHECKBOX,
      PATH
    };
  CPropertyList();
  
// Attributes
public:
  CMakeSetupDialog *m_CMakeSetupDialog;

// Operations
public:
  bool GetShowAdvanced() {return m_ShowAdvanced;}
  bool IsDirty() { return m_Dirty;  }
  void ClearDirty() { m_Dirty = false;  }
  
  int AddItem(CString txt);
  void AddProperty(const char* name,
                  const char* value,
                  const char* helpString,
                  int type,
                  const char* comboItems,
                  bool reverseOrder,
                  bool advanced);
  void RemoveProperty(const char* name);
  void HideControls();
  void ShowAdvanced();
  void HideAdvanced();
  std::set<CPropertyItem*> GetItems() 
    {
      return m_PropertyItems;
    }
  void RemoveAll();
  CPropertyItem* GetItem(int index);
// Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CPropertyList)
public:
  virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
  virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
protected:
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
  virtual void PreSubclassWindow();
  //}}AFX_VIRTUAL

// Implementation
public:
  virtual ~CPropertyList();

  // Generated message map functions
protected:
  //{{AFX_MSG(CPropertyList)
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnSelchange();
  afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
  afx_msg void OnKillFocus(CWnd* pNewWnd);
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnMouseMove(UINT nFlags, CPoint point);
  afx_msg void OnRButtonUp( UINT nFlags, CPoint point );
  //}}AFX_MSG
  afx_msg void OnKillfocusCmbBox();
  afx_msg void OnSelchangeCmbBox();
  afx_msg void OnKillfocusEditBox();
  afx_msg void OnChangeEditBox();
  afx_msg void OnButton();
  afx_msg void OnIgnore();
  afx_msg void OnDelete();
  afx_msg void OnHelp();
  afx_msg void OnCheckBox();
  afx_msg void OnVScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar );


  DECLARE_MESSAGE_MAP()

  void InvertLine(CDC* pDC,CPoint ptFrom,CPoint ptTo);
  void DisplayButton(CRect region);
// order = 0 sorted
// order = 1 add to top
// order = 2 add to bottom
  int AddPropItem(CPropertyItem* pItem, int order);
  void InvalidateList();
  
  CComboBox m_cmbBox;
  CEdit m_editBox;
  CButton m_btnCtrl;
  CButton m_CheckBoxControl;
  
  CFont m_SSerif8Font;
        
  bool m_Dirty;
  int m_curSel;
  int m_prevSel;
  int m_nDivider;
  int m_nDivTop;
  int m_nDivBtm;
  int m_nOldDivX;
  int m_nLastBox;
  BOOL m_bTracking;
  BOOL m_bDivIsSet;
  HCURSOR m_hCursorArrow;
  HCURSOR m_hCursorSize;
  bool m_ShowAdvanced;
  std::set<CPropertyItem*> m_PropertyItems;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROPERTYLIST_H__74205380_1B56_11D4_BC48_00105AA2186F__INCLUDED_)
