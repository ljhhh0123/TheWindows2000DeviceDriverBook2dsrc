//
// Driver.c - Chapter 13 - WMI Support Driver Example
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

static NTSTATUS DispatchWMI (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			);

VOID DispatchCancel (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			);

static BOOLEAN TransmitByte( 
		IN PVOID pArg );

VOID StartIo(
	IN PDEVICE_OBJECT pDevObj,
	IN PIRP pIrp
	);

VOID PollingTimerDpc( IN PKDPC pDpc,
					  IN PVOID pContext,
					  IN PVOID SysArg1,
					  IN PVOID SysArg2 );

NTSTATUS
  DpWmiQueryReginfo(
    IN PDEVICE_OBJECT DeviceObject,
    OUT PULONG RegFlags,
    OUT PUNICODE_STRING InstanceName,
    OUT PUNICODE_STRING *RegistryPath,
    OUT PUNICODE_STRING MofResourceName,
    OUT PDEVICE_OBJECT *Pdo
    );

NTSTATUS
  DpWmiQueryDataBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG InstanceIndex,
    IN ULONG InstanceCount,
    IN OUT PULONG InstanceLengthArray,
    IN ULONG BufferAvail,
    OUT PUCHAR Buffer
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
static UNICODE_STRING regPath;

extern "C" NTSTATUS DriverEntry (
			IN PDRIVER_OBJECT pDriverObject,
			IN PUNICODE_STRING pRegistryPath	) {
#if DBG>=2
	DbgPrint("WMIEX: DriverEntry\n");
#endif
	ULONG ulDeviceNumber = 0;
	NTSTATUS status = STATUS_SUCCESS;

	// Announce other driver entry points
	pDriverObject->DriverUnload = DriverUnload;

	// Announce the WMI request entry point
	pDriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] =
				DispatchWMI;
	
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

	// Save the registry key service name for this driver
	regPath = *pRegistryPath;	

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
#if DBG>=1
	DbgPrint("WMIEX: AddDevice; current DeviceNumber = %d\n",
				ulDeviceNumber);
#endif
	
	// Form the internal Device Name
	CUString devName("\\Device\\WMIEXAMPLE"); // for WMI Example dev
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
	pDevExt->state = Stopped;

	// Initialize the MOF data
	pDevExt->mofData.totalTransfers = 
		pDevExt->mofData.totalReads =
		pDevExt->mofData.totalWrites = 0;

	// Pile this new fdo on top of the existing lower stack
	pDevExt->pLowerDevice =		// downward pointer
		IoAttachDeviceToDeviceStack( pfdo, pdo);

	// Save the PDO in the device extension
	pDevExt->pPDO = pdo;

	// This is where the upper pointer would be initialized.
	// Notice how the cast of the lower device's extension
	// must be known in order to find the offset pUpperDevice.
	// PLOWER_DEVEXT pLowerDevExt = (PLOWER_DEVEXT)
	//		pDevExt->pLowerDevice->DeviceExtension;
	// pLowerDevExt->pUpperDevice = pfdo;

	// Calculate the polling interval in uS
	//	and keep as relative time (negative value)
	pDevExt->pollingInterval =
		RtlConvertLongToLargeInteger( POLLING_INTERVAL * -10 );

	// Prepare the polling timer and DPC
	KeInitializeTimer( &pDevExt->pollingTimer );
	// Notice that the DPC routine receives the fdo
	//	as its argument
	KeInitializeDpc( &pDevExt->pollingDPC,
						PollingTimerDpc,
						(PVOID) pfdo );

	// Form the symbolic link name
	CUString symLinkName("\\??\\WMIEX");
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

	// Made it
	ulDeviceNumber++;
	return STATUS_SUCCESS;
}


NTSTATUS DispPnp(	IN PDEVICE_OBJECT pDO,
					IN PIRP pIrp ) {
	// obtain current IRP stack location
	PIO_STACK_LOCATION pIrpStack;
	pIrpStack = IoGetCurrentIrpStackLocation( pIrp );
#if DBG>=1
	DbgPrint("WMIEX: Received PNP IRP: %d\n",
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
#if DBG>=1
	DbgPrint("WMIEX: StartDevice, Symbolic device #%d\n",
				pDevExt->DeviceNumber+1);
#endif

	PCM_RESOURCE_LIST pResourceList;
	PCM_FULL_RESOURCE_DESCRIPTOR pFullDescriptor;
	PCM_PARTIAL_RESOURCE_LIST pPartialList;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR pPartialDescriptor;
	int i;

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
#if DBG>=1
	DbgPrint("WMIEX: Presented with Interrupt Resources - "
			 "ignored");
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
#if DBG>=1
	DbgPrint("WMIEX: Claiming Port Resources: Base=%X Len=%d\n",
				pDevExt->portBase, pDevExt->portLength);
#endif
			break;
		case CmResourceTypeMemory:
			// We don't do memory usage
			break;
		}
	}

	// Register as a WMI Participant
	IoWMIRegistrationControl( pDO, WMIREG_ACTION_REGISTER);

	pDevExt->state = Started;

	return PassDownPnP(pDO, pIrp);

}

