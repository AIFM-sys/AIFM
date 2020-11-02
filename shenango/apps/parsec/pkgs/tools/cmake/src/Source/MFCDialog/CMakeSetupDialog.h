/*=========================================================================

Program:   CMake - Cross-Platform Makefile Generator
Module:    $RCSfile: CMakeSetupDialog.h,v $
Language:  C++
Date:      $Date: 2012/03/29 17:21:10 $
Version:   $Revision: 1.1.1.1 $

Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even 
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
// CMakeSetupDialogDlg.h : header file
//

#if !defined(AFX_CMakeSetupDialogDLG_H__AC17A6F6_4634_11D4_8F21_00A0CC33FCD3__INCLUDED_)
#define AFX_CMakeSetupDialogDLG_H__AC17A6F6_4634_11D4_8F21_00A0CC33FCD3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include "PropertyList.h"
#include "CMakeGenDialog.h"

/////////////////////////////////////////////////////////////////////////////
// CMakeSetupDialog dialog

class CMakeCommandLineInfo;
class cmake;

class CMakeSetupDialog : public CDialog
{
// Construction
public:
  CMakeSetupDialog(const CMakeCommandLineInfo& cmdInfo, 
                   CWnd* pParent = NULL); 

  ~CMakeSetupDialog();
  // return the cmake that is currently being used
  cmake *GetCMakeInstance() {
    return m_CMakeInstance; }
protected:
  //! Load cache file from m_WhereBuild and display in GUI editor
  void LoadCacheFromDiskToGUI();
  //! Save GUI values to cmCacheManager and then save to disk.
  void SaveCacheFromGUI();
  void SaveToRegistry();
  void LoadFromRegistry();
  bool Browse(CString&, const char* title);
  void ReadRegistryValue(HKEY hKey,
                         CString *val,
                         const char *key,
                         const char *aadefault);
  void ShowAdvancedValues();
  void RemoveAdvancedValues();
  // Dialog Data
  //{{AFX_DATA(CMakeSetupDialog)
  enum { IDD = IDD_CMakeSetupDialog_DIALOG };
  CButton       m_AdvancedValuesControl;
  CButton       m_SuppressDevWarningsControl;
  CButton       m_BrowseSource;
  CButton       m_BrowseBuild;
  CButton m_HelpButton;
  CButton m_DeleteButton;
  CButton m_OKButton;
  CButton m_CancelButton;
  CString       m_WhereSource;
  CString       m_WhereBuild;
  CButton       m_ListFrame;
  bool          m_BuildPathChanged;
  CComboBox     m_WhereSourceControl;
  CComboBox     m_WhereBuildControl;
  CPropertyList m_CacheEntriesList;
  CStatic       m_MouseHelp;
  CStatic       m_StatusDisplay;
  CButton       m_Configure;
  BOOL    m_AdvancedValues;
  BOOL    m_SuppressDevValue;
  //}}AFX_DATA
  
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CMakeSetupDialog)
protected:
  virtual void DoDataExchange(CDataExchange* pDX);      // DDX/DDV support
  //}}AFX_VIRTUAL
  
// Implementation
protected:
  void RunCMake(bool generateProjectFiles);
  // copy from the cache manager to the cache edit list box
  void FillCacheGUIFromCacheManager();
  // copy from the list box to the cache manager
  void FillCacheManagerFromCacheGUI();
  // Create a shortcut on the desktop with the current Source/Build dir.
  int CreateShortcut();
  
  // Set initial directories from a file path.
  void ChangeDirectoriesFromFile(const char* arg);
  
  HICON m_hIcon;
  CString m_RegistryKey;
  CString m_PathToExecutable;
  // Generated message map functions
  //{{AFX_MSG(CMakeSetupDialog)
  virtual BOOL OnInitDialog();
  afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
  afx_msg void OnCancel();
  afx_msg void OnPaint();
  afx_msg HCURSOR OnQueryDragIcon();
  afx_msg void OnBrowseWhereSource();
  virtual void OnConfigure();
  afx_msg void OnBrowseWhereBuild();
  afx_msg void OnChangeWhereBuild();
  afx_msg void OnSelendokWhereBuild();
  afx_msg void OnChangeWhereSource();
  afx_msg void OnSelendokWhereSource();
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnGetMinMaxInfo( MINMAXINFO FAR* lpMMI );
  afx_msg void OnOk();
  afx_msg void OnHelpButton();
  afx_msg void OnDeleteButton();
  afx_msg void OnAdvancedValues();
  afx_msg void OnDoubleclickedAdvancedValues();
  afx_msg void OnSuppressDevValue();
  afx_msg void OnDoubleclickedSuppressDevValue();
  afx_msg void OnDropFiles(HDROP);
  afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    
  int m_oldCX;
  int m_oldCY;
  float m_deltaXRemainder;
  cmake *m_CMakeInstance;
  HCURSOR m_Cursor;
  bool m_RunningConfigure;
  bool m_GeneratorPicked;

  CCMakeGenDialog m_GeneratorDialog;

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CMakeSetupDialogDLG_H__AC17A6F6_4634_11D4_8F21_00A0CC33FCD3__INCLUDED_)
