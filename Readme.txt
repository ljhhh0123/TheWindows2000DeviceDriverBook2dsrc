I.  CONTENTS OF THE CD

The Windows 2000 Device Driver Book CD contains the following components:

- Sample drivers presented at the end of 12 chapters in the book
- Tools to facilitate development and installation of some drivers


II.  SYSTEM REQUIREMENTS

- Windows 2000 Professional or Server
- Visual C++ 6.0 Professional or Enterprise
- Windows 2000 DDK
- Windows 2000 Platform SDK 
- Parallel Port Loopback connector (CheckIt(R) compliant)


The Windows 2000 DDK is available for free download from:

	http://www.microsoft.com/DDK/


The Windows 2000 Platform SDK is available for free download from:

	http://msdn.microsoft.com/downloads/sdks/platform/

Of course, all necessary software to develop Windows 2000 device 
drivers are available from Microsoft as part of MSDN Subscription service.


It is also recommended that two tools:

- WinObj.exe
- DbgView.exe

be obtained by download from the site:

	http://www.sysinternals.com


The Parallel Port loopback connector is required for several sample
drivers used in the book and is explained in Chapter 8 of the book.
The DB-25 connector can be constructed according to directions within
this chapter, or can be purchased commercially.


III. INSTALLATION

III-1.

It is important that Visual C++ (Visual Studio) be installed BEFORE
installing the Platform SDK or the DDK.  Also, be sure to check the
innocent box "Register Environment Variables" (by default it is left
unchecked) near the end of the Visual C++ installation.  This selection 
sets up numerous environment (including path) variables necessary for
successful installation of the Platform SDK and DDK.


III-2.

Once the Microsoft products are successfully installed, copy the file:

DDAppWiz.awx

from the Tools directory of this CD to the system's directory:

...\Microsoft Visual Studio\Common\MsDev98\Template

Typically, this directory is located under the system's "Program Files"
directory.

Once copied, a new App Wizard (for creating projects for Windows 2000
device drivers) is available from the Visual C++ IDE (File...New...Project).


III-3.

Copy the directories "ChapNN" from this CD to the Windows 2000 system, 
to the directory of your choice.  Also, copy the Tools directory.


IV.  TESTING THE INSTALLATION

Each ChapNN directory contains one (or more) sub-directories for the
sample driver explained within the chapter.  For example, Chap6 contains
Visual Studio workspace, project, and source files for a "Minimal"
Windows 2000 device driver.


IV-1.

By navigating to Chap6\Minimal, the file:

Minimal.dsw

can be opened within Visual Studio as a workspace.

Once opened, select Project...Settings...C/C++...Category: Preprocessor

and ensure that the path to the "Additional Include Directories" is
appropriate for your installation of the DDK.  The workspaces contained
on this CD were generated assuming that the DDK was installed on:

D:\NTDDK

Be sure to modify the two include directories to reflect the actual
installation location for your system.

Similarly, select Project...Settings...Link...Category: Input

and ensure that the path to the "Additional Library Path" is appropriate.

Then build the project (Build...Build Minimal.SYS).

Assuming no errors, the .SYS file is copied to the "system32\Drivers\"
directory.  (The copy procedure occurs because of a "Post Build" step
within the Project Settings.)


IV-2.

From the Windows Explorer (a.k.a. File Manager), locate and double click on the file:

Chap6\Minimal\Minimal.reg

This action will install appropriate registry entries for the Minimal driver.


IV-3.

Reboot the system.


IV-4.

From the Tools directory (copied from this CD), launch the program:

Tools\DriverControl.exe

Type "Minimal" for the name of the driver.
Click "Start"

The driver should start.


IV-5.

Use WinObj.exe (from the Platform SDK or from "www.sysinternals.com") to
validate that the driver has started.

A similar procedure applies to all driver samples throughout the book.


V.  SUPPORT

Be sure to visit the book's web site:

http://www.W2KDriverBook.com/

for updates or troubleshooting suggestions.

