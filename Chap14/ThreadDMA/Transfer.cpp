//
// Transfer.cpp - Thread-based DMA Driver Transfer routine
//
// Copyright (C) 2000 by Jerry Lozano
//

#include "Driver.h"

//
// Forward declarations
//

static NTSTATUS AcquireAdapterObject(
	IN PDEVICE_EXTENSION pDE,
	IN ULONG MapRegsNeeded
	);

static NTSTATUS PerformSynchronousTransfer( 
			IN PDEVICE_OBJECT pDevObj,
			IN PIRP pIrp );

IO_ALLOCATION_ACTION AdapterControl(
    IN PDEVICE_OBJECT pDevObj,
    IN PIRP pIrp,
    IN PVOID MapRegisterBase,
    IN PVOID pContext
    );


CCHAR PerformDataTransfer(
	IN PDEVICE_OBJECT pDevObj,
	IN PIRP pIrp
	)
{
	PIO_STACK_LOCATION	pIrpStack = 
		IoGetCurrentIrpStackLocation( pIrp );
		
	PDEVICE_EXTENSION pDE = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;

	PMDL pMdl = pIrp->MdlAddress;
	ULONG MapRegsNeeded;
	NTSTATUS status;

	// Set the I/O direction flag
	if( pIrpStack->MajorFunction == IRP_MJ_WRITE )
		pDE->bWriteToDevice = TRUE;
	else
		pDE->bWriteToDevice = FALSE;

	// Set up bookkeeping values
	pDE->bytesRequested = 
			MmGetMdlByteCount( pMdl );

	pDE->bytesRemaining = 
			pDE->bytesRequested;

	pDE->transferVA = (PUCHAR)
			MmGetMdlVirtualAddress( pMdl );

	// Flush CPU cache if necessary
	KeFlushIoBuffers(
		pIrp->MdlAddress,
		!pDE->bWriteToDevice,
		TRUE );

	// Calculate size of first partial transfer
	pDE->transferSize = pDE->bytesRemaining;

	MapRegsNeeded = 
		ADDRESS_AND_SIZE_TO_SPAN_PAGES(
			pDE->transferVA,
			pDE->transferSize );

	if( MapRegsNeeded > pDE->mapRegisterCount )
	{
		MapRegsNeeded = 
			pDE->mapRegisterCount;

		pDE->transferSize = 
			MapRegsNeeded * PAGE_SIZE - 
			MmGetMdlByteOffset( pMdl );
	}

	// Acquire the adapter object.
	status = AcquireAdapterObject( 
				pDE, 
				MapRegsNeeded );
	if( !NT_SUCCESS( status )) {
		pIrp->IoStatus.Status = status;
		pIrp->IoStatus.Information = 0;
		return IO_NO_INCREMENT;
	}

	// Try to perform the first partial transfer
	status = 
		PerformSynchronousTransfer( 
			pDevObj,
			pIrp );

	if( !NT_SUCCESS( status )) {
		pDE->pDmaAdapter->DmaOperations->
			FreeAdapterChannel( pDE->pDmaAdapter );
		pIrp->IoStatus.Status = status;
		pIrp->IoStatus.Information = 0;
		return IO_NO_INCREMENT;
	}

	// It worked. Update the bookkeeping information
	pDE->transferVA += pDE->transferSize;
	pDE->bytesRemaining -= pDE->transferSize;

	// Loop through all the partial transfer 
	// operations for this request.
	while( pDE->bytesRemaining >0 )
	{
		// Try to do all of it in one operation
		pDE->transferSize = pDE->bytesRemaining;

		MapRegsNeeded = 
			ADDRESS_AND_SIZE_TO_SPAN_PAGES(
					pDE->transferVA,
					pDE->transferSize );

		// If the remainder of the buffer is more
		// than we can handle in one I/O. Reduce
		// our expectations.
		if (MapRegsNeeded > pDE->mapRegisterCount) {
			MapRegsNeeded = pDE->mapRegisterCount;

			pDE->transferSize = 
				MapRegsNeeded * PAGE_SIZE -
					BYTE_OFFSET(pDE->transferVA);
		}

		// Try to perform a device operation.
		status = 
			PerformSynchronousTransfer( 
				pDevObj,
				pIrp );

		if( !NT_SUCCESS( status )) break;

		// It worked. Update the bookkeeping
		// information for the next cycle.
		pDE->transferVA += pDE->transferSize;
		pDE->bytesRemaining -= pDE->transferSize;
	}

	// After the last partial transfer is done,
	// release the DMA Adapter object .
	pDE->pDmaAdapter->DmaOperations->
		FreeAdapterChannel( pDE->pDmaAdapter );

	// Send the IRP back to the caller. Its final
	// status is the status of the last transfer
	// operation. 
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 
				pDE->bytesRequested -
					pDE->bytesRemaining;
	// Since there has been at least one I/O
	// operation, give the IRP a priority boost.
	//
	return IO_DISK_INCREMENT;
}

