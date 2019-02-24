//
// Driver.c - Chapter 17 - Crasher Driver
//
// Copyright (C) 2000 by Jerry Lozano
//

#include "Driver.h"

static ULONG CrashPoint;	// Determines where the
							// driver will crash.

// Forward declarations
//
static NTSTATUS CreateDevice (
		IN PDRIVER_OBJECT	pDriverObject,
		IN ULONG			DeviceNumber	);

static VOID DriverUnload (
		IN PDRIVER_OBJECT	pDriverObject	);

VOID
TryToCrash(
	IN ULONG CurrentLocation 	);

//++
// Function:	DriverEntry
//
// Description:
//		Initializes the driver, locating and claiming
//		hardware resources.  Creates the kernel objects
//		needed to process I/O requests.
//
// Arguments:
//		pDriverObject - Passed from I/O Manager
//		pRegistryPath - UNICODE_STRING pointer to
//						registry info (service key)
//						for this driver
//
// Return value:
//		NTSTATUS signaling success or failure
//--
extern "C" NTSTATUS DriverEntry (
			IN PDRIVER_OBJECT pDriverObject,
			IN PUNICODE_STRING pRegistryPath	) {
	ULONG ulDeviceNumber = 0;
	NTSTATUS status;

	// Set the crash point with a hardcoded number:
	CrashPoint = 1;

	// If this driver controlled real hardware,
	// code would be placed here to locate it.
	// Using IoReportResourceUsage, the ports,
	// IRQs, and DMA channels would be "marked"
	// as "in use" and under the control of this driver.
	// This minimal driver has no HW, so...

	// Announce other driver entry points
	pDriverObject->DriverUnload = DriverUnload;
	// Over time, the MajorFunction array will be filled
	
	// For each physical or logical device detected
	// that will be under this Driver's control,
	// a new Device object must be created.
	status =
		CreateDevice(pDriverObject, ulDeviceNumber);
	// This call would be repeated until all devices are created
	ulDeviceNumber++;
	status =
		CreateDevice(pDriverObject, ulDeviceNumber);

	return status;
}

//++
// Function:	CreateDevice
//
// Description:
//		Adds a new device
//
// Arguments:
//		pDriverObject - Passed from I/O Manager
//		ulDeviceNumber - Logical device number (zero-based)
//
// Return value:
//		None
//--
NTSTATUS CreateDevice (
		IN PDRIVER_OBJECT	pDriverObject,
		IN ULONG			ulDeviceNumber	) {

	NTSTATUS status;
	PDEVICE_OBJECT pDevObj;
	PDEVICE_EXTENSION pDevExt;
	
	// Form the internal Device Name
	CUString devName("\\Device\\CRASHER");	// for "crasher" device
	devName += CUString(ulDeviceNumber);

	// Now create the device
	status =
		IoCreateDevice( pDriverObject,
						sizeof(DEVICE_EXTENSION),
						&(UNICODE_STRING)devName,
						FILE_DEVICE_UNKNOWN,
						0, TRUE,
						&pDevObj );
	if (!NT_SUCCESS(status))
		return status;

	// Initialize the Device Extension
	pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	pDevExt->pDevice = pDevObj;	// back pointer
	pDevExt->DeviceNumber = ulDeviceNumber;
	pDevExt->ustrDeviceName = devName;

	// Form the symbolic link name
	CUString symLinkName("\\??\\CRASH");
	symLinkName += CUString(ulDeviceNumber+1);	// 1 based
	pDevExt->ustrSymLinkName = symLinkName;

	// Now create the link name
	status = 
		IoCreateSymbolicLink( &(UNICODE_STRING)symLinkName,
							  &(UNICODE_STRING)devName );
	if (!NT_SUCCESS(status)) {
		// if it fails now, must delete Device object
		IoDeleteDevice( pDevObj );
		return status;
	}

	// Force a blue screen of death:
	TryToCrash( 1 );

	// Made it
	return STATUS_SUCCESS;
}

//++
// Function:	DriverUnload
//
// Description:
//		Stops & Deletes devices controlled by this driver.
//		Stops interrupt processing (if any)
//		Releases kernel resources consumed by driver
//
// Arguments:
//		pDriverObject - Passed from I/O Manager
//
// Return value:
//		None
//--

VOID DriverUnload (
		IN PDRIVER_OBJECT	pDriverObject	) {

	PDEVICE_OBJECT	pNextObj;

	// Loop through each device controlled by Driver
	pNextObj = pDriverObject->DeviceObject;
	while (pNextObj != NULL) {
		// Dig out the Device Extension from the
		// Device Object
		PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
			pNextObj->DeviceExtension;
		// This will yield the symbolic link name
		UNICODE_STRING pLinkName =
			pDevExt->ustrSymLinkName;
		// ... which can now be deleted
		IoDeleteSymbolicLink(&pLinkName);
		// a little trickery... 
		// we need to delete the device object, BUT
		// the Device object is pointed to by pNextObj
		// If we delete the device object first,
		// we can't traverse to the next Device in the list
		// Rather than create another pointer, we can
		// use the DeviceExtension's back pointer to the device
		// So, first update the next pointer...
		pNextObj = pNextObj->NextDevice;
		// then delete the device using the Extension
		IoDeleteDevice( pDevExt->pDevice );
	}
	// Finally, hardware that was allocated in DriverEntry
	// would be released here using
	// IoReportResourceUsage
}

//++
// Function:
//		TryToCrash
//
// Description:
//		If this function is called from the place
//		in the driver where a crash should occur,
//		generate an access violation. Otherwise,
//		the call is a no-op. 
//
// Arguments:
//		Indicator of place in driver where this
//		function is being called.
//
// Return Value:
//		(None)
//--
VOID
TryToCrash(
	IN ULONG CurrentLocation
	)
{
	UCHAR c;

	//
	// If this is the place to crash, generate
	// an access violation
	//
	if( CurrentLocation == CrashPoint )
	{
		KdPrint(( "About to generate crash...\n" ));
		c = *(PUCHAR)0;
	}
}

