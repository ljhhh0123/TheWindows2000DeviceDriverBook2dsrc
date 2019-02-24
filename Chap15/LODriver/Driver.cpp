//
// Driver.c - LoDriver Driver
// Simplisitc driver example has an arbitrarily limited
// maximum Write transfer size of 16 bytes.
// Depends on higher filter driver, HIFILTER, to
// eliminate this restriction.
//
// Copyright (C) 2000 by Jerry Lozano
//

#include "Driver.h"

// Forward declarations
//
NTSTATUS AddDevice (
			IN PDRIVER_OBJECT pDriverObject,
			IN PDEVICE_OBJECT pdo	);

NTSTATUS DispPnp(	IN PDEVICE_OBJECT pDO,
					IN PIRP pIrp );

NTSTATUS PassDownPnP( IN PDEVICE_OBJECT pDO,
					IN PIRP pIrp );

NTSTATUS HandleStartDevice(	IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp );
NTSTATUS HandleStopDevice(	IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp );

NTSTATUS HandleRemoveDevice(IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp );

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

static NTSTATUS DispatchDeviceControl (
		IN PDEVICE_OBJECT pDevObj,
		IN PIRP			  pIrp				);

//++
// Function:	DriverEntry
//
// Description:
//		Initializes the driver.
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
#if DBG>=2
	DbgPrint("LODRIVER: DriverEntry\n");
#endif
	ULONG ulDeviceNumber = 0;
	NTSTATUS status = STATUS_SUCCESS;

	// Announce other driver entry points
	pDriverObject->DriverUnload = DriverUnload;

	// Initialize Event logging
	InitializeEventLog(pDriverObject);

	// Announce the PNP AddDevice entry point
	pDriverObject->DriverExtension->AddDevice =
				AddDevice;

	// Announce the PNP Major Function entry point
	pDriverObject->MajorFunction[IRP_MJ_PNP] =
				DispPnp;

	// This includes Dispatch routines for Create, Write & Read
	pDriverObject->MajorFunction[IRP_MJ_CREATE] =
				DispatchCreate;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] =
				DispatchClose;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] =
				DispatchWrite;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
				DispatchDeviceControl;
	
	ReportEvent(
		LOG_LEVEL_DEBUG,
		MSG_DRIVER_STARTING,
		ERRORLOG_INIT,
		(PVOID)pDriverObject,
		NULL,					// No IRP
		NULL, 0,				// No dump data
		NULL, 0 );				// No strings

	// Notice that no device objects are created by DriverEntry.
	// Instead, we await the PnP call to AddDevice

	return status;
}

