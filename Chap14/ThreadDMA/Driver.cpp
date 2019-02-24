//
// Driver.c - Chapter 14 - Thread-based Parallel Port Driver
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

NTSTATUS GetDmaInfo( IN INTERFACE_TYPE busType,
					 IN PDEVICE_OBJECT pDevObj );

VOID WorkerThreadMain( IN PVOID pContext );

VOID KillThread( IN PDEVICE_EXTENSION pDE );

static VOID DriverUnload (
		IN PDRIVER_OBJECT	pDriverObject	);

static NTSTATUS DispatchCreate (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			);

static NTSTATUS DispatchClose (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			);

static NTSTATUS DispatchReadWrite (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			);

BOOLEAN Isr (
			IN PKINTERRUPT pIntObj,
			IN PVOID pServiceContext		);

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
	DbgPrint("THREADDMA: DriverEntry\n");
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
				DispatchReadWrite;
	pDriverObject->MajorFunction[IRP_MJ_READ] =
				DispatchReadWrite;
	
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
	DbgPrint("THREADDMA: AddDevice; current DeviceNumber = %d\n",
				ulDeviceNumber);
#endif
	
	// Form the internal Device Name
	CUString devName("\\Device\\THREADDMA"); // for "ThreadDMA" dev
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

	// Initialize the work queue lock
	KeInitializeSpinLock( &pDevExt->lkIrpQueue );

	// Initialize the work queue itself
	InitializeListHead( &pDevExt->IrpQueueListHead );

	// Initialize the work queue semaphore
	KeInitializeSemaphore( &pDevExt->semIrpQueue,
						0, MAXLONG);

	// Initialize the event for the Adapter object
	KeInitializeEvent(
			&pDevExt-> evAdapterObjectIsAcquired,
			SynchronizationEvent, FALSE );

	// Intialize the event for the operation complete
	KeInitializeEvent(
			&pDevExt->evDeviceOperationComplete,
			SynchronizationEvent, FALSE );

	//  Initially the worker thread runs
	pDevExt->bThreadShouldStop = FALSE;

	// Start the worker thread
	HANDLE hThread = NULL;
	status =
		PsCreateSystemThread( &hThread,
					(ACCESS_MASK)0,
					NULL,
					(HANDLE)0,
					NULL,
					WorkerThreadMain,
					pDevExt );		// arg
	if (!NT_SUCCESS(status)) {
		IoDeleteSymbolicLink( &(UNICODE_STRING)symLinkName );
		IoDeleteDevice( pfdo );
		return status;
	}

	// Obtain real pointer to Thread object
	ObReferenceObjectByHandle(
		hThread,
		THREAD_ALL_ACCESS,
		NULL,
		KernelMode,
		(PVOID*)&pDevExt->pThreadObj,
		NULL );
	ZwClose( hThread );	// don't need handle at all

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
	DbgPrint("THREADDMA: Received PNP IRP: %d\n",
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
	DbgPrint("THREADDMA: StartDevice, Symbolic device #%d\n",
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
	DbgPrint("THREADDMA: Claiming Interrupt Resources: "
			 "IRQ=%d Vector=0x%03X Affinity=%X\n",
			 pDevExt->IRQL, pDevExt->Vector, pDevExt->Affinity);
#endif
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
#if DBG>=2
	DbgPrint("THREADDMA: Claiming Port Resources: Base=%X Len=%d\n",
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
		DbgPrint("THREADDMA: Interrupt connection failure: %X\n", status);
	#endif
		return status;
	}
	
	#if DBG>=2
		DbgPrint("THREADDMA: Interrupt successfully connected\n");
	#endif

	pDevExt->state = Started;

	return PassDownPnP(pDO, pIrp);

}

NTSTATUS HandleStopDevice(	IN PDEVICE_OBJECT pDO,
							IN PIRP pIrp ) {
#if DBG>=1
	DbgPrint("THREADDMA: StopDevice Handler\n");
#endif
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
#if DBG>=1
	DbgPrint("THREADDMA: RemoveDevice Handler\n");
#endif
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDO->DeviceExtension;

	if (pDevExt->state == Started) {
		// Woah!  we still have an interrupt object out there!
		// Delete our Interrupt object
		if (pDevExt->pIntObj)
			IoDisconnectInterrupt( pDevExt->pIntObj );
		pDevExt->pIntObj = NULL;
	}

	// Since AddDevice created a system thread,
	// RemoveDevice must kill it.
	KillThread( pDevExt );

	// This will yield the symbolic link name
	UNICODE_STRING pLinkName =
		pDevExt->ustrSymLinkName;
	// ... which can now be deleted
	IoDeleteSymbolicLink(&pLinkName);
#if DBG>=1
	DbgPrint("THREADDMA: Symbolic Link MPMP%d Deleted\n",
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
//		For this driver, does nothing
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
	DbgPrint("THREADDMA: DriverUnload\n");
#endif

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
	DbgPrint("THREADDMA: DispatchCreate\n");
#endif

	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;
	NTSTATUS status = STATUS_SUCCESS;
	if (pDevExt->state != Started)
		status = STATUS_DEVICE_REMOVED;

	// TODO: Reserve any resources needed on a per Open basis
	
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
	DbgPrint("THREADDMA: DispatchClose\n");
#endif
	
	// For this driver, does nothing

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
	DbgPrint("THREADDMA: IRP Canceled\n");
#endif
	
	// TODO:  Perform IRP cleanup work

	// Just complete the IRP
	pIrp->IoStatus.Status = STATUS_CANCELLED;
	pIrp->IoStatus.Information = 0;	// bytes xfered
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	IoStartNextPacket( pDevObj, TRUE );
}


//++
// Function:	DispatchReadWrite
//
// Description:
//		Handles call from Win32 WriteFile request
//		For this driver, IRP is "pended" and placed
//		into the work queue for this Device object.
//		The work queue's Semaphore object is incremented.
//		Notice the lack of a call to IoStartPacket.
//		Instead the worker thread will pick up the
//
// Arguments:
//		pDevObj - Passed from I/O Manager
//		pIrp - Passed from I/O Manager
//
// Return value:
//		NTSTATUS - success or failuer code
//--

NTSTATUS DispatchReadWrite (
		IN PDEVICE_OBJECT	pDevObj,
		IN PIRP				pIrp			) {
#if DBG>=1
	DbgPrint("THREADDMA: Read/Write Operation requested (DispatchWrite)\n");
#endif
	
	PIO_STACK_LOCATION pIrpStack =
		IoGetCurrentIrpStackLocation( pIrp );

	PDEVICE_EXTENSION pDE = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;

	// Check for zero-length transfers
	if ( pIrpStack->Parameters.Read.Length == 0 )  {
		pIrp->IoStatus.Status = STATUS_SUCCESS;
		pIrp->IoStatus.Information = 0;
		IoCompleteRequest( pIrp, IO_NO_INCREMENT );
		return STATUS_SUCCESS;
	}

	// Start device operation
	IoMarkIrpPending( pIrp );

	// Add the IRP to the thread's work queue
	ExInterlockedInsertTailList (
		&pDE->IrpQueueListHead,
		&pIrp->Tail.Overlay.ListEntry,
		&pDE->lkIrpQueue );

	KeReleaseSemaphore(
		&pDE->semIrpQueue,
		0,			// No priority boost
		1,			// Increment semaphore by 1
		FALSE );	// No WaitForXxx after this call
	
	return STATUS_PENDING;
}


BOOLEAN Isr (
			IN PKINTERRUPT pIntObj,
			IN PVOID pServiceContext		) {

	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pServiceContext;
	PDEVICE_OBJECT pDevObj = pDevExt->pDevice;
	PIRP pIrp = pDevObj->CurrentIrp;

	// TODO: If interrupt was not generated by our device,
	//		get out quickly by returning FALSE.
		// return FALSE;

	// If its our interrupt, deal with it
	// TODO: Dismiss HW interrupt by writing appropriate
	//			Control info to device

	// Were we expecting an interrupt?
	if (!pDevExt->bInterruptExpected)
		return TRUE;	// nope
	pDevExt->bInterruptExpected = FALSE;

	// TODO: If more data must be transferred as part
	//			of this IRP request, restart the device.
	// Otherwise, schedule the DPC to complete the IRP.
	IoRequestDpc( pDevObj, pIrp, (PVOID)pDevExt );

	return TRUE;
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
	PDEVICE_EXTENSION pDE = (PDEVICE_EXTENSION)
		pContext;
	
#if DBG>=1
	DbgPrint("THREADDMA: DpcForIsr\n");
#endif

// When the device generates an 
// interrupt, the Interrupt Service routine 
// saves the status of the hardware 
// and requests a DPC.  Eventually, DpcForIsr 
// executes and just sets an Event object into the 
// Signaled state.  PerformSynchronousTransfer 
// (which has been waiting for this Event object) wakes up and 
// continues processing the current IRP.	
	KeSetEvent( 
		&pDE->evDeviceOperationComplete,
		0,
		FALSE );
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
