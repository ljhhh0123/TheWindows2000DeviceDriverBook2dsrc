//
// Driver.c - HiFilter Driver
// Modifies behavior of LODriver to allow for unlimited
// Write transfer sizes.
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

VOID GetBufferLimits(
				IN PDEVICE_OBJECT pTargetDevObj,
				OUT PBUFFER_SIZE_INFO pBufferInfo );

static VOID DriverUnload (
		IN PDRIVER_OBJECT	pDriverObject	);

static NTSTATUS OverriddenDispatchWrite (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			);

static NTSTATUS OverriddenDispatchIoControl (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			);

static NTSTATUS DispatchPassThru (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			);

NTSTATUS GenericCompletion(
				IN PDEVICE_OBJECT pDevObj,
				IN PIRP pIrp,
				IN PVOID pContext );

NTSTATUS WriteCompletion(
				IN PDEVICE_OBJECT pDevObj,
				IN PIRP pIrp,
				IN PVOID pContext );


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
#if DBG>=1
	DbgPrint("HIFILTER: DriverEntry\n");
#endif
	ULONG ulDeviceNumber = 0;
	NTSTATUS status = STATUS_SUCCESS;

	// Assume (initially) nothing is overridden
	for (int i=0; i<=IRP_MJ_MAXIMUM_FUNCTION; i++)
		if (i!=IRP_MJ_POWER)
			pDriverObject->MajorFunction[i] = DispatchPassThru;

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

	// Export the overridden MajorFunctions
	pDriverObject->MajorFunction[ IRP_MJ_WRITE ] =
			OverriddenDispatchWrite;
	pDriverObject->MajorFunction[ IRP_MJ_DEVICE_CONTROL ] =
			OverriddenDispatchIoControl;
	
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
//	creating an FDO, and duplicating the lower device's
//	characteristics.
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
	PDEVICE_OBJECT pFilterDevObj;
	PDEVICE_EXTENSION pDevExt;
#if DBG>=1
	DbgPrint("HIFILTER: AddDevice\n");
#endif
	
	// Create the un-named filter device
	status =
		IoCreateDevice( pDriverObject,
					sizeof(DEVICE_EXTENSION),
					NULL,	// no name
					FILE_DEVICE_UNKNOWN,
					0, TRUE,
					&pFilterDevObj );
	if (!NT_SUCCESS(status))
		return status;

	// Initialize the Device Extension
	pDevExt = (PDEVICE_EXTENSION)pFilterDevObj->DeviceExtension;
	pDevExt->pDevice = pFilterDevObj;	// back pointer
	pDevExt->state = Stopped;

	// Pile this new filter on top of the existing target
	pDevExt->pTargetDevice =		// downward pointer
		IoAttachDeviceToDeviceStack( pFilterDevObj, pdo);

	// Copy the characteristics of the target into the
	//		the new filter device object
	pFilterDevObj->DeviceType =
			pDevExt->pTargetDevice->DeviceType;
	pFilterDevObj->Characteristics =
			pDevExt->pTargetDevice->Characteristics;
	pFilterDevObj->Flags |=
			( pDevExt->pTargetDevice->Flags &
				( DO_BUFFERED_IO | DO_DIRECT_IO |
				  DO_POWER_INRUSH | DO_POWER_PAGABLE));

	// Initialize Event Logging counters:
	pDevExt->IrpRetryCount = 0;
	pDevExt->IrpSequenceNumber = 0;

	// Explore the limitations of the target device's
	//	 buffer.  Save the results in the bufferInfo struct
	GetBufferLimits( pDevExt->pTargetDevice,
				   	&pDevExt->bufferInfo );

    //  Clear the Device Initializing bit since the FDO was created
    //  outside of DriverEntry.
    pFilterDevObj->Flags &= ~DO_DEVICE_INITIALIZING;

	// Made it
	return STATUS_SUCCESS;
}