//++
// Function:	AddDevice
//
// Description:
//	Called by the PNP Manager when a new device is
//	detected on a bus.  The responsibilities include
//	creating an FDO, device name, and symbolic link.
//
// Arguments:
//	pDriverObject - Passed from PNP Manager
//	pdo		    - pointer to Physcial Device Object
//				 passed from PNP Manager
//
// Return value:
//	NTSTATUS signaling success or failure
//--
NTSTATUS AddDevice (
			IN PDRIVER_OBJECT pDriverObject,
			IN PDEVICE_OBJECT pdo	) {
	NTSTATUS status;
	PDEVICE_OBJECT pfdo;
	PDEVICE_EXTENSION pDevExt;
	static int ulDeviceNumber = 0;
#if DBG>=2
	DbgPrint("LODRIVER: AddDevice; current DeviceNumber = %d\n",
				ulDeviceNumber);
#endif
	
	// Form the internal Device Name
	CUString devName("\\Device\\LODRIVER"); // for "LODriver" dev
	devName += CUString(ulDeviceNumber);

	// Now create the device
	status =
		IoCreateDevice( pDriverObject,
						sizeof(DEVICE_EXTENSION),
						&(UNICODE_STRING)devName,
						FILE_DEVICE_UNKNOWN,
						0, FALSE,
						&pfdo );
	if (!NT_SUCCESS(status))
		return status;

	// TODO: Set either DO_BUFFERED_IO or DO_DIRECT_IO
	// Choose to use BUFFERED_IO
	pfdo->Flags |= DO_BUFFERED_IO;

	// Initialize the Device Extension
	pDevExt = (PDEVICE_EXTENSION)pfdo->DeviceExtension;
	pDevExt->pDevice = pfdo;	// back pointer
	pDevExt->DeviceNumber = ulDeviceNumber;
	pDevExt->ustrDeviceName = devName;
	pDevExt->state = Stopped;

	// Initialize Event Logging counters:
	pDevExt->IrpRetryCount = 0;
	pDevExt->IrpSequenceNumber = 0;

	// Pile this new fdo on top of the existing lower stack
	pDevExt->pLowerDevice =		// downward pointer
		IoAttachDeviceToDeviceStack( pfdo, pdo);

	// This is where the upper pointer would be initialized.
	// Notice how the cast of the lower device's extension
	// must be known in order to find the offset pUpperDevice.
	// PLOWER_DEVEXT pLowerDevExt = (PLOWER_DEVEXT)
	//		pDevExt->pLowerDevice->DeviceExtension;
	// pLowerDevExt->pUpperDevice = pfdo;

	// Form the symbolic link name
	CUString symLinkName("\\??\\LODRV");
	symLinkName += CUString(ulDeviceNumber+1);	// 1 based
	pDevExt->ustrSymLinkName = symLinkName;

	// Now create the link name
	status = 
		IoCreateSymbolicLink( &(UNICODE_STRING)symLinkName,
						  &(UNICODE_STRING)devName );
	if (!NT_SUCCESS(status)) {
		// if it fails now, must delete Device object
		IoDeleteDevice( pfdo );
		return status;
	}

    //  Clear the Device Initializing bit since the FDO was created
    //  outside of DriverEntry.
    pfdo->Flags &= ~DO_DEVICE_INITIALIZING;

	// Put an add device message in the Event Log
	PWSTR strings[1] = {(PWSTR)pDevExt->ustrSymLinkName,};
	ReportEvent(
		LOG_LEVEL_DEBUG,
		MSG_ADDING_DEVICE,
		ERRORLOG_PNP,
		(PVOID)pfdo,
		NULL,					// No IRP
		NULL, 0,				// No dump data
		// String data: External device name
		strings, 1 );

	// Made it
	ulDeviceNumber++;
	return STATUS_SUCCESS;
}


NTSTATUS DispPnp(	IN PDEVICE_OBJECT pDO,
					IN PIRP pIrp ) {
	// obtain current IRP stack location
	PIO_STACK_LOCATION pIrpStack;
	pIrpStack = IoGetCurrentIrpStackLocation( pIrp );
#if DBG>=2
	DbgPrint("LODRIVER: Received PNP IRP: %d\n",
				pIrpStack->MinorFunction);
#endif

	switch (pIrpStack->MinorFunction) {
	case IRP_MN_START_DEVICE:
		return HandleStartDevice(pDO, pIrp );
	case IRP_MN_STOP_DEVICE:
		return HandleStopDevice( pDO, pIrp );
	case IRP_MN_REMOVE_DEVICE:
		return HandleRemoveDevice( pDO, pIrp );
	default:
		// if not supported here, just pass it down
		return PassDownPnP(pDO, pIrp);
	}

	// all paths from the switch statement will "return"
	// the results of the handler invoked
}

NTSTATUS PassDownPnP( IN PDEVICE_OBJECT pDO,
					IN PIRP pIrp ) {
	IoSkipCurrentIrpStackLocation( pIrp );
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDO->DeviceExtension;
	return IoCallDriver(pDevExt->pLowerDevice, pIrp);
}

NTSTATUS HandleStartDevice(	IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp ) {
	// The stack location contains the Parameter info
	PIO_STACK_LOCATION pIrpStack =
		IoGetCurrentIrpStackLocation( pIrp );
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDO->DeviceExtension;
#if DBG>=2
	DbgPrint("LODRIVER: StartDevice, Symbolic device #%d\n",
				pDevExt->DeviceNumber+1);
#endif

	// Put a start device message in the Event Log
	ReportEvent(
		LOG_LEVEL_DEBUG,
		MSG_STARTING_DEVICE,
		ERRORLOG_PNP,
		(PVOID)pDO,
		pIrp,					// IRP
		NULL, 0,				// No dump data
		NULL, 0 );				// No strings

	pDevExt->state = Started;

	return PassDownPnP(pDO, pIrp);

}

