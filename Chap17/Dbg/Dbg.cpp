//++
// File Name:
//		Dbg.cpp
//
// Contents:
//		WINDBG extensions for TIMERPP (from Chap 11)
//--

//
// Header files...
//

/////////////////////////////////////////////////////
// HERE BEGINS A MAGICKAL SEQUENCE OF HEADER FILES
// AND DEFINTIONS. DON'T CHANGE ANYTHING !!!
//

#define _X86_

extern "C" {
#include <ntddk.h>
#include <windef.h>
}

//
// The following items are from WINBASE.H
// in the Win32 SDK. (WINBASE.H itself can't
// coexist with NTDDK.H, yet we need to be 
// able to get at DDK and driver-defined data 
// structures and types.)
//
#define LMEM_FIXED          0x0000
#define LMEM_MOVEABLE       0x0002
#define LMEM_NOCOMPACT      0x0010
#define LMEM_NODISCARD      0x0020
#define LMEM_ZEROINIT       0x0040
#define LMEM_MODIFY         0x0080
#define LMEM_DISCARDABLE    0x0F00
#define LMEM_VALID_FLAGS    0x0F72
#define LMEM_INVALID_HANDLE 0x8000

#define LPTR (LMEM_FIXED | LMEM_ZEROINIT)

#define WINBASEAPI

WINBASEAPI
HLOCAL
WINAPI
LocalAlloc(
    UINT uFlags,
    UINT uBytes
    );

WINBASEAPI
HLOCAL
WINAPI
LocalFree(
    HLOCAL hMem
    );

#define CopyMemory RtlCopyMemory
#define FillMemory RtlFillMemory
#define ZeroMemory RtlZeroMemory


//
// Now we can bring in the WINDBG extension
// definitions...
//
#undef RC_INVOKED
#include <wdbgexts.h>	// located in MSTOOLS\H
						// in the Win32 SDK
//
// END OF MAGICKAL HEADER SEQUENCE
/////////////////////////////////////////////////////

//
// Other header files...
//
#include <stdlib.h>
#include <string.h>

//
// Driver-specific header file...
//
#include "driver.h"

//
// Global variables
//
static EXT_API_VERSION
	ApiVersion = { 3, 5, EXT_API_VERSION_NUMBER, 0 };

static WINDBG_EXTENSION_APIS ExtensionApis;

static USHORT SavedMajorVersion;
static USHORT SavedMinorVersion;

//
// Forward declarations of local functions
//
// (None)


//++
// Function:
//		WinDbgExtensionDllInit
//
// Description:
//		This function performs some required
//		initialization for extension DLL.  
//
// Arguments:
//		(None)
//
// Return Value:
//		(None)
//--
extern "C" VOID
WinDbgExtensionDllInit(
    PWINDBG_EXTENSION_APIS lpExtensionApis,
    USHORT MajorVersion,
    USHORT MinorVersion
    )
{
	//
	// Save pointer to the table of utility
	// functions provided by WINDBG
	//
	// Functions defined in WDBGEXTS.H assume
	// that the saved pointer has exactly this
	// name. Don't change it !
	//
    ExtensionApis = *lpExtensionApis;

	//
	// NOTE: For a list of the helper functions
	// and descriptions of what they do, see the
	// online help in WINDBG itself. From the
	// Contents screen, click on the KD button,
	// then click on the "Creating Extensions"
	// topic. Scroll about half way down this
	// topic and you'll find a list of helper
	// functions. Click on the name of a function
	// to see its prototype and a description.
	//

	//
	// Save information about the version of
	// NT that's being debugged
	//
    SavedMajorVersion = MajorVersion;
    SavedMinorVersion = MinorVersion;

    return;
}

//++
// Function:
//		CheckVersion
//
// Description:
//		This function gets called before
//		each extension command is executed.
//		Its job is to complain if the version
//		of NT being debugged doesn't match the
//		version of the extension DLL.
//
// Arguments:
//		(None)
//
// Return Value:
//		(None)
//--
extern "C" VOID
CheckVersion(
    VOID
    )
{
    //
    // Your version-checking code goes here
    //
	dprintf( 
		"CheckVersion called... [%1x;%d]\n",
		SavedMajorVersion,
		SavedMinorVersion
		);
}

