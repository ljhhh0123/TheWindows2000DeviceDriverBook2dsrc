//
// Driver.c - Chapter 12 - DMA Slave Packet Driver
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

static NTSTATUS DispatchReadWrite(
	IN PDEVICE_OBJECT pDO,
	IN PIRP pIrp
	);


BOOLEAN Isr (
			IN PKINTERRUPT pIntObj,
			IN PVOID pServiceContext		);

VOID StartIo(
	IN PDEVICE_OBJECT pDevObj,
	IN PIRP pIrp
	);

NTSTATUS GetDmaInfo( IN INTERFACE_TYPE busType,
				 IN PDEVICE_OBJECT pDevObj );

VOID DpcForIsr(
	IN PKDPC pDpc,
	IN PDEVICE_OBJECT pDevObj,
	IN PIRP pIrp,
	IN PVOID pContext
	);

IO_ALLOCATION_ACTION AdapterControl(
					IN PDEVICE_OBJECT pDevObj,
					IN PIRP pIrp,
					IN PVOID MapRegisterBase,
					IN PVOID pContext );

VOID StartTransfer( IN PDEVICE_EXTENSION pDevExt );

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
				DispatchReadWrite;
	pDriverObject->MajorFunction[IRP_MJ_READ] =
				DispatchReadWrite;
	pDriverObject->DriverStartIo = StartIo;
	
	// Notice that no device objects are created by DriverEntry.
	// Instead, we await the PnP call to AddDevice

	return status;
}

//++
// Function:	GetDmaInfo
//
// Description:
//		Initializes the driver's DMA Adapter
//
// Arguments:
//		busType - Isa, Internal, etc.
//		busNumber - raw bus number
//
// Return value:
//		NTSTATUS signaling success or failure
//--
NTSTATUS GetDmaInfo( IN INTERFACE_TYPE busType,
					 IN PDEVICE_OBJECT pDevObj ) {
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;

	DEVICE_DESCRIPTION dd;
	
	// Zero out the entire structure
	RtlZeroMemory( &dd, sizeof(dd) );

	dd.Version = DEVICE_DESCRIPTION_VERSION1;
	dd.Master = FALSE;	// this is a slave device
	dd.ScatterGather = FALSE;
	dd.DemandMode = FALSE;
	dd.AutoInitialize = FALSE;
	dd.Dma32BitAddresses = FALSE;

	dd.InterfaceType = busType;	// as passed in

	dd.DmaChannel = pDevExt->dmaChannel;
	dd.MaximumLength = MAX_DMA_LENGTH;
	dd.DmaWidth = Width16Bits;
	dd.DmaSpeed = Compatible;

	// Compute the maximum number of mapping regs
	// this device could possibly need.  Since the
	// transfer may not be paged aligned, add one
	// to allow the max xfer size to span a page.
	pDevExt->mapRegisterCount =
		(MAX_DMA_LENGTH / PAGE_SIZE) + 1;

	pDevExt->pDmaAdapter =
		IoGetDmaAdapter( pDevObj,
					 &dd,
				      &pDevExt->mapRegisterCount);

	// If the Adapter object can't be assigned, fail
	if (pDevExt->pDmaAdapter == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;
	else
		return STATUS_SUCCESS;
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
	
	// Form the internal Device Name
	CUString devName("\\Device\\DMASLAVE"); // for "Slave DMA" dev
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

	// Choose to use DIRECT_IO (typical for DMA)
	pfdo->Flags |= DO_DIRECT_IO;

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
	CUString symLinkName("\\??\\DMAS");
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
			break;

		case CmResourceTypeDma:
			pDevExt->dmaChannel = 
				pPartialDescriptor->u.Dma.Channel;

			// Now use the DMA resources to grab an Adapter object
			GetDmaInfo( Isa, pDO );
			break;

		case CmResourceTypePort:
			pDevExt->portBase = (PUCHAR)
				pPartialDescriptor->u.Port.Start.LowPart;
			pDevExt->portLength =
				pPartialDescriptor->u.Port.Length;
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
		return status;
	}
	
	pDevExt->state = Started;

	return PassDownPnP(pDO, pIrp);

}

NTSTATUS HandleStopDevice(	IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp ) {
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDO->DeviceExtension;

	// Delete our Interrupt object
	if (pDevExt->pIntObj)
		IoDisconnectInterrupt( pDevExt->pIntObj );
	pDevExt->pIntObj = NULL;

	// Delete the DMA Adapter object
	pDevExt->pDmaAdapter->DmaOperations->
		FreeAdapterChannel( pDevExt->pDmaAdapter );
	pDevExt->pDmaAdapter = NULL;

	pDevExt->state = Stopped;

	return PassDownPnP(pDO, pIrp);
}

NTSTATUS HandleRemoveDevice(IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp ) {
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
	
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
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

	// Just complete the IRP
	pIrp->IoStatus.Status = STATUS_CANCELLED;
	pIrp->IoStatus.Information = 0;	// bytes xfered
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	IoStartNextPacket( pDevObj, TRUE );
}


//++
// Function:
//		DispatchReadWrite
//
// Description:
//		This function dispatches ReadFile
//		and WriteFile requests from Win32
//
// Arguments:
//		Pointer to Device object
//		Pointer to IRP for this request
//
// Return Value:
//		This function returns STATUS_XXX
//--
static NTSTATUS DispatchReadWrite(
	IN PDEVICE_OBJECT pDO,
	IN PIRP pIrp
	)
{
	PIO_STACK_LOCATION pIrpStack = 
				IoGetCurrentIrpStackLocation( pIrp );
	//
	// Check for zero-length transfers
	//
	if( pIrpStack->Parameters.Read.Length == 0 ) {
		pIrp->IoStatus.Status = STATUS_SUCCESS;
		pIrp->IoStatus.Information = 0;
		IoCompleteRequest( pIrp, IO_NO_INCREMENT );
		return STATUS_SUCCESS;
	}
	
	//
	// Start device operation
	//
	IoMarkIrpPending( pIrp );
	IoStartPacket( pDO, pIrp, 0, NULL );
	return STATUS_PENDING;
}