NTSTATUS HandleStopDevice(	IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp ) {
#if DBG>=1
	DbgPrint("LODRIVER: StopDevice Handler\n");
#endif
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDO->DeviceExtension;

	// Put a stop device message in the Event Log
	ReportEvent(
		LOG_LEVEL_DEBUG,
		MSG_STOPPING_DEVICE,
		ERRORLOG_PNP,
		(PVOID)pDO,
		pIrp,					// IRP
		NULL, 0,				// No dump data
		NULL, 0 );				// No strings

	pDevExt->state = Stopped;

	return PassDownPnP(pDO, pIrp);
}

NTSTATUS HandleRemoveDevice(IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp ) {
#if DBG>=1
	DbgPrint("LODRIVER: RemoveDevice Handler\n");
#endif
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDO->DeviceExtension;

	// This will yield the symbolic link name
	UNICODE_STRING pLinkName =
		pDevExt->ustrSymLinkName;
	// ... which can now be deleted
	IoDeleteSymbolicLink(&pLinkName);
#if DBG>=1
	DbgPrint("LODRIVER: Symbolic Link MPMP%d Deleted\n",
				pDevExt->DeviceNumber+1);
#endif
	
	// Delete the device
	IoDeleteDevice( pDO );

	// Put a remove device message in the Event Log
	ReportEvent(
		LOG_LEVEL_DEBUG,
		MSG_REMOVING_DEVICE,
		ERRORLOG_PNP,
		(PVOID)pDO,
		pIrp,					// IRP
		NULL, 0,				// No dump data
		NULL, 0 );				// No strings

	pDevExt->state = Removed;
	return PassDownPnP( pDO, pIrp );
}

//++
// Function:	DriverUnload
//
// Description:
//		For this driver, merely logs an event
//
// Arguments:
//		pDriverObject - Passed from I/O Manager
//
// Return value:
//		None
//--

VOID DriverUnload (
		IN PDRIVER_OBJECT	pDriverObject	) {
#if DBG>=1
	DbgPrint("LODRIVER: DriverUnload\n");
#endif

	// Put a shutdown message in the Event Log
	ReportEvent(
		LOG_LEVEL_DEBUG,
		MSG_DRIVER_STOPPING,
		ERRORLOG_UNLOAD,
		(PVOID)pDriverObject,
		NULL,					// No IRP
		NULL, 0,				// No dump data
		NULL, 0 );				// No strings

}

//++
// Function:	DispatchCreate
//
// Description:
//		Handles call from Win32 CreateFile request
//		For this driver, does nothing
//
// Arguments:
//		pDevObj - Passed from I/O Manager
//		pIrp - Passed from I/O Manager
//
// Return value:
//		NTSTATUS - success or failure code
//--

NTSTATUS DispatchCreate (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			) {
#if DBG>=1
	DbgPrint("LODRIVER: DispatchCreate\n");
#endif
	// Put an open handle message in the Event Log
	ReportEvent(
		LOG_LEVEL_DEBUG,
		MSG_OPENING_HANDLE,
		ERRORLOG_DISPATCH,
		(PVOID)pDevObj,
		pIrp,					// IRP
		NULL, 0,				// No dump data
		NULL, 0 );				// No strings

	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;
	NTSTATUS status = STATUS_SUCCESS;
	if (pDevExt->state != Started)
		status = STATUS_DEVICE_REMOVED;

	
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;	// no bytes xfered
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	return status;
}

//++
// Function:	DispatchClose
//
// Description:
//		Handles call from Win32 CreateHandle request
//		For this driver, does nothing
//
// Arguments:
//		pDevObj - Passed from I/O Manager
//		pIrp - Passed from I/O Manager
//
// Return value:
//		NTSTATUS - success or failure code
//--

NTSTATUS DispatchClose (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			) {
#if DBG>=1
	DbgPrint("LODRIVER: DispatchClose\n");
#endif
	// Put a close handle message in the Event Log
	ReportEvent(
		LOG_LEVEL_DEBUG,
		MSG_CLOSING_HANDLE,
		ERRORLOG_DISPATCH,
		(PVOID)pDevObj,
		pIrp,					// IRP
		NULL, 0,				// No dump data
		NULL, 0 );				// No strings
	
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;	// no bytes xfered
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	return STATUS_SUCCESS;
}

