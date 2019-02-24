# Microsoft Developer Studio Project File - Name="HiFilter" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=HiFilter - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "HiFilter.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "HiFilter.mak" CFG="HiFilter - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "HiFilter - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "HiFilter - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "HiFilter - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "NDEBUG" /D DBG=0 /YX /FD /c
# ADD CPP /nologo /Gz /W3 /Gi /O2 /I "D:\NTDDK\inc" /D "NDEBUG" /D DBG=0 /D "_X86_" /D _WIN32_WINNT=0x500 /YX /FD /Gs -GF /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 int64.lib ntoskrnl.lib hal.lib /nologo /base:"0x10000" /entry:"DriverEntry" /machine:I386 /nodefaultlib /out:"Release\HiFilter.SYS" /libpath:"D:\NTDDK\libfre\i386" -driver -subsystem:NATIVE,4.00
# Begin Custom Build - Copying Driver to System32\Drivers
TargetPath=.\Release\HiFilter.SYS
TargetName=HiFilter
InputPath=.\Release\HiFilter.SYS
SOURCE="$(InputPath)"

"$(SystemRoot)\System32\Drivers\$(TargetName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetPath) $(SystemRoot)\System32\Drivers\*.*

# End Custom Build

!ELSEIF  "$(CFG)" == "HiFilter - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D DBG=1 /YX /FD /GZ /c
# ADD CPP /nologo /Gz /W3 /Gm /Gi /Zi /Od /I "D:\NTDDK\inc" /D DBG=1 /D "_X86_" /D _WIN32_WINNT=0x500 /YX /FD /Gs -GF /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 int64.lib ntoskrnl.lib hal.lib /nologo /base:"0x10000" /entry:"DriverEntry" /incremental:no /debug /machine:I386 /nodefaultlib /out:"Debug\HiFilter.SYS" /pdbtype:con /libpath:"D:\NTDDK\libchk\i386" -driver -subsystem:NATIVE,4.00
# Begin Custom Build - Copying Driver to System32\Drivers
TargetPath=.\Debug\HiFilter.SYS
TargetName=HiFilter
InputPath=.\Debug\HiFilter.SYS
SOURCE="$(InputPath)"

"$(SystemRoot)\System32\Drivers\$(TargetName)" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetPath) $(SystemRoot)\System32\Drivers\*.*

# End Custom Build

!ENDIF 

# Begin Target

# Name "HiFilter - Win32 Release"
# Name "HiFilter - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\Driver.cpp
# End Source File
# Begin Source File

SOURCE=.\EventLog.cpp
# End Source File
# Begin Source File

SOURCE=.\Unicode.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\Driver.h
# End Source File
# Begin Source File

SOURCE=.\EventLog.h
# End Source File
# Begin Source File

SOURCE=.\Msg.h
# End Source File
# Begin Source File

SOURCE=.\Unicode.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\Msg.rc
# End Source File
# Begin Source File

SOURCE=.\MSG00001.bin
# End Source File
# End Group
# Begin Source File

SOURCE=.\HiFilter.reg
# End Source File
# Begin Source File

SOURCE=.\Msg.mc

!IF  "$(CFG)" == "HiFilter - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Build Msg.mc File using MC Tool
InputPath=.\Msg.mc
InputName=Msg

BuildCmds= \
	MC -c $(InputName).mc

"$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputName).RC" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "HiFilter - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Build Msg.mc File using MC Tool
InputPath=.\Msg.mc
InputName=Msg

BuildCmds= \
	MC -c $(InputName).mc

"$(InputName).h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(InputName).RC" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
