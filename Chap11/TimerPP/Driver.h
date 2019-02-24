// Driver.h - Chapter 11
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
	PUCHAR deviceBuffer;		// temporary pool buffer
	ULONG deviceBufferSize;
	ULONG xferCount;			// current transfer count
	ULONG maxXferCount;			// requested xfer count
	PUCHAR portBase;				// I/O register address
	ULONG  portLength;
	DRIVER_STATE state;		// current state of driver

	KDPC pollingDPC;	// reserve custom DPC object
	KTIMER pollingTimer;// and the timer object
	LARGE_INTEGER pollingInterval;	// timeout counter in uS

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

// Define the interval between polls of device in uS
#define POLLING_INTERVAL 100

#define DATA_REG	0
#define STATUS_REG	1
#define CONTROL_REG	2

//
// Status register bits:
//
#define STS_NOT_IRQ	0x04	// 0 Int requested
#define STS_NOT_ERR	0x08	// 0 Error
#define STS_SELCTD	0x10	// 1 Selected
#define STS_PE	0x20		// 1 Paper empty
#define STS_NOT_ACK	0x40	// 0 Acknowledge
#define STS_NOT_BSY	0x80	// 0 Printer busy

//
// Control register bits:
//
#define CTL_STROBE	0x01	// 1 Strobe data
#define CTL_AUTOLF	0x02	// 1 Auto line feed
#define CTL_NOT_RST	0x04	// 0 Reset printer
#define CTL_SELECT	0x08	// 1 Select printer
#define CTL_INTENB	0x10	// 1 Interrupt enable
#define	CTL_DEFAULT	0xC0	// (Sets unused bits)

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
