//
// Driver.c - Chapter 9 - Minimal PnP Parallel Port Driver
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

static NTSTATUS DispatchRead (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			);

BOOLEAN Isr (
			IN PKINTERRUPT pIntObj,
			IN PVOID pServiceContext		);

static BOOLEAN TransmitByte( 
		IN PVOID pArg );

VOID StartIo(
	IN PDEVICE_OBJECT pDevObj,
	IN PIRP pIrp
	);

VOID DpcForIsr(
	IN PKDPC pDpc,
	IN PDEVICE_OBJECT pDevObj,
	IN PIRP pIrp,
	IN PVOID pContext
	);

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
	DbgPrint("MINPNP: DriverEntry\n");
#endif
	ULONG ulDeviceNumber = 0;
	NTSTATUS status = STATUS_SUCCESS;

	// Announce other driver entry points
	pDriverObject->DriverUnload = DriverUnload;

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
	pDriverObject->MajorFunction[IRP_MJ_READ] =
				DispatchRead;
	pDriverObject->DriverStartIo = StartIo;
	
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
	DbgPrint("MINPNP: AddDevice; current DeviceNumber = %d\n",
				ulDeviceNumber);
#endif
	
	// Form the internal Device Name
	CUString devName("\\Device\\MINPNP"); // for "minimal" dev
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

	// Choose to use BUFFERED_IO
	pfdo->Flags |= DO_BUFFERED_IO;

	// Initialize the Device Extension
	pDevExt = (PDEVICE_EXTENSION)pfdo->DeviceExtension;
	pDevExt->pDevice = pfdo;	// back pointer
	pDevExt->DeviceNumber = ulDeviceNumber;
	pDevExt->ustrDeviceName = devName;
	pDevExt->pIntObj = NULL;
	pDevExt->bInterruptExpected = FALSE;
	pDevExt->state = Stopped;

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
	CUString symLinkName("\\??\\MPNP");
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

	// We need a DpcForIsr registration
	IoInitializeDpcRequest( 
		pfdo, 
		DpcForIsr );

    //  Clear the Device Initializing bit since the FDO was created
    //  outside of DriverEntry.
    pfdo->Flags &= ~DO_DEVICE_INITIALIZING;

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
	DbgPrint("MINPNP: Received PNP IRP: %d\n",
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
	DbgPrint("MINPNP: StartDevice, Symbolic device #%d\n",
				pDevExt->DeviceNumber+1);
#endif

	PCM_RESOURCE_LIST pResourceList;
	PCM_FULL_RESOURCE_DESCRIPTOR pFullDescriptor;
	PCM_PARTIAL_RESOURCE_LIST pPartialList;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR pPartialDescriptor;
	int i;
	NTSTATUS status;

	pResourceList =	pIrpStack->Parameters.StartDevice.AllocatedResourcesTranslated;
	pFullDescriptor =
		pResourceList->List;
	pPartialList =
		&pFullDescriptor->PartialResourceList;
	for (i=0; i<(int)pPartialList->Count; i++) {
		pPartialDescriptor =
			&pPartialList->PartialDescriptors[i];
		switch (pPartialDescriptor->Type) {
		case CmResourceTypeInterrupt:
			pDevExt->IRQL = (KIRQL)
				pPartialDescriptor->u.Interrupt.Level;
			pDevExt->Vector = 
				pPartialDescriptor->u.Interrupt.Vector;
			pDevExt->Affinity = 
				pPartialDescriptor->u.Interrupt.Affinity;
#if DBG>=2
	DbgPrint("MINPNP: Claiming Interrupt Resources: "
			 "IRQ=%d Vector=0x%03X Affinity=%X\n",
			 pDevExt->IRQL, pDevExt->Vector, pDevExt->Affinity);
#endif
			break;
		case CmResourceTypeDma:
			// We don't do DMA - ignore
			break;
		case CmResourceTypePort:
			pDevExt->portBase = (PUCHAR)
				pPartialDescriptor->u.Port.Start.LowPart;
			pDevExt->portLength =
				pPartialDescriptor->u.Port.Length;
#if DBG>=2
	DbgPrint("MINPNP: Claiming Port Resources: Base=%X Len=%d\n",
				pDevExt->portBase, pDevExt->portLength);
#endif
			break;
		case CmResourceTypeMemory:
			// We don't do memory usage
			break;
		}
	}

	// Make sure we got our interrupt (and port) resources
	// Fail this IRP if we didn't.
	// Most likely cause for no interrupt:
	//   Failure to request an interrupt resource for the
	//	 printer port from the Device Manager.
	// Be SURE to use Control Panel...
	//				  Administrative Tools...
	//				  Computer Management...
	//				  Device Manager...
	// Then select Ports...Printer Port (LPT1)
	// From the Port Settings tab,
	// select "Use any interrupt assigned to the port"
	if (pDevExt->IRQL == 0 ||
		pDevExt->portBase == 0)
		return STATUS_BIOS_FAILED_TO_CONNECT_INTERRUPT;

	// Create & connect to an Interrupt object
	status =
		IoConnectInterrupt(
			&pDevExt->pIntObj,	// the Interrupt object
			Isr,			// our ISR
			pDevExt,		// Service Context
			NULL,			// no spin lock
			pDevExt->Vector,			// vector
			pDevExt->IRQL,		// DIRQL
			pDevExt->IRQL,		// DIRQL
			Latched,	// Latched or LevelSensitive
			TRUE,			// Shared?
			pDevExt->Affinity,		// processors in an MP set
			FALSE );		// save FP registers?
	if (!NT_SUCCESS(status)) {
	#if DBG>=2
		DbgPrint("MINPNP: Interrupt connection failure: %X\n", status);
	#endif
		return status;
	}
	
	#if DBG>=2
		DbgPrint("MINPNP: Interrupt successfully connected\n");
	#endif

	pDevExt->state = Started;

	return PassDownPnP(pDO, pIrp);

}

