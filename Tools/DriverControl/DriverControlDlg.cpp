// DriverControlDlg.cpp : implementation file
//

#include "stdafx.h"
#include "DriverControl.h"
#include "DriverControlDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDriverControlDlg dialog

CDriverControlDlg::CDriverControlDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CDriverControlDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDriverControlDlg)
	m_drvName = _T("");
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	m_hSCM = NULL;
	m_hDriver = NULL;
}

void CDriverControlDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDriverControlDlg)
	DDX_Text(pDX, IDC_DRIVERNAME, m_drvName);
	//}}AFX_DATA_MAP

	if (pDX->m_bSaveAndValidate && m_drvName.IsEmpty()) {
		AfxMessageBox("Please supply a Driver name");
		pDX->Fail();
	}
}

BEGIN_MESSAGE_MAP(CDriverControlDlg, CDialog)
	//{{AFX_MSG_MAP(CDriverControlDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_STARTBTN, OnStartbtn)
	ON_BN_CLICKED(IDC_STOPBTN, OnStopbtn)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDriverControlDlg message handlers

BOOL CDriverControlDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CDriverControlDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CDriverControlDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CDriverControlDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CDriverControlDlg::OnStartbtn() 
{
	// TODO: Add your control notification handler code here

	if (!UpdateData(TRUE)) return;	// read screen

	if (!OpenDriver())
		return;			// Helper function will report error

	// Query driver to see if currently stopped...
	SERVICE_STATUS ss;
	ControlService(m_hDriver, SERVICE_CONTROL_INTERROGATE,
							&ss);
	if (ss.dwCurrentState != SERVICE_STOPPED) {
		AfxMessageBox("Driver is not in STOPPED state");
		return;
	}

	// Now do the request - start the thing
	DWORD success =
		StartService(m_hDriver, 0, NULL);	// no args
	if (!success) {
		DWORD err = GetLastError();
		CString msg;
		msg.Format("The attempt to start service %s failed due to error: %d",
					m_drvName, err);
		AfxMessageBox(msg);
		return;
	}

	// Finally - pause up to 20 seconds waiting for it to start
	int time = 0;
	while (time < 20) {
		ControlService(m_hDriver, SERVICE_CONTROL_INTERROGATE, &ss);
		if (ss.dwCurrentState == SERVICE_RUNNING) break;
		Sleep(2000);
		time += 2;
	}

	if (time >= 20) {	// did we finally start?
		AfxMessageBox("Driver failed to start...");
		return;
	}

	CString msg;
	msg.Format("Driver: %s has started", m_drvName);
	AfxMessageBox(msg);
	m_state = ss.dwCurrentState;
	GetDlgItem(IDC_STARTBTN)->EnableWindow(FALSE);
	GetDlgItem(IDC_STOPBTN)->EnableWindow(TRUE);

}

void CDriverControlDlg::OnStopbtn() 
{
	// TODO: Add your control notification handler code here
	if (!UpdateData(TRUE)) return;	// read screen

	// open the SCM and the Driver
	if (!OpenDriver())
		return;			// helper routine reported error

	// Query driver to see if currently running...
	SERVICE_STATUS ss;
	ControlService(m_hDriver, SERVICE_CONTROL_INTERROGATE,
							&ss);
	if (ss.dwCurrentState != SERVICE_RUNNING) {
		AfxMessageBox("Driver is not in RUNNING state");
		return;
	}

	// Now do the request - stop the thing
	DWORD success =
		ControlService(m_hDriver, SERVICE_CONTROL_STOP,
							&ss);
	if (!success) {
		DWORD err = GetLastError();
		CString msg;
		msg.Format("The attempt to stop service %s failed due to error: %d",
					m_drvName, err);
		AfxMessageBox(msg);
		return;
	}

	// Finally - pause up to 20 seconds waiting for it to stop
	int time = 0;
	while (time < 20) {
		ControlService(m_hDriver, SERVICE_CONTROL_INTERROGATE, &ss);
		if (ss.dwCurrentState == SERVICE_STOPPED) break;
		Sleep(2000);
		time += 2;
	}

	if (time >= 20) {	// did we finally stop?
		AfxMessageBox("Driver failed to stop...");
		return;
	}

	CString msg;
	msg.Format("Driver: %s has stopped", m_drvName);
	AfxMessageBox(msg);
	m_state = ss.dwCurrentState;
	GetDlgItem(IDC_STOPBTN)->EnableWindow(FALSE);
	GetDlgItem(IDC_STARTBTN)->EnableWindow(TRUE);

}


bool CDriverControlDlg::OpenDriver()
{
	if (m_hSCM == NULL) {
		m_hSCM =
			OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (m_hSCM == NULL) {
			AfxMessageBox("Failed to gain access to the Service Control Manager\n"
						"Possibly running without Administrator rights?");
			return false;
		}
	}

	if (m_hDriver == NULL) {
		m_hDriver =
			OpenService(m_hSCM, m_drvName, SERVICE_ALL_ACCESS);
		if (m_hDriver == NULL) {
			CString msg;
			msg.Format("Failed to locate driver: %s", m_drvName);
			AfxMessageBox(msg);
			return false;
		}
	}
	return true;
}
