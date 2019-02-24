// Driver.h - Chapter 12
//
// Copyright (C) 2000 by Jerry Lozano
//

#pragma once

extern "C" {
#include <WDM.h>
}
#include "Unicode.h"

enum DRIVER_STATE {Stopped, Started, Removed};

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
	DRIVER_STATE state;		// current state of driver

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
	BOOLEAN bWriting;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define MAX_DMA_LENGTH (4096 * 4)

#define DEVICE_FAIL( pDevExt ) FALSE