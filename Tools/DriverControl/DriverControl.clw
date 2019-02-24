; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CDriverControlDlg
LastTemplate=CDialog
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "DriverControl.h"

ClassCount=3
Class1=CDriverControlApp
Class2=CDriverControlDlg
Class3=CAboutDlg

ResourceCount=3
Resource1=IDD_ABOUTBOX
Resource2=IDR_MAINFRAME
Resource3=IDD_DRIVERCONTROL_DIALOG

[CLS:CDriverControlApp]
Type=0
HeaderFile=DriverControl.h
ImplementationFile=DriverControl.cpp
Filter=N

[CLS:CDriverControlDlg]
Type=0
HeaderFile=DriverControlDlg.h
ImplementationFile=DriverControlDlg.cpp
Filter=D
BaseClass=CDialog
VirtualFilter=dWC
LastObject=CDriverControlDlg

[CLS:CAboutDlg]
Type=0
HeaderFile=DriverControlDlg.h
ImplementationFile=DriverControlDlg.cpp
Filter=D

[DLG:IDD_ABOUTBOX]
Type=1
Class=CAboutDlg
ControlCount=4
Control1=IDC_STATIC,static,1342177283
Control2=IDC_STATIC,static,1342308480
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342373889

[DLG:IDD_DRIVERCONTROL_DIALOG]
Type=1
Class=CDriverControlDlg
ControlCount=5
Control1=IDC_STATIC,static,1342308352
Control2=IDC_DRIVERNAME,edit,1350631552
Control3=IDC_STARTBTN,button,1342242816
Control4=IDC_STOPBTN,button,1342242816
Control5=IDOK,button,1342242817