NTSTATUS HandleStopDevice(	IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp ) {
#if DBG>=1
	DbgPrint("MINPNP: StopDevice Handler\n");
#endif
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDO->DeviceExtension;

	// Delete our Interrupt object
	if (pDevExt->pIntObj)
		IoDisconnectInterrupt( pDevExt->pIntObj );

	pDevExt->state = Stopped;

	return PassDownPnP(pDO, pIrp);
}

NTSTATUS HandleRemoveDevice(IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp ) {
#if DBG>=1
	DbgPrint("MINPNP: RemoveDevice Handler\n");
#endif
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDO->DeviceExtension;

	if (pDevExt->state == Started) {
		// Woah!  we still have an interrupt object out there!
		// Delete our Interrupt object
		if (pDevExt->pIntObj)
			IoDisconnectInterrupt( pDevExt->pIntObj );
	}

	// This will yield the symbolic link name
	UNICODE_STRING pLinkName =
		pDevExt->ustrSymLinkName;
	// ... which can now be deleted
	IoDeleteSymbolicLink(&pLinkName);
#if DBG>=1
	DbgPrint("PPORT: Symbolic Link MPMP%d Deleted\n",
				pDevExt->DeviceNumber+1);
#endif
	
	// Delete the device
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
	DbgPrint("MINPNP: DriverUnload\n");
#endif

}

//++
// Function:	DispatchCreate
//
// Description:
//		Handles call from Win32 CreateFile request
//		Does nothing
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
	DbgPrint("MINPNP: DispatchCreate\n");
#endif

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
//		For this driver, frees any buffer
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
	DbgPrint("MINPNP: DispatchClose\n");
#endif
	
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
	DbgPrint("MINPNP: IRP Canceled\n");
#endif
	
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
//		For this driver, merely "starts" device
//			by indirectly calling StartIo
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
	DbgPrint("MINPNP: Write Operation requested (DispatchWrite)\n");
#endif
	
	// Start the I/O
	IoMarkIrpPending( pIrp );
	IoStartPacket( pDevObj, pIrp, 0, DispatchCancel);
	return STATUS_PENDING;
}

//++
// Function:	DispatchRead
//
// Description:
//		Handles call from Win32 ReadFile request
//		For this driver, xfers pool buffer to user
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
	
#if DBG>=1
	DbgPrint("MINPNP: Read Operation requested (DispatchRead)\n");
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

