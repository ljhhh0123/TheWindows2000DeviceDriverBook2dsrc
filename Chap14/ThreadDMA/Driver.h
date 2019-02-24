// File Name:
//		Driver.h
//
// Contents:
//		Constants, structures, and function
//		declarations specific to this driver.
//
#pragma once
//
// Header files
//
extern "C" {
#include <WDM.h>
}
#include "Unicode.h"
#include "EventLog.h"
#include "Msg.h"

enum DRIVER_STATE {Stopped, Started, Removed};

//++
// Description:
//		Driver-defined structure used to hold 
//		miscellaneous device information.
//
// Access:
//		Allocated from NON-PAGED POOL
//		Available at any IRQL
//--
typedef struct _DEVICE_EXTENSION {
	PDEVICE_OBJECT pDevice;
	PDEVICE_OBJECT pLowerDevice;
	ULONG DeviceNumber;
	CUString ustrDeviceName;	// internal name
	CUString ustrSymLinkName;	// external name
	PUCHAR portBase;				// I/O register address
	ULONG  portLength;
	KIRQL IRQL;					// Irq for parallel port
	ULONG Vector;
	KAFFINITY Affinity;
	PKINTERRUPT pIntObj;	// the interrupt object
	BOOLEAN bInterruptExpected;	// TRUE iff this driver is expecting interrupt
	ULONG DeviceStatus;		// Mythical device HW status
	DRIVER_STATE state;		// current state of driver

	// Pointer to worker thread object
	PETHREAD pThreadObj;	
	// Flag set to TRUE when worker thread should quit
	BOOLEAN bThreadShouldStop;

	// Event object signaling Adapter is now owned
	KEVENT evAdapterObjectIsAcquired;
	// Event signaling last operation now completed
	KEVENT evDeviceOperationComplete;

	// The work queue of IRPs is managed by this
	//	semaphore and spin lock
	KSEMAPHORE semIrpQueue;
	KSPIN_LOCK lkIrpQueue;
	LIST_ENTRY IrpQueueListHead;

	PDMA_ADAPTER pDmaAdapter;
	ULONG mapRegisterCount;
	ULONG dmaChannel;

	// This is the "handle" assigned to the map registers
	// when the AdapterControl routine is called back
	PVOID mapRegisterBase;

	ULONG bytesRequested;
	ULONG bytesRemaining;
	ULONG transferSize;
	PUCHAR transferVA;

	// This flag is TRUE if writing, FALSE if reading
	BOOLEAN bWriteToDevice;

	// Items used for event logging
	ULONG IrpSequenceNumber;
	UCHAR IrpRetryCount;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define MAX_DMA_LENGTH (4096 * 4)

// These are registers for a mythical piece of HW
#define DATA_REG	0
#define STATUS_REG	1
#define CONTROL_REG	2

//
// Status register bits: (also mythical)
//
#define STS_IRQ	0x04	// 0 Int requested
#define STS_ERR	0x08	// 1 Error

#define STS_OK(devStatus) (!(devStatus)&STS_ERR == 0)

//
// Control register bits: (mythical)
//
#define CTL_INTENB	0x10	// 1 Interrupt enable
#define CTL_DMA_GO	0x40	// 1 Start the device's DMA operation

//
// Define access macros for registers. Each macro takes
// a pointer to a Device Extension as an argument
//
#define ReadStatus( pDevExt )				\
(READ_PORT_UCHAR( 							\
  pDevExt->portBase + STATUS_REG ))

#define ReadControl( pDevExt )			\
(READ_PORT_UCHAR( 							\
  pDevExt->PortBase + CONTROL_REG ))

#define WriteControl( pDevExt, bData )	\
(WRITE_PORT_UCHAR( 							\
  pDevExt->portBase + CONTROL_REG, bData ))

#define WriteData( pDevExt, bData )		\
(WRITE_PORT_UCHAR( 						\
  pDevExt->portBase + DATA_REG, bData ))
