//
// Driver.c - Chapter 8 - Parallel Port Driver
//
// Copyright (C) 2000 by Jerry Lozano
//

#include "Driver.h"

// Forward declarations
//
static NTSTATUS CreateDevice (
		IN PDRIVER_OBJECT	pDriverObject,
		IN ULONG			DeviceNumber,
		IN ULONG			portBase,
		IN ULONG			Irq  );

static VOID DriverUnload (
		IN PDRIVER_OBJECT	pDriverObject	);

NTSTATUS ClaimHardware(	PDRIVER_OBJECT pDriverObj,
						PDEVICE_OBJECT pDevObj,
						ULONG portBase, 
						ULONG portSpan, 
						ULONG Irq);

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
	pDriverObject->DriverStartIo = StartIo;
	
	// For each physical or logical device detected
	// that will be under this Driver's control,
	// a new Device object must be created.
	status =
		CreateDevice(pDriverObject, ulDeviceNumber,
					0x378, 0x7);
	// This call would be repeated until all devices are created

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
//		portBase - Base Address of Parallel Port Register
//		Irq - Interrupt Request Level for Parallel Port
//
// Return value:
//		NTSTATUS - Success or Failure code
//--
NTSTATUS CreateDevice (
		IN PDRIVER_OBJECT	pDriverObject,
		IN ULONG			ulDeviceNumber,
		IN ULONG			portBase,
		IN ULONG			Irq		) {

	NTSTATUS status;
	PDEVICE_OBJECT pDevObj;
	PDEVICE_EXTENSION pDevExt;

	// Form the internal Device Name
	CUString devName("\\Device\\PPORT");	// for "Parallel Port" device
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

	// Choose to use BUFFERED_IO
	pDevObj->Flags |= DO_BUFFERED_IO;

	// Initialize the Device Extension
	pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	pDevExt->pDevice = pDevObj;	// back pointer
	pDevExt->DeviceNumber = ulDeviceNumber;
	pDevExt->ustrDeviceName = devName;
	pDevExt->Irq = Irq;
	pDevExt->portBase = (PUCHAR)portBase;
	pDevExt->pIntObj = NULL;

	// Since this driver controlls real hardware,
	// the hardware controlled must be "discovered."
	// Chapter 9 will discuss auto-detection,
	// but for now we will hard-code the hardware
	// resource for the common printer port.
	// We use IoReportResourceForDetection to mark
	// PORTs and IRQs as "in use."
	// This call will fail if another driver
	// (such as the standard parallel driver(s))
	// already control the hardware
	/* status =
		ClaimHardware(pDriverObject,
					  pDevObj,
					  portBase,	// fixed port address
					  PPORT_REG_LENGTH,
					  Irq);		// fixed irq

	if (!NT_SUCCESS(status)) {
		// if it fails now, must delete Device object
		IoDeleteDevice( pDevObj );
		return status;
	}
	*/

	// We need a DpcForIsr registration
	IoInitializeDpcRequest( 
		pDevObj, 
		DpcForIsr );

	// Create & connect to an Interrupt object
	// To make interrupts real, we must translate irq into
	// a HAL irq and vector (with processor affinity)
	KIRQL kIrql;
	KAFFINITY kAffinity;
	ULONG kVector =
		HalGetInterruptVector(Isa, 0, pDevExt->Irq, 0,
							&kIrql, &kAffinity);

#if DBG==1
	DbgPrint("PPORT: Interrupt %X converted to kIrql = %X, kAffinity = %X, kVector = %X\n",
				pDevExt->Irq, kIrql, kAffinity, kVector);
#endif
	
	status =
		IoConnectInterrupt(
			&pDevExt->pIntObj,	// the Interrupt object
			Isr,			// our ISR
			pDevExt,		// Service Context
			NULL,			// no spin lock
			kVector,			// vector
			kIrql,		// DIRQL
			kIrql,		// DIRQL
			Latched,	// Latched or LevelSensitive
			TRUE,			// Shared?
			kAffinity,		// processors in an MP set
			FALSE );		// save FP registers?
	if (!NT_SUCCESS(status)) {
		// if it fails now, must delete Device object
		IoDeleteDevice( pDevObj );
		return status;
	}
	
#if DBG==1
	DbgPrint("PPORT: Interrupt successfully connected\n");
#endif
	
	// Form the symbolic link name
	CUString symLinkName("\\??\\PPT");
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

		// Delete our Interrupt object
		if (pDevExt->pIntObj)
			IoDisconnectInterrupt( pDevExt->pIntObj );

		// This will yield the symbolic link name
		UNICODE_STRING pLinkName =
			pDevExt->ustrSymLinkName;
		// ... which can now be deleted
		IoDeleteSymbolicLink(&pLinkName);
#if DBG==1
	DbgPrint("PPORT: Object %ws deleted\n",
				(PWSTR)pDevExt->ustrSymLinkName);
#endif
	
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

NTSTATUS ClaimHardware(	PDRIVER_OBJECT pDriverObj,
						PDEVICE_OBJECT pDevObj,
						ULONG portBase, 
						ULONG portSpan, 
						ULONG Irq) {

	NTSTATUS status;
	PHYSICAL_ADDRESS maxPortAddr;
	PIO_RESOURCE_REQUIREMENTS_LIST  pRRList;
	// This size is a headache.  The RRLIST already includes
	// 1 Resource List plus 1 Resource Descriptor
	// we need 1 additional Resource Descriptor (2 total)
	int rrSize = sizeof(IO_RESOURCE_REQUIREMENTS_LIST) +
				 sizeof(IO_RESOURCE_DESCRIPTOR);
	pRRList = (PIO_RESOURCE_REQUIREMENTS_LIST)
		ExAllocatePool(PagedPool, rrSize);

	RtlZeroMemory(pRRList, rrSize);
	pRRList->ListSize = rrSize;
	pRRList->AlternativeLists = 1;	// only 1 Resource List

	pRRList->InterfaceType = Isa;
	pRRList->List[0].Version = 1;
	pRRList->List[0].Revision = 1;
	pRRList->List[0].Count = 2;	// 2 Resource Descriptors: port & irq

	pRRList->List[0].Descriptors[0].Type = CmResourceTypePort;
	pRRList->List[0].Descriptors[0].ShareDisposition = CmResourceShareDeviceExclusive;
	pRRList->List[0].Descriptors[0].Flags = CM_RESOURCE_PORT_IO;
	pRRList->List[0].Descriptors[0].u.Port.Length = portSpan;
	pRRList->List[0].Descriptors[0].u.Port.Alignment = FILE_WORD_ALIGNMENT;
	maxPortAddr.QuadPart = portBase;
	pRRList->List[0].Descriptors[0].u.Port.MinimumAddress = maxPortAddr;
	maxPortAddr.LowPart += portSpan-1;
	pRRList->List[0].Descriptors[0].u.Port.MaximumAddress = maxPortAddr;

	pRRList->List[0].Descriptors[1].Type = CmResourceTypeInterrupt;
	pRRList->List[0].Descriptors[1].ShareDisposition = CmResourceShareDeviceExclusive;
	pRRList->List[0].Descriptors[1].Flags = CM_RESOURCE_INTERRUPT_LATCHED;
	pRRList->List[0].Descriptors[1].u.Interrupt.MinimumVector = Irq;
	pRRList->List[0].Descriptors[1].u.Interrupt.MaximumVector = Irq;

	status =
		IoReportDetectedDevice(
			pDriverObj,		// DriverObject
			Isa,			// Bus type
			-1,				// Bus number
			-1,				// SlotNumber
			NULL,			// Driver RESOURCE_LIST
			pRRList,		// Device Resource List
			FALSE,			// Already claimed?
			&pDevObj );		// device object

	ExFreePool(pRRList);
	return status;
}

//++
// Function:	DispatchCreate
//
// Description:
//		Handles call from Win32 CreateFile request
//		Does nothing.
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
//		For PPort driver, frees any buffer
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

#if DBG==1
	DbgPrint("PPORT: IRP Canceled\n");
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
//		For PPort driver, "starts" the device by
//		indirectly calling StartIo.
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
#if DBG==1
	DbgPrint("PPORT: Write Operation requested (DispatchWrite)\n");
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
//		For PPort driver, xfers pool buffer to user
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
	
#if DBG==1
	DbgPrint("PPORT: Read Operation requested (DispatchRead)\n");
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

#if dbg==1
	DbgPrint("PPORT: Interrupt Service Routine\n");
#endif

	UCHAR status = ReadStatus( pDevExt );
	if ((status & STS_NOT_IRQ))
		return FALSE;

	// its our interrupt, deal with it
	WriteControl( pDevExt, CTL_DEFAULT);
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
#if DBG==1
	DbgPrint("PPORT: TransmitByte: Sending 0x%02X (%c) to port %X\n",
				nextByte, nextByte, pDevExt->portBase);
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
#if DBG==1
	DbgPrint("PPORT: Wrote to Control: 0x%02X\n", bits);
#endif

	// Now read the data from the loopback
	UCHAR status =
		ReadStatus( pDevExt );
#if DBG==1
	DbgPrint("PPORT: Read From Status: 0x%03X\n", status);
#endif

	// Format the nibble into the upper half of the byte
	UCHAR readByte =
		((status & 0x8)<< 1) |
		((status & 0x30)<<1) |
		(status & 0x80);
	pDevExt->deviceBuffer[pDevExt->xferCount++] =
		readByte;
#if DBG==1
	DbgPrint("PPORT: TransmitByte read character: 0x%03X (%c)\n",
				readByte, readByte);
#endif

	// Force an interrupt
#if DBG==1
	DbgPrint("PPORT: TransmitByte: generating interrupt.\n");
#endif
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
#if DBG==1
	DbgPrint("PPORT: Start I/O Operation\n");
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
#if DBG==1
	DbgPrint("PPORT: StartIO: Transmitting first byte of %d\n", pDevExt->deviceBufferSize);
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
	
#if DBG==1
	DbgPrint("PPORT: DpcForIsr, xferCount = %d\n",
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
