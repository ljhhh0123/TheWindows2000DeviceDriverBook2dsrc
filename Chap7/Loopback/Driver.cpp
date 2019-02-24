//
// Driver.c - Chapter 7 - Loopback Driver
//
// Copyright (C) 2000 by Jerry Lozano
//

#include "Driver.h"

// Forward declarations
//
static NTSTATUS CreateDevice (
		IN PDRIVER_OBJECT	pDriverObject,
		IN ULONG			DeviceNumber	);

static VOID DriverUnload (
		IN PDRIVER_OBJECT	pDriverObject	);

static NTSTATUS DispatchCreate (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			);

static NTSTATUS DispatchClose (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			);

static NTSTATUS DispatchWrite (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			);

static NTSTATUS DispatchRead (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			);


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

	// If this driver controlled real hardware,
	// code would be placed here to locate it.
	// Using IoReportDetectedDevice, the ports,
	// IRQs, and DMA channels would be "marked"
	// as "in use" and under the control of this driver.
	// This Loopback driver has no HW, so...

	// Announce other driver entry points
	pDriverObject->DriverUnload = DriverUnload;
	// This includes Dispatch routines for Create, Write & Read
	pDriverObject->MajorFunction[IRP_MJ_CREATE] =
				DispatchCreate;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] =
				DispatchClose;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] =
				DispatchWrite;
	pDriverObject->MajorFunction[IRP_MJ_READ] =
				DispatchRead;
	
	// For each physical or logical device detected
	// that will be under this Driver's control,
	// a new Device object must be created.
	status =
		CreateDevice(pDriverObject, ulDeviceNumber);
	// This call would be repeated until all devices are created
	//ulDeviceNumber++;
	//status =
	//	CreateDevice(pDriverObject, ulDeviceNumber);

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
	CUString devName("\\Device\\LOOPBACK");	// for "loopback" device
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

	// Announce that we will be working with a copy of the user's buffer
	pDevObj->Flags |= DO_BUFFERED_IO;

	// Initialize the Device Extension
	pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;

	pDevExt->pDevice = pDevObj;	// back pointer
	pDevExt->DeviceNumber = ulDeviceNumber;
	pDevExt->ustrDeviceName = devName;
	pDevExt->deviceBuffer = NULL;
	pDevExt->deviceBufferSize = 0;

	// Form the symbolic link name
	CUString symLinkName("\\??\\LBK");
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
		// Free up any buffer still held by this device
		if (pDevExt->deviceBuffer != NULL) {
			ExFreePool(pDevExt->deviceBuffer);
			pDevExt->deviceBuffer = NULL;
			pDevExt->deviceBufferSize = 0;
		}
		// DevExt also holds the symbolic link name
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
// Function:	DispatchCreate
//
// Description:
//		Handles call from Win32 CreateFile request
//		For loopback driver, does nothing
//
// Arguments:
//		pDevObj - Passed from I/O Manager
//		pIrp - Passed from I/O Manager
//
// Return value:
//		NTSTATUS - success or failuer code
//--

NTSTATUS DispatchCreate (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			) {

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;	// no bytes xfered
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	return STATUS_SUCCESS;
}

//++
// Function:	DispatchClose
//
// Description:
//		Handles call from Win32 CreateHandle request
//		For loopback driver, frees any buffer
//
// Arguments:
//		pDevObj - Passed from I/O Manager
//		pIrp - Passed from I/O Manager
//
// Return value:
//		NTSTATUS - success or failuer code
//--

NTSTATUS DispatchClose (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			) {
	
	// Dig out the Device Extension from the Device object
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;
	if (pDevExt->deviceBuffer != NULL) {
		ExFreePool(pDevExt->deviceBuffer);
		pDevExt->deviceBuffer = NULL;
		pDevExt->deviceBufferSize = 0;
	}
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;	// no bytes xfered
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	return STATUS_SUCCESS;
}

//++
// Function:	DispatchWrite
//
// Description:
//		Handles call from Win32 WriteFile request
//		For loopback driver, allocates new pool buffer
//			then xfers user buffer to pool buffer
//
// Arguments:
//		pDevObj - Passed from I/O Manager
//		pIrp - Passed from I/O Manager
//
// Return value:
//		NTSTATUS - success or failuer code
//--

NTSTATUS DispatchWrite (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			) {
	
	
	NTSTATUS status = STATUS_SUCCESS;
	PVOID userBuffer;
	ULONG xferSize;
	// The stack location contains the user buffer info
	PIO_STACK_LOCATION pIrpStack =
		IoGetCurrentIrpStackLocation( pIrp );
	// Dig out the Device Extension from the Device object
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;
	// Since we processing a new Write request,
	// free up any old buffer
	if (pDevExt->deviceBuffer != NULL) {
		ExFreePool(pDevExt->deviceBuffer);
		pDevExt->deviceBuffer = NULL;
		pDevExt->deviceBufferSize = 0;
	}
	// Determine the length of the request
	xferSize = pIrpStack->Parameters.Write.Length;
	// Obtain user buffer pointer
	userBuffer = pIrp->AssociatedIrp.SystemBuffer;

	// Allocate the new buffer
	pDevExt->deviceBuffer =
		ExAllocatePool( PagedPool, xferSize );
	if (pDevExt->deviceBuffer == NULL) {
		// buffer didn't allocate???
		status = STATUS_INSUFFICIENT_RESOURCES;
		xferSize = 0;
	} else {
		// copy the buffer
		pDevExt->deviceBufferSize = xferSize;
		RtlCopyMemory( pDevExt->deviceBuffer, userBuffer,
							xferSize );
	}

	// Now complete the IRP
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = xferSize;	// bytes xfered
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	return status;
}

//++
// Function:	DispatchRead
//
// Description:
//		Handles call from Win32 ReadFile request
//		For loopback driver, xfers pool buffer to user
//
// Arguments:
//		pDevObj - Passed from I/O Manager
//		pIrp - Passed from I/O Manager
//
// Return value:
//		NTSTATUS - success or failuer code
//--

NTSTATUS DispatchRead (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			) {
	
	
	NTSTATUS status = STATUS_SUCCESS;
	PVOID userBuffer;
	ULONG xferSize;
	// The stack location contains the user buffer info
	PIO_STACK_LOCATION pIrpStack =
		IoGetCurrentIrpStackLocation( pIrp );
	// Dig out the Device Extension from the Device object
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;
	// Determine the length of the request
	xferSize = pIrpStack->Parameters.Read.Length;
	// Obtain user buffer pointer
	userBuffer = pIrp->AssociatedIrp.SystemBuffer;

	// Don't transfer more than the user's request
	xferSize = (xferSize < pDevExt->deviceBufferSize) ?
				xferSize : pDevExt->deviceBufferSize;
	// Now copy the pool buffer into user space
	RtlCopyMemory( userBuffer, pDevExt->deviceBuffer,
							xferSize );
	// Free the temporary pool buffer
	ExFreePool( pDevExt->deviceBuffer );
	pDevExt->deviceBuffer = NULL;
	pDevExt->deviceBufferSize = 0;

	// Now complete the IRP
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = xferSize;	// bytes xfered
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	return status;
}