BOOLEAN Isr (
			IN PKINTERRUPT pIntObj,
			IN PVOID pServiceContext		) {

	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pServiceContext;
	PDEVICE_OBJECT pDevObj = pDevExt->pDevice;
	PIRP pIrp = pDevObj->CurrentIrp;

	// Check HW to see if interrupt for this driver
	// If not, return FALSE immediately
		//return FALSE;

	// its our interrupt, deal with it
	// Dismiss the interrupt in HW now

	// Were we expecting an interrupt?
	if (!pDevExt->bInterruptExpected)
		return TRUE;	// nope
	pDevExt->bInterruptExpected = FALSE;

	// Do the rest of the work down
	// at DISPATCH_LEVEL IRQL
	IoRequestDpc( 
		pDevObj, 
		pIrp,
		(PVOID)pDevExt );

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
VOID StartIo( IN PDEVICE_OBJECT pDevObj,
			IN PIRP pIrp ) {
	PIO_STACK_LOCATION pStack =
		IoGetCurrentIrpStackLocation( pIrp );

	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;

	// The IRP holds the MDL structure, already set up by
	//   the I/O Manager because DO_DIRECT_IO flag is set
	PMDL pMdl = pIrp->MdlAddress;

	ULONG mapRegsNeeded;
	NTSTATUS status;

	pDevExt->bWriting = FALSE;	// assume read operation

	switch ( pStack->MajorFunction ) {
	case IRP_MJ_WRITE:
		pDevExt->bWriting = TRUE;	// bad assumption
	case IRP_MJ_READ:
		pDevExt->bytesRequested = 
			MmGetMdlByteCount( pMdl );
		pDevExt->transferVA = (PUCHAR)
			MmGetMdlVirtualAddress( pMdl );
		pDevExt->bytesRemaining =
		pDevExt->transferSize = 
			pDevExt->bytesRequested;

		mapRegsNeeded =
			ADDRESS_AND_SIZE_TO_SPAN_PAGES(
				pDevExt->transferVA,
				pDevExt->transferSize );

		if (mapRegsNeeded > pDevExt->mapRegisterCount) {
			mapRegsNeeded = pDevExt->mapRegisterCount;
			pDevExt->transferSize =
				mapRegsNeeded * PAGE_SIZE -
				MmGetMdlByteOffset( pMdl );
		}

		status = pDevExt->pDmaAdapter->DmaOperations->
				AllocateAdapterChannel(
					pDevExt->pDmaAdapter,
					pDevObj,
					mapRegsNeeded,
					AdapterControl,
					pDevExt );

		if (!NT_SUCCESS( status )) {
			// fail the IRP & don't continue with it
			pIrp->IoStatus.Status = status;
			// Show no bytes transferred
			pIrp->IoStatus.Information = 0;
			IoCompleteRequest( pIrp, IO_NO_INCREMENT );
			IoStartNextPacket( pDevObj, FALSE);
		}
		break;	// nice job - AdapterControl takes it
				//			from here on

	default:
		// Shouldn't be here - ditch this strange IRP
		pIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;
		pIrp->IoStatus.Information = 0;
		IoCompleteRequest( pIrp, IO_NO_INCREMENT );
		IoStartNextPacket( pDevObj, FALSE );
	}
}

//++
// Function:
//		AdapterControl
//
// Description:
//		This function sets up the first
//		part of a split transfer and
//		starts the DMA device.
//
// Arguments:
//		Device object
//		Current Irp
//		Map register base handle
//		Pointer to Device Extension
//
// Return Value:
//		KeepObject
//--
IO_ALLOCATION_ACTION AdapterControl(
					IN PDEVICE_OBJECT pDevObj,
					IN PIRP pIrp,
					IN PVOID MapRegisterBase,
					IN PVOID pContext ) {
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
								pContext;

	// Save the handle to the mapping register set
	pDevExt->mapRegisterBase = MapRegisterBase;

	// Flush the CPU cache(s), 
	//	if necessary on this platform...
	KeFlushIoBuffers( pIrp->MdlAddress,
				   !pDevExt->bWriting, 	// inverted
				   TRUE );			// yes DMA

	pDevExt->pDmaAdapter->DmaOperations->
		MapTransfer( pDevExt->pDmaAdapter,
				   pIrp->MdlAddress,
				   MapRegisterBase,	
				   pDevExt->transferVA,
				   &pDevExt->transferSize,
				   pDevExt->bWriting );

	// Start the device
	StartTransfer( pDevExt );

	return KeepObject;
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
VOID DpcForIsr(IN PKDPC pDpc,
			IN PDEVICE_OBJECT pDevObj,
			IN PIRP pIrp,
			IN PVOID pContext ) {

	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pContext;
	ULONG mapRegsNeeded;
	PMDL pMdl = pIrp->MdlAddress;

	// Flush the Apapter buffer to system RAM or device.
	pDevExt->pDmaAdapter->DmaOperations->
		FlushAdapterBuffers( pDevExt->pDmaAdapter,
						 pMdl,
						 pDevExt->mapRegisterBase,	
						 pDevExt->transferVA,
						 pDevExt->transferSize,
						 pDevExt->bWriting );

	// If the device is reporting errors, fail the IRP
	if (DEVICE_FAIL( pDevExt )) {
		// An error occurred, the DMA channel is now free
		pDevExt->pDmaAdapter->DmaOperations->
			FreeAdapterChannel( pDevExt->pDmaAdapter );
		pIrp->IoStatus.Status = STATUS_DEVICE_DATA_ERROR;
		pIrp->IoStatus.Information =
			pDevExt->bytesRequested - 
			pDevExt->bytesRemaining;
		IoCompleteRequest( pIrp, IO_NO_INCREMENT );
		IoStartNextPacket( pDevObj, FALSE);
	}

	// Device had no errors, see if another partial needed
	pDevExt->bytesRemaining -= pDevExt->transferSize;
	if (pDevExt->bytesRemaining > 0) {
		// Another partial transfer needed
		// Update the transferVA and try to finish it
		pDevExt->transferVA += pDevExt->transferSize;
		pDevExt->transferSize = pDevExt->bytesRemaining;
		mapRegsNeeded =
			ADDRESS_AND_SIZE_TO_SPAN_PAGES(
				pDevExt->transferVA,
				pDevExt->transferSize );
		// If it still doesn't fit in one swipe,
		//	cut back the expectation
		if (mapRegsNeeded > pDevExt->mapRegisterCount) {
			mapRegsNeeded = pDevExt->mapRegisterCount;
			pDevExt->transferSize =
				mapRegsNeeded * PAGE_SIZE -
				BYTE_OFFSET( pDevExt->transferVA );
		}

		// Now set up the mapping registers for another
		pDevExt->pDmaAdapter->DmaOperations->
			MapTransfer( pDevExt->pDmaAdapter,
					   pMdl,
					   pDevExt->mapRegisterBase,
					   pDevExt->transferVA,
					   &pDevExt->transferSize,
					   pDevExt->bWriting );

		// And start the device
		StartTransfer( pDevExt );
	} else {
		// Entire transfer has now completed -
		// Free the DMA channel for another device
		pDevExt->pDmaAdapter->DmaOperations->
			FreeAdapterChannel( pDevExt->pDmaAdapter );
		// And complete the IRP in glory
		pIrp->IoStatus.Status = STATUS_SUCCESS;
		pIrp->IoStatus.Information = 
			pDevExt->bytesRequested;

		// Choose a priority boost appropriate for device
		IoCompleteRequest( pIrp, IO_DISK_INCREMENT );
		IoStartNextPacket( pDevObj, FALSE );
	}
}

VOID StartTransfer( IN PDEVICE_EXTENSION pDevExt ) {
	// This place holder routine would hold the code
	// necessary to manipulate the slave DMA device
	// so that it starts the transfer of data.
}
