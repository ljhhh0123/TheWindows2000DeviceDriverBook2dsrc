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

// BUFFER_SIZE_INFO is a driver-defined structure
// that describes the buffers used by the filter
typedef struct _BUFFER_SIZE_INFO
{
	ULONG MaxWriteLength;
	ULONG MaxReadLength;
} BUFFER_SIZE_INFO, *PBUFFER_SIZE_INFO;


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
	DRIVER_STATE state;		// current state of driver

	// Items used for event logging
	ULONG IrpSequenceNumber;
	UCHAR IrpRetryCount;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define IOCTL_GET_MAX_BUFFER_SIZE		\
	CTL_CODE( FILE_DEVICE_UNKNOWN, 0x803,	\
		METHOD_BUFFERED, FILE_ANY_ACCESS )

// Define a really (arbitrarily) small transfer size
//		for the lower driver
#define MAX_LO_TRANSFER_SIZE 16