NTSTATUS HandleStopDevice(	IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp ) {
#if DBG>=1
	DbgPrint("WMIEX: StopDevice Handler\n");
#endif
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDO->DeviceExtension;

	pDevExt->state = Stopped;

	return PassDownPnP(pDO, pIrp);
}

NTSTATUS HandleRemoveDevice(IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp ) {
#if DBG>=1
	DbgPrint("WMIEX: RemoveDevice Handler\n");
#endif
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDO->DeviceExtension;

	// Revoke participation as a WMI Provider
	IoWMIRegistrationControl( pDO, WMIREG_ACTION_DEREGISTER);

	// This will yield the symbolic link name
	UNICODE_STRING pLinkName =
		pDevExt->ustrSymLinkName;
	// ... which can now be deleted
	IoDeleteSymbolicLink(&pLinkName);
#if DBG>=1
	DbgPrint("PPORT: Symbolic Link TMRPP%d Deleted\n",
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
	DbgPrint("WMIEX: DriverUnload\n");
#endif

}

//++
// Function:	DispatchWMI
//
// Description:
//		Handles call for WMI SYSTEM_CONTROL request
//
// Arguments:
//		pDevObj - Passed from I/O Manager
//		pIrp - Passed from I/O Manager
//
// Return value:
//		NTSTATUS - success or failuer code
//--

NTSTATUS DispatchWMI (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			) {
#if DBG>=1
	DbgPrint("WMIEX: WMI SYSTEM_CONTROL requested (DispatchWMI)\n");
#endif

	static WMIGUIDREGINFO wmiGuids = {
		&GUID_WMIEx,	// GUIDS
		1,			// Number of instances
		WMIREG_FLAG_INSTANCE_PDO };	// Flags

	static WMILIB_CONTEXT wmiCtx = {
		1,			// GuidCount
		&wmiGuids,	// GuidList
		DpWmiQueryReginfo,
		DpWmiQueryDataBlock,
		NULL,		// SetWmiDataBlock
		NULL,		// SetWmiDataItem
		NULL,		// ExecuteWmiMethod
		NULL };		// WmiFunctionControl

	SYSCTL_IRP_DISPOSITION disposition;
	NTSTATUS status =
		WmiSystemControl( &wmiCtx, 
						  pDevObj, 
						  pIrp, 
						  &disposition);

	switch (disposition) {
	case IrpProcessed:
		return status;
	case IrpNotCompleted:
		// Complete the IRP
		IoCompleteRequest( pIrp, IO_NO_INCREMENT );
		return status;
	case IrpNotWmi:
	case IrpForward:
	default:
		return PassDownPnP( pDevObj, pIrp );
	}
}

NTSTATUS
  DpWmiQueryReginfo(
    IN PDEVICE_OBJECT pDevObj,
    OUT PULONG pRegFlags,
    OUT PUNICODE_STRING InstanceName,
    OUT PUNICODE_STRING *pRegistryPath,
    OUT PUNICODE_STRING MofResourceName,
    OUT PDEVICE_OBJECT *pPdo
    ) {

#if DBG>=1
	DbgPrint("WMIEX: DpWmiQuueryRegInfo\n");
#endif
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;

	*pRegFlags = WMIREG_FLAG_INSTANCE_PDO;
	*pRegistryPath = &regPath;
	
	// MofResourceName is whatever name was used in the
	//	Resource (.rc) file for the driver project
	RtlInitUnicodeString(MofResourceName, L"MofResource");

	// Returning the PDO (Physical Device Object) is needed
	// since we are requesting auto-naming based on the PDO
	*pPdo = pDevExt->pPDO;

	return STATUS_SUCCESS;
}

NTSTATUS
  DpWmiQueryDataBlock(
    IN PDEVICE_OBJECT pDevObj,
    IN PIRP pIrp,
    IN ULONG GuidIndex,
    IN ULONG InstanceIndex,
    IN ULONG InstanceCount,
    IN OUT PULONG InstanceLengthArray,
    IN ULONG BufferAvail,
    OUT PUCHAR pBuffer
    ) {

#if DBG>=1
	DbgPrint("WMIEX: DpWmiQueryDataBlock\n");
	DbgPrint("WMIEX: GuidIndex = %d, InstanceIndex = %d\n", GuidIndex, InstanceIndex);
	DbgPrint("WMIEX: InstanceCount = %d, InstanceLengthArray = %d\n", InstanceCount, InstanceLengthArray);
	DbgPrint("WMIEX: BufferAvail = %d, MOF_DATA_SIZE = %d\n", BufferAvail, MOF_DATA_SIZE);
#endif
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;

	// Do we have enough room for the MOF data?
	if ( MOF_DATA_SIZE > BufferAvail )
		return WmiCompleteRequest( pDevObj,
									pIrp,
									STATUS_BUFFER_TOO_SMALL,
									MOF_DATA_SIZE,
									IO_NO_INCREMENT );

	// Stuff Buffer with gathered MOF data (3 ULONGS)
	// Simply cast Buffer as MofData structure
	// Then let the compiler copy everything
	*(PMOFDATA)pBuffer = pDevExt->mofData;

	return WmiCompleteRequest( pDevObj,
								pIrp,
								STATUS_SUCCESS,
								MOF_DATA_SIZE,
								IO_NO_INCREMENT );
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
	DbgPrint("WMIEX: DispatchCreate\n");
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
	DbgPrint("WMIEX: DispatchClose\n");
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
	DbgPrint("WMIEX: IRP Canceled\n");
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
//		For this driver, starts the device by indirectly
//		invoking StartIo
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
	DbgPrint("WMIEX: Write Operation requested (DispatchWrite)\n");
#endif
	// Update MOF data
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;
	pDevExt->mofData.totalWrites++;
	
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
	DbgPrint("WMIEX: Read Operation requested (DispatchRead)\n");
#endif
	// Update MOF data
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;
	pDevExt->mofData.totalReads++;
	
	NTSTATUS status = STATUS_SUCCESS;
	PVOID userBuffer;
	ULONG xferSize;
	// The stack location contains the user buffer info
	PIO_STACK_LOCATION pIrpStack =
		IoGetCurrentIrpStackLocation( pIrp );
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

//++
// Function:
//		TransmitByte
//
// Description:
//		This function sends one character to the device
//		The polling timer is then started which will
//		eventually invoke the PollingTimerDpc.
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
#if DBG>=1
	DbgPrint("WMIEX: TransmitByte: Sending 0x%02X to port %X\n",
				nextByte, pDevExt->portBase);
#endif
	// Update MOF data
	pDevExt->mofData.totalTransfers++;

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
#if DBG>=1
	DbgPrint("WMIEX: TransmitByte read character: 0x%03X\n",
				readByte);
#endif

	// Update MOF data
	pDevExt->mofData.totalTransfers++;

	// Start the polling timer
#if DBG>=2
	DbgPrint("WMIEX: TransmitByte starting timer\n");
#endif
	KeSetTimer(
		&pDevExt->pollingTimer,
		pDevExt->pollingInterval,
		&pDevExt->pollingDPC );


	return TRUE;
}

//++
// Function:
//		StartIo
//
// Description:
//		This function is responsible initiating the
//		actual data transfer. The ultimate result
//		should be a started Timer object.
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
	DbgPrint("WMIEX: Start I/O Operation\n");
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

			// Try to send the first byte of data.
#if DBG>=1
	DbgPrint("WMIEX: StartIO: Transmitting first byte of %d\n", pDevExt->deviceBufferSize);
#endif
			TransmitByte( pDevExt );
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
//		PollingTimerDpc
//
// Description:
//		This function is the DPC routine called
//		each time the timer "ticks".
//		The routine checks on the device - 
//		if room available, more data is sent
//
// Arguments:
//		Pointer to the Device object
//		Pointer to the IRP for this request
//
// Return Value:
//		(None)
//--
VOID PollingTimerDpc( IN PKDPC pDpc,
					  IN PVOID pContext,
					  IN PVOID SysArg1,
					  IN PVOID SysArg2 ) {

#if DBG>=1
	DbgPrint("WMIEX: PollingTimerDpc Fired\n");
#endif
	PDEVICE_OBJECT pDevObj = (PDEVICE_OBJECT)
		pContext;
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;

	// Try to send more data
	if (!TransmitByte( pDevExt ) ) {
		// Transfer complete (normal or error)
		// Complete the IRP appropriately
#if DBG>=1
	DbgPrint("WMIEX: PollingTimerDpc Completing IRP\n");
#endif
		PIRP pIrp = pDevObj->CurrentIrp;
		pIrp->IoStatus.Information =
			pDevExt->xferCount;

		// Figure out what the final status should be
		pIrp->IoStatus.Status = STATUS_SUCCESS;
		// If an error occurred, Status would change

		// Now Complete the IRP
		IoCompleteRequest( pIrp, IO_PARALLEL_INCREMENT );

		// And request another IRP
		IoStartNextPacket( pDevObj, FALSE);
	}
}