VOID GetBufferLimits(
				IN PDEVICE_OBJECT pTargetDevObj,
				OUT PBUFFER_SIZE_INFO pBufferInfo ) {
	KEVENT keIoctlComplete;
	IO_STATUS_BLOCK Iosb;
	PIRP pIrp;

#if DBG>=1
	DbgPrint("HIFILTER: GetBufferLimits\n");
#endif
	// Initialize the event that is signaled when the
	// IOCTL IRP completes (by the target device)
	KeInitializeEvent(
			&keIoctlComplete,
			NotificationEvent,
			FALSE );

	// Construct the IRP for the private IOCTL request
	pIrp = IoBuildDeviceIoControlRequest(
				IOCTL_GET_MAX_BUFFER_SIZE,
				pTargetDevObj,
				NULL,
				0,
				pBufferInfo,
				sizeof(BUFFER_SIZE_INFO),
				FALSE,
				&keIoctlComplete,
				&Iosb );

	// Send the new IRP down to the target
	IoCallDriver( pTargetDevObj, pIrp );

	// Wait for the target to complete the IRP
	LARGE_INTEGER maxWait;
	maxWait.QuadPart = -3 * 10000000;
	KeWaitForSingleObject(
			&keIoctlComplete,
			Executive,
			KernelMode,
			FALSE,
			&maxWait );	// wait no more than 3 secs

#if DBG>=1
	DbgPrint("HIFILTER: GetBufferLimits: MW=%d, MR=%d\n",
			pBufferInfo->MaxWriteLength,
			pBufferInfo->MaxReadLength);
#endif

}

NTSTATUS DispatchPassThru(
				IN PDEVICE_OBJECT pDevObj,
				IN PIRP pIrp ) {

	PDEVICE_EXTENSION pFilterExt = (PDEVICE_EXTENSION)
			pDevObj->DeviceExtension;

	PIO_STACK_LOCATION pIrpStack =
			IoGetCurrentIrpStackLocation( pIrp );

	PIO_STACK_LOCATION pNextIrpStack =
			IoGetNextIrpStackLocation( pIrp );

#if DBG>=1
	DbgPrint("HIFILTER: DispatchPassThru\n");
#endif
	// Copy args to the next level
    // IoCopyCurrentIrpStackLocationToNext(pIrp);
	*pNextIrpStack = *pIrpStack;

	// Set up a completion routine to handle the bubbling
	//	of the "pending" mark of an IRP
	IoSetCompletionRoutine(
			pIrp,
			GenericCompletion,
			NULL,
			TRUE, TRUE, TRUE );

	// Pass the IRP to the target.
	return IoCallDriver(
				pFilterExt->pTargetDevice,
				pIrp );
}


NTSTATUS GenericCompletion(
				IN PDEVICE_OBJECT pDevObj,
				IN PIRP pIrp,
				IN PVOID pContext ) {

	if ( pIrp->PendingReturned )
		IoMarkIrpPending( pIrp );

	return STATUS_SUCCESS;
}