BOOLEAN Isr (
			IN PKINTERRUPT pIntObj,
			IN PVOID pServiceContext		) {

	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pServiceContext;
	PDEVICE_OBJECT pDevObj = pDevExt->pDevice;
	PIRP pIrp = pDevObj->CurrentIrp;

	UCHAR status = ReadStatus( pDevExt );
	if ((status & STS_NOT_IRQ))
		return FALSE;

	// its our interrupt, deal with it
	WriteControl( pDevExt, CTL_DEFAULT);
	// Were we expecting an interrupt?
	if (!pDevExt->bInterruptExpected)
		return TRUE;	// nope
	pDevExt->bInterruptExpected = FALSE;

	// transmit another character
	if (!TransmitByte( pDevExt ))
		// if no more bytes, complete the request
		IoRequestDpc( pDevObj, pIrp, (PVOID)pDevExt );

	return TRUE;
}

//++
// Function:
//		TransmitByte
//
// Description:
//		This function sends one character to the device
//		An interrupt is then forced from the port.
//		If no characters remain to be transmitted, return FALSE.
//
// Arguments:
//		Pointer to the Device Extension
//
// Return Value:
//		TRUE - one more byte transmitted
//		FALSE - no more bytes remain to be transmitted
//--
 BOOLEAN TransmitByte( 
		IN PVOID pArg ) {
	
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pArg;
	 // If all the bytes have been sent, just quit
	if( pDevExt->xferCount >= pDevExt->maxXferCount)
		return FALSE;

	// A transfer is happening.
	PDEVICE_OBJECT pDevObj =
		pDevExt->pDevice;
	PIRP pIrp = pDevObj->CurrentIrp;
	// Obtain user buffer pointer
	PUCHAR userBuffer = (PUCHAR)
		pIrp->AssociatedIrp.SystemBuffer;
	UCHAR nextByte =
		userBuffer[pDevExt->xferCount];

	// Now send it...
#if DBG>=2
	DbgPrint("MINPNP: TransmitByte: Sending 0x%02X to port %X\n",
				nextByte, pDevExt->portBase);
#endif
	// The loopback connector requires us to do
	// some bizzare work.
	// Bit 0 of the data is sent as bit 0 of Port data.
	WriteData( pDevExt, nextByte & 1);
	// It will be read in Status bit 3
	// Bits 1-3 of the data are sent as bits 0,1, & 3
	// of the Control register.
	// Control bits 0 & 1 are inverted.
	UCHAR bits = (nextByte & 0x8) +
				 ((nextByte & 0x6)>>1);
	bits ^= 0x3;	// Invert the 2 control bits (0 & 1)
	// These bits will be read in Status bits 4,5, & 7
	WriteControl( pDevExt, bits | CTL_DEFAULT);

	// Now read the data from the loopback
	UCHAR status =
		ReadStatus( pDevExt );

	// Format the nibble into the upper half of the byte
	UCHAR readByte =
		((status & 0x8)<< 1) |
		((status & 0x30)<<1) |
		(status & 0x80);
	pDevExt->deviceBuffer[pDevExt->xferCount++] =
		readByte;
#if DBG>=2
	DbgPrint("MINPNP: TransmitByte read character: 0x%03X\n",
				readByte);
#endif

	// Force an interrupt
#if DBG>=2
	DbgPrint("MINPNP: TransmitByte: generating interrupt.\n");
#endif
	// Tell the ISR to expect it
	pDevExt->bInterruptExpected = TRUE;
	// Enable interrupts, raise BUSY
	WriteControl( pDevExt, CTL_INTENB | CTL_SELECT | CTL_DEFAULT );
	// Lower BUSY
	WriteControl( pDevExt, CTL_INTENB | CTL_DEFAULT );
	// Pulse ACK#
	WriteControl( pDevExt, CTL_INTENB | CTL_NOT_RST | CTL_DEFAULT);
	// hold it for about 50 uS
	KeStallExecutionProcessor(50);
	WriteControl( pDevExt, CTL_INTENB | CTL_DEFAULT );

	return TRUE;
}