static NTSTATUS AcquireAdapterObject(
	IN PDEVICE_EXTENSION pDE,
	IN ULONG MapRegsNeeded
	) {
	KIRQL OldIrql;
	NTSTATUS status;

	// We must be at DISPATCH_LEVEL in order
	// to request the Adapter object
	KeRaiseIrql( DISPATCH_LEVEL, &OldIrql );

	status = 
		pDE->pDmaAdapter->DmaOperations->
			AllocateAdapterChannel(
				pDE->pDmaAdapter,
				pDE->pDevice,
				MapRegsNeeded,
				AdapterControl,
				pDE );

	KeLowerIrql( OldIrql );

	// If the call failed, it's because there
	// weren't enough mapping registers.
	if( !NT_SUCCESS( status ))
		return status;
			
	// Stop and wait for the Adapter Control
	// routine to set the Event object. This is
	// our signal that the Adapter object is ours.
	KeWaitForSingleObject(
		&pDE->evAdapterObjectIsAcquired,
		Executive,
		KernelMode,
		FALSE,
		NULL );

	return STATUS_SUCCESS;
}

IO_ALLOCATION_ACTION AdapterControl(
    IN PDEVICE_OBJECT pDevObj,
    IN PIRP pIrp,
    IN PVOID MapRegisterBase,
    IN PVOID pContext
    )
{
	PDEVICE_EXTENSION pDE = (PDEVICE_EXTENSION)
		pContext;
	
	// Save the handle to the mapping
	// registers. The thread will need it
	// to set up data transfers.
	//
	pDE->mapRegisterBase = MapRegisterBase;

	// Let the thread know that its Device
	// object the Adapter object
	KeSetEvent( 
		&pDE->evAdapterObjectIsAcquired,
		0,
		FALSE );

	return KeepObject;
}

NTSTATUS PerformSynchronousTransfer(
	IN PDEVICE_OBJECT pDevObj,
	IN PIRP pIrp
	) {
	PDEVICE_EXTENSION pDE = (PDEVICE_EXTENSION)
		pDevObj->DeviceExtension;

	// Set up the system DMA controller
	// attached to this device.
	pDE->pDmaAdapter->DmaOperations->
		MapTransfer(
			pDE->pDmaAdapter,
			pIrp->MdlAddress,
			pDE->mapRegisterBase,
			pDE->transferVA,
			&pDE->transferSize,
			pDE->bWriteToDevice );

	// Start the device
	WriteControl( 
		pDE, 
		CTL_INTENB | CTL_DMA_GO );

	// The DPC routine will set an Event
	// object when the I/O operation is
	// done. Stop here and wait for it.
	KeWaitForSingleObject(
		&pDE->evDeviceOperationComplete,
		Executive,
		KernelMode,
		FALSE,
		NULL );

	// Flush data out of the Adapater
	// object cache.
	pDE->pDmaAdapter->DmaOperations->
		FlushAdapterBuffers(
			pDE->pDmaAdapter,
			pIrp->MdlAddress,
			pDE->mapRegisterBase,
			pDE->transferVA,
			pDE->transferSize,
			pDE->bWriteToDevice );

	// Check for device errors
	if( !STS_OK( pDE->DeviceStatus ))
		return STATUS_DEVICE_DATA_ERROR;
	else
		return STATUS_SUCCESS;
}