NTSTATUS DispPnp(	IN PDEVICE_OBJECT pDO,
					IN PIRP pIrp ) {
	// obtain current IRP stack location
	PIO_STACK_LOCATION pIrpStack;
	pIrpStack = IoGetCurrentIrpStackLocation( pIrp );
#if DBG>=1
	DbgPrint("HIFILTER: Received PNP IRP: %d\n",
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
	return IoCallDriver(pDevExt->pTargetDevice, pIrp);
}

NTSTATUS HandleStartDevice(	IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp ) {
	// The stack location contains the Parameter info
	PIO_STACK_LOCATION pIrpStack =
		IoGetCurrentIrpStackLocation( pIrp );
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDO->DeviceExtension;
#if DBG>=1
	DbgPrint("HIFILTER: StartDevice\n");
#endif

	pDevExt->state = Started;

	return PassDownPnP(pDO, pIrp);

}

NTSTATUS HandleStopDevice(	IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp ) {
#if DBG>=1
	DbgPrint("HIFILTER: StopDevice Handler\n");
#endif
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDO->DeviceExtension;

	pDevExt->state = Stopped;

	return PassDownPnP(pDO, pIrp);
}

NTSTATUS HandleRemoveDevice(IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp ) {
#if DBG>=1
	DbgPrint("HIFILTER: RemoveDevice Handler\n");
#endif
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDO->DeviceExtension;

	// Delete the filter device
	IoDeleteDevice( pDO );

	pDevExt->state = Removed;
	return PassDownPnP( pDO, pIrp );
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
#if DBG>=1
	DbgPrint("HIFILTER: DriverUnload\n");
#endif

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
	DbgPrint("HIFILTER: IRP Canceled\n");
#endif
	
	// TODO:  Perform IRP cleanup work

	// Just complete the IRP
	pIrp->IoStatus.Status = STATUS_CANCELLED;
	pIrp->IoStatus.Information = 0;	// bytes xfered
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	IoStartNextPacket( pDevObj, TRUE );
}


//++
// Function:	OverriddenDispatchWrite
//
// Description:
//		Handles call from Win32 WriteFile request
//		Breaks up large requests into multiple chuncks
//		suitable for the LODriver.
//
// Arguments:
//		pDevObj - Passed from I/O Manager
//		pIrp - Passed from I/O Manager
//
// Return value:
//		NTSTATUS - success or failuer code
//--

NTSTATUS OverriddenDispatchWrite (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			) {
#if DBG>=1
	DbgPrint("HIFILTER: Write Operation requested (DispatchWrite)\n");
#endif
	
	PDEVICE_EXTENSION pFilterExt = (PDEVICE_EXTENSION)
			pDevObj->DeviceExtension;

	PIO_STACK_LOCATION pIrpStack =
			IoGetCurrentIrpStackLocation( pIrp );

	PIO_STACK_LOCATION pNextIrpStack =
			IoGetNextIrpStackLocation( pIrp );

	ULONG maxTransfer =
			pFilterExt->bufferInfo.MaxWriteLength;

	ULONG bytesRequested =
			pIrpStack->Parameters.Write.Length;

	// We can handle the request for 0 bytes ourselves
	if (bytesRequested == 0) {
		pIrp->IoStatus.Status = STATUS_SUCCESS;
		pIrp->IoStatus.Information = 0;
		IoCompleteRequest( pIrp, IO_NO_INCREMENT );
		return STATUS_SUCCESS;
	}

	// If the request is small enough for the target
	// device, just pass it thru...
	if (bytesRequested <= maxTransfer)
		return DispatchPassThru( pDevObj, pIrp );

	// Set up the next lower stack location to xfer as
	// much data as the lower level allows.
	pNextIrpStack->MajorFunction = IRP_MJ_WRITE;
	pNextIrpStack->Parameters.Write.Length = maxTransfer;

	// It turns out that the lower driver doesn't use the
	// ByteOffset field of the IRP's Parameter.Write block
	// so we use it for context storage.
	// HighPart holds the remaining transfer count.
	// LowPart holds the original buffer address. 
	pIrpStack->Parameters.Write.ByteOffset.HighPart =
			bytesRequested;
	pIrpStack->Parameters.Write.ByteOffset.LowPart =
			(ULONG) pIrp->AssociatedIrp.SystemBuffer;

	// Set up the I/O Completion routine.  Since there is
	// no external context (beyond the IRP's Parameters)
	// no context is passed to the Completion routine.
	IoSetCompletionRoutine(
				pIrp,
				WriteCompletion,
				NULL,		// no context
				TRUE, TRUE, TRUE );

	// Pass the IRP to the target
	return IoCallDriver( 
				pFilterExt->pTargetDevice,
				pIrp );
}

//++
// Function:	WriteCompletion
//
// Description:
//		Each time a partial Write completes,
//		this routine sends down another chunk.
//		suitable for the LODriver.
//		When the entire Write is complete,
//		complete the original IRP.
//
// Arguments:
//		pDevObj - Passed from I/O Manager
//		pIrp - Passed from I/O Manager
//
// Return value:
//		NTSTATUS - success or failuer code
//--
NTSTATUS WriteCompletion(
				IN PDEVICE_OBJECT pDevObj,
				IN PIRP pIrp,
				IN PVOID pContext ) {

	PDEVICE_EXTENSION pFilterExt = (PDEVICE_EXTENSION)
			pDevObj->DeviceExtension;

	PIO_STACK_LOCATION pIrpStack =
			IoGetCurrentIrpStackLocation( pIrp );

	PIO_STACK_LOCATION pNextIrpStack =
			IoGetNextIrpStackLocation( pIrp );

#if DBG>=1
	DbgPrint("HIFILTER: WriteCompletion\n");
#endif
	ULONG transferSize =
				pIrp->IoStatus.Information;

	ULONG bytesRequested =
				pIrpStack->Parameters.Write.Length;

	ULONG bytesRemaining = (ULONG)
			pIrpStack->Parameters.Write.ByteOffset.HighPart;

	ULONG maxTransfer =
			pFilterExt->bufferInfo.MaxWriteLength;

	NTSTATUS status = pIrp->IoStatus.Status;

	// If the last transfer was successful, reduce the
	// "bytesRemaining" context variable.
	if (NT_SUCCESS( status ))
		bytesRemaining -= transferSize;
	pIrpStack->Parameters.Write.ByteOffset.HighPart =
		bytesRemaining;

		// If there is still more data to transfer, do it.
	if ( NT_SUCCESS( status ) && (bytesRemaining > 0) ) {

		// Bump the buffer address to next chunk.
		pIrp->AssociatedIrp.SystemBuffer =
				(PUCHAR)pIrp->AssociatedIrp.SystemBuffer +
							transferSize;

		// Update the new transferSize:
		transferSize = bytesRemaining;
		if ( transferSize > maxTransfer )
			transferSize = maxTransfer;

		// Build the IRP stack beneath us (again)
		pNextIrpStack->MajorFunction = IRP_MJ_WRITE;

		pNextIrpStack->Parameters.Write.Length =
			transferSize;

		// Set up so we get called again:
		IoSetCompletionRoutine(
				pIrp,
				WriteCompletion,
				NULL,
				TRUE, TRUE, TRUE );

		// Now pass it down:
		IoCallDriver(
			pFilterExt->pTargetDevice,
			pIrp );

		return STATUS_MORE_PROCESSING_REQUIRED;

	} else {
		// There was either an error on the last xfer, or
		// we're done.  Either way, complete the IRP.

		// Restore the original system buffer address:
		pIrp->AssociatedIrp.SystemBuffer = (PVOID)
			pIrpStack->Parameters.Write.ByteOffset.LowPart;

		// Show the total number of bytes xfered:
		pIrp->IoStatus.Information =
			bytesRequested - bytesRemaining;

		// See if the pending mark should be bubbled:
		if ( pIrp->PendingReturned )
			IoMarkIrpPending( pIrp );

		return STATUS_SUCCESS;
	}
}


//++
// Function:	OverriddenDispatchDeviceIoControl
//
// Description:
//		Handles call from Win32 DeviceIoControl request
//
// Arguments:
//		pDevObj - Passed from I/O Manager
//		pIrp - Passed from I/O Manager
//
// Return value:
//		NTSTATUS - success or failuer code
//--

NTSTATUS OverriddenDispatchIoControl(
			IN PDEVICE_OBJECT pDevObj,
			IN PIRP pIrp ) {

	PIO_STACK_LOCATION pIrpStack =
			IoGetCurrentIrpStackLocation( pIrp );

	PBUFFER_SIZE_INFO pBufferInfo;

	// Here is the interception
	if (pIrpStack->Parameters.DeviceIoControl.IoControlCode
					== IOCTL_GET_MAX_BUFFER_SIZE ) {
		// The buffer passed by the user (by mutual
		// agreement) is treated as BUFFER_SIZE_INFO type.
		pBufferInfo = (PBUFFER_SIZE_INFO)
			pIrp->AssociatedIrp.SystemBuffer;
		pBufferInfo->MaxWriteLength = NO_BUFFER_LIMIT;
		pBufferInfo->MaxReadLength = NO_BUFFER_LIMIT;

		// Complete the IRP by announcing the size of
		// the returned BUFFER_SIZE_INFO information.
		pIrp->IoStatus.Information = 
				sizeof(BUFFER_SIZE_INFO);
		pIrp->IoStatus.Status = STATUS_SUCCESS;
		IoCompleteRequest( pIrp, IO_NO_INCREMENT );
		return STATUS_SUCCESS;

	} else
		// not the IOCTL we're supposed to intercept,
		// just pass it thru to the "real" device.
		return DispatchPassThru( pDevObj, pIrp );
}


