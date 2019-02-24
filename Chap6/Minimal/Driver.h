// Driver.h - Chapter 6
//
// Copyright (C) 2000 by Jerry Lozano
//

#pragma once

extern "C" {
#include <NTDDK.h>
}
#include "Unicode.h"

typedef struct _DEVICE_EXTENSION {
	PDEVICE_OBJECT pDevice;
	ULONG DeviceNumber;
	CUString ustrDeviceName;	// internal name
	CUString ustrSymLinkName;	// external name
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;
