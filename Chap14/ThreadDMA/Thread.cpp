//
// Thread.cpp - Thread-based DMA Driver Thread routines
//
// Copyright (C) 2000 by Jerry Lozano
//

#include "Driver.h"

// Forward declarations
//

CCHAR PerformDataTransfer(
					PDEVICE_OBJECT pDevObj, 
					PIRP pIrp );


VOID WorkerThreadMain( IN PVOID pContext ) {
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
		pContext;

	PDEVICE_OBJECT pDeviceObj = 
			pDevExt->pDevice;

	PLIST_ENTRY ListEntry;
	PIRP pIrp;
	CCHAR PriorityBoost;

	// Worker thread runs at higher priority than
	//	user threads - it sets its own priority
	KeSetPriorityThread(
		KeGetCurrentThread(),
		LOW_REALTIME_PRIORITY );

	// Now enter the main IRP-processing loop
	while( TRUE )
	{
		// Wait indefinitely for an IRP to appear in
		// the work queue or for the RemoveDevice
		// routine to stop the thread.
		KeWaitForSingleObject(
			&pDevExt->semIrpQueue,
			Executive,
			KernelMode,
			FALSE,
			NULL );

		// See if thread was awakened because
		// device is being removed
		if( pDevExt->bThreadShouldStop )
			PsTerminateSystemThread(STATUS_SUCCESS);

		// It must be a real request. Get an IRP
		ListEntry = 
			ExInterlockedRemoveHeadList(
				&pDevExt->IrpQueueListHead,
				&pDevExt->lkIrpQueue);

		pIrp = CONTAINING_RECORD(
				ListEntry,
				IRP,
				Tail.Overlay.ListEntry );

		// Process the IRP. This is a synchronous
		// operation, so this function doesn't return
		// until it's time to get rid of the IRP.
		PriorityBoost = 
			PerformDataTransfer(
				pDeviceObj, 
				pIrp );

		// Release the IRP and go back to the
		// top of the loop to see if there's
		// another request waiting.
		IoCompleteRequest( pIrp, PriorityBoost );

	} // end of while-loop
}

VOID KillThread( IN PDEVICE_EXTENSION pDE ) {
	// Set the Stop flag
	pDE->bThreadShouldStop = TRUE;

	// Make sure the thread wakes up 
	KeReleaseSemaphore(
		&pDE->semIrpQueue,
		0,			// No priority boost
		1,			// Increment semaphore by 1
		TRUE );		// WaitForXxx after this call

	// Wait for the thread to terminate
	KeWaitForSingleObject(
		&pDE->pThreadObj,
		Executive,
		KernelMode,
		FALSE,
		NULL );

	ObDereferenceObject( &pDE->pThreadObj );
}