//++
// Function:
//		ExtensionApiVersion
//
// Description:
//		This function returns the version
//		of the extension DLL itself.  
//
// Arguments:
//		(None)
//
// Return Value:
//		Pointer to a version structure
//--
extern "C" LPEXT_API_VERSION
ExtensionApiVersion(
    VOID
    )
{
    return &ApiVersion;
}

//
// The remainder of the DLL contains extension
// commands. Define each one using:
//
//		DECLARE_API(command_name) 
//
// This macro comes from MSTOOLS\H\\WDBGEXTS.H (in
// the Win32 SDK) and it declares the command 
// function as:
//
// VOID
// command_name(
//	IN HANDLE hCurrentProcess,
//	IN HANDLE hCurrentThread,
//	IN ULONG  dwCurrentPc,
//	IN ULONG  dwProcessor,
//	IN PCSTR  args
//	);
//
// NOTE: COMMAND-NAMES *MUST* BE
//		 ENTIRELY LOWER-CASE !


//++
// Command:
//		devext <addr of Device obj>
//
// Description:
//		This command formats and prints
//		the contents of a Device Extension 
//
// Arguments:
//		(See above)
//
// Return Value:
//		(None)
//--
extern "C" DECLARE_API(devext)
{
	DWORD dwBytesRead;
	DWORD dwAddress;

	PDEVICE_OBJECT pDevObj;
	PDEVICE_EXTENSION pDevExt; 

	//
	// Get memory for Device object buffer
	//
	if(( pDevObj = (PDEVICE_OBJECT)malloc( 
				sizeof( DEVICE_OBJECT ))) == NULL )
	{
		dprintf( "Can't allocate buffer.\n" );
		return;
	}

	//
	// Get address of Device object from
	// command line
	//
	dwAddress = GetExpression( args );

	if( !ReadMemory(
			dwAddress,
			pDevObj,
			sizeof( DEVICE_OBJECT ),
			&dwBytesRead ))
	{
		dprintf( "Can't get Device object.\n ");
		free( pDevObj );
		return;
	}

	//
	// Get memory for Device Extension buffer
	//
	if( (pDevExt = (PDEVICE_EXTENSION)malloc( 
				sizeof( DEVICE_EXTENSION ))) == NULL )
	{
		dprintf( "Can't allocate buffer.\n" );
		free( pDevObj );
		return;
	}

	//
	// Use Device object to get Device Extension
	//
	if( !ReadMemory(
			(DWORD)pDevObj->DeviceExtension,
			pDevExt,
			sizeof( DEVICE_EXTENSION ),
			&dwBytesRead ))
	{
		dprintf( "Can't get Device Extension.\n ");
		free( pDevExt );
		free( pDevObj );
		return;
	}

	//
	// Print out interesting values
	//
	dprintf(
		"BytesRequested: %d\n"
		"BytesRemaining: %d\n"
		"PollingInterval: %d\n"
		"DeviceObject: %8x\n",

		pDevExt->maxXferCount,
		pDevExt->maxXferCount - pDevExt->xferCount,
		pDevExt->pollingInterval.LowPart,
		pDevExt->pDevice
		);

	//
	// Clean up and go
	//
	free( pDevExt );
	free( pDevObj );
}

//++
// Command:
//		printargs
//
// Description:
//		This function just illustrates
//		how to get at the arguments passed
//		to a command.  
//
// Arguments:
//		(See above)
//
// Return Value:
//		(None)
//--
extern "C" DECLARE_API(printargs)
{
	dprintf( 
		"Process:\t%8x\n"
		"Thread:\t%8x \n"
		"PC:\t%8x\n"
		"CPU:\t%8x\n"
		"Args: %s\n", 

		hCurrentProcess,
		hCurrentThread,
		dwCurrentPc,    
		dwProcessor,
		args
		);
}