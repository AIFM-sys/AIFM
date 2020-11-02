/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile: MakeHelp.h,v $
  Language:  C++
  Date:      $Date: 2012/03/29 17:21:10 $
  Version:   $Revision: 1.1.1.1 $

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#if !defined(AFX_MAKEHELP_H__DD327AED_1E65_43E8_A605_0933065D1757__INCLUDED_)
#define AFX_MAKEHELP_H__DD327AED_1E65_43E8_A605_0933065D1757__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MakeHelp.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CMakeHelp dialog

class CMakeHelp : public CDialog
{
// Construction
public:
        CMakeHelp(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
        //{{AFX_DATA(CMakeHelp)
        enum { IDD = IDD_CMAKE_HELP_DIALOG };
        CString m_HelpMessage;
        //}}AFX_DATA


// Overrides
        // ClassWizard generated virtual function overrides
        //{{AFX_VIRTUAL(CMakeHelp)
        protected:
        virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
        //}}AFX_VIRTUAL

// Implementation
protected:

        // Generated message map functions
        //{{AFX_MSG(CMakeHelp)
                // NOTE: the ClassWizard will add member functions here
        //}}AFX_MSG
        DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAKEHELP_H__DD327AED_1E65_43E8_A605_0933065D1757__INCLUDED_)