//++
// Function:
//		StartIo
//
// Description:
//		This function is responsible initiating the
//		actual data transfer. The ultimate result
//		should be an interrupt from the device.
//
// Arguments:
//		Pointer to the Device object
//		Pointer to the IRP for this request
//
// Return Value:
//		(None)
//--
VOID
StartIo(
	IN PDEVICE_OBJECT pDevObj,
	IN PIRP pIrp
	) {
#if DBG>=1
	DbgPrint("MINPNP: Start I/O Operation\n");
#endif
	
	PIO_STACK_LOCATION	pIrpStack = 
		IoGetCurrentIrpStackLocation( pIrp );
		
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;
	PUCHAR userBuffer;
	ULONG xferSize;
		
	switch( pIrpStack->MajorFunction ) {
		
		// Use a SynchCritSection routine to
		// start the write operation...
		case IRP_MJ_WRITE:
			// Set up counts and byte pointer
			pDevExt->maxXferCount = 
				pIrpStack->Parameters.Write.Length;
			pDevExt->xferCount = 0;

			// Since we processing a new Write request,
			// free up any old buffer
			if (pDevExt->deviceBuffer != NULL) {
				ExFreePool(pDevExt->deviceBuffer);
				pDevExt->deviceBuffer = NULL;
				pDevExt->deviceBufferSize = 0;
			}

			// Determine the length of the request
			xferSize = 
				pIrpStack->Parameters.Write.Length;
			// Obtain user buffer pointer
			userBuffer = (PUCHAR)
				pIrp->AssociatedIrp.SystemBuffer;

			// Allocate the new buffer
			pDevExt->deviceBuffer = (PUCHAR)
				ExAllocatePool( PagedPool, xferSize );
			if (pDevExt->deviceBuffer == NULL) {
				// buffer didn't allocate???
				// fail the IRP
				pIrp->IoStatus.Status = 
					STATUS_INSUFFICIENT_RESOURCES;
				pIrp->IoStatus.Information = 0;
				IoCompleteRequest( pIrp, IO_NO_INCREMENT );
				IoStartNextPacket( pDevObj, FALSE );
			}
			pDevExt->deviceBufferSize = xferSize;

			//
			// Try to send the first byte of data.
			// If there's a problem, use
			// the DPC routine to fail the IRP.
			//
#if DBG>=1
	DbgPrint("MINPNP: StartIO: Transmitting first byte of %d\n", pDevExt->deviceBufferSize);
#endif
			if( !KeSynchronizeExecution(
					pDevExt->pIntObj,
					TransmitByte,
					pDevExt ))
			{
				DpcForIsr(
					NULL,
					pDevObj,
					pIrp,
					pDevExt );
			}
			break;
		//
		// Should never get here -- just get rid
		// of the packet...
		//	
		default:
			pIrp->IoStatus.Status =
						STATUS_NOT_SUPPORTED;
			pIrp->IoStatus.Information = 0;
			IoCompleteRequest(
				pIrp,
				IO_NO_INCREMENT );
			IoStartNextPacket( pDevObj, FALSE );
			break;
	}
}

//++
// Function:
//		DpcForIsr
//
// Description:
//		This function performs the low-IRQL
//		post-processing of I/O requests
//
// Arguments:
//		Pointer to a DPC object
//		Pointer to the Device object
//		Pointer to the IRP for this request
//		Pointer to the Device Extension
//
// Return Value:
//		(None)
//--
VOID
DpcForIsr(
	IN PKDPC pDpc,
	IN PDEVICE_OBJECT pDevObj,
	IN PIRP pIrp,
	IN PVOID pContext
	)
{
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pContext;
	
#if DBG>=1
	DbgPrint("MINPNP: DpcForIsr, xferCount = %d\n",
				pDevExt->xferCount);
#endif
	
	pIrp->IoStatus.Information =
			pDevExt->xferCount;

	// This loopback device always works
	pIrp->IoStatus.Status =	
			STATUS_SUCCESS;

	//
	// If we're being called directly from Start I/O,
	// don't give the user any priority boost.
	//
	if( pDpc == NULL )
		IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	else
		IoCompleteRequest( pIrp, IO_PARALLEL_INCREMENT );

	//
	// This one's done. Begin working on the next
	//
	IoStartNextPacket( pDevObj, FALSE );
}