//++
// Function:	DispatchCancel
//
// Description:
//		Handles canceled IRP
//
// Arguments:
//		pDevObj - Passed from I/O Manager
//		pIrp - Passed from I/O Manager
//
// Return value:
//		NTSTATUS - success or failuer code
//--

VOID DispatchCancel (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			) {

#if DBG>=1
	DbgPrint("LODRIVER: IRP Canceled\n");
#endif
	
	// TODO:  Perform IRP cleanup work

	// Just complete the IRP
	pIrp->IoStatus.Status = STATUS_CANCELLED;
	pIrp->IoStatus.Information = 0;	// bytes xfered
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	IoStartNextPacket( pDevObj, TRUE );
}


//++
// Function:	DispatchWrite
//
// Description:
//		Handles call from Win32 WriteFile request
//		For LODRIVER, puts an arbitray restriction
//			on size of Write (16 - MAX_LO_TRANSFER_SIZE)
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
#if DBG>=1
	DbgPrint("LODRIVER: Write Operation requested (DispatchWrite)");
#endif

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
	xferSize = pIrpStack->Parameters.Write.Length;
	// Obtain user buffer pointer
	userBuffer = pIrp->AssociatedIrp.SystemBuffer;
#if DBG>=1
	DbgPrint("%d bytes\n", xferSize);
#endif

	// Put a write request message in the Event Log
	ReportEvent(
		LOG_LEVEL_DEBUG,
		MSG_WRITE_REQUEST,
		ERRORLOG_DISPATCH,
		(PVOID)pDevObj,
		pIrp,					// IRP
		// Dump data is the data xfer request
		&xferSize, 1,
		NULL, 0 );				// No strings

	if (xferSize > MAX_LO_TRANSFER_SIZE) {
		xferSize = 0;	// You get nothing if you ask for too much
		status = STATUS_INVALID_BUFFER_SIZE;
	};	// else - thank you very much
	// Note that we don't actually keep the data...
	// or do anything with it for that matter...
	// Not really the point of this exercise...

	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = xferSize;

	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	return status;
}

//++
// Function:	DispatchDeviceControl
//
// Description:
//		Handles call from Win32 DeviceControl request
//		For LODRIVER, puts an arbitray restriction
//			on size of Write (16 - MAX_LO_TRANSFER_SIZE)
//
// Arguments:
//		pDevObj - Passed from I/O Manager
//		pIrp - Passed from I/O Manager
//
// Return value:
//		NTSTATUS - success or failuer code
//--

NTSTATUS DispatchDeviceControl (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			) {
#if DBG>=1
	DbgPrint("LODRIVER: DeviceIoControl Operation requested (DispatchWrite)\n");
#endif

	NTSTATUS status = STATUS_SUCCESS;

	PIO_STACK_LOCATION pIrpStack =
			IoGetCurrentIrpStackLocation( pIrp );

	PBUFFER_SIZE_INFO pBufferInfo;
	ULONG xferSize = 0;

	// Here is the implementation
	if (pIrpStack->Parameters.DeviceIoControl.IoControlCode
					== IOCTL_GET_MAX_BUFFER_SIZE ) {
		// The buffer passed by the user (by mutual
		// agreement) is treated as BUFFER_SIZE_INFO type.
		pBufferInfo = (PBUFFER_SIZE_INFO)
			pIrp->AssociatedIrp.SystemBuffer;
		pBufferInfo->MaxWriteLength = MAX_LO_TRANSFER_SIZE;
		pBufferInfo->MaxReadLength = 0;

			// Put a write request message in the Event Log
		ReportEvent(
			LOG_LEVEL_DEBUG,
			MSG_DEVICE_CONTROL_REQUEST,
			ERRORLOG_DISPATCH,
			(PVOID)pDevObj,
			pIrp,					// IRP
			// Dump data is the Buffer Info data
			(ULONG*)pBufferInfo, 2,
			NULL, 0 );				// No strings

		// Complete the IRP by announcing the size of
		// the returned BUFFER_SIZE_INFO information.
		xferSize = 	sizeof(BUFFER_SIZE_INFO);

	} else
		// Not a recognized DeviceIoControl request
		status = STATUS_INVALID_DEVICE_REQUEST;

	pIrp->IoStatus.Information = xferSize;
	pIrp->IoStatus.Status = status;

	IoCompleteRequest( pIrp, IO_NO_INCREMENT );

	return status;
}


