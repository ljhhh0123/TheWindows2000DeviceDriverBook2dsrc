// DriverControlDlg.h : header file
//

#if !defined(AFX_DRIVERCONTROLDLG_H__06FCCF8A_061E_4686_970B_3F5251749AFA__INCLUDED_)
#define AFX_DRIVERCONTROLDLG_H__06FCCF8A_061E_4686_970B_3F5251749AFA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <winsvc.h>

/////////////////////////////////////////////////////////////////////////////
// CDriverControlDlg dialog

class CDriverControlDlg : public CDialog
{
// Construction
public:
	SC_HANDLE m_hDriver;
	CDriverControlDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CDriverControlDlg)
	enum { IDD = IDD_DRIVERCONTROL_DIALOG };
	CString	m_drvName;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDriverControlDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	bool OpenDriver();
	SC_HANDLE m_hSCM;
	DWORD m_state;
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CDriverControlDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnStartbtn();
	afx_msg void OnStopbtn();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DRIVERCONTROLDLG_H__06FCCF8A_061E_4686_970B_3F5251749AFA__INCLUDED_)
