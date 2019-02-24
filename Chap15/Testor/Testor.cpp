// Testor for Chapter 15 Filter Driver

#include <windows.h>
#include <stdio.h>

#define IOCTL_GET_MAX_BUFFER_SIZE		\
	CTL_CODE( FILE_DEVICE_UNKNOWN, 0x803,	\
		METHOD_BUFFERED, FILE_ANY_ACCESS )

// BUFFER_SIZE_INFO is a driver-defined structure
// that describes the buffers used by the filter
typedef struct _BUFFER_SIZE_INFO
{
	ULONG MaxWriteLength;
	ULONG MaxReadLength;
} BUFFER_SIZE_INFO, *PBUFFER_SIZE_INFO;


int main() {
	HANDLE hDevice;
	BOOL status;

	printf("Beginning test of Filter Driver (CH15)...\n");

	hDevice =
		CreateFile("\\\\.\\LODRV1",
					GENERIC_READ | GENERIC_WRITE,
					0,		// share mode none
					NULL,	// no security
					OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL,
					NULL );		// no template
	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Failed to obtain file handle to device: "
			"%s with Win32 error code: %d\n",
			"LODRV1", GetLastError() );
		return 1;
	}

	printf("Succeeded in obtaining handle to LODRV1 device.\n");

	printf("Attempting DeviceIoControl request...\n");
	BUFFER_SIZE_INFO bufferInfo;
	DWORD bR;
	BOOL bSuccess =
		DeviceIoControl(hDevice, IOCTL_GET_MAX_BUFFER_SIZE,
						NULL, 0,	// input buffer
						&bufferInfo, sizeof(bufferInfo),
						&bR, NULL);
	if (bSuccess)
		printf("Succeeded DeviceIoControl. MaxWriteLength = %d, MaxReadLength = %d\n",
			bufferInfo.MaxWriteLength, bufferInfo.MaxReadLength);
	else
		printf("Failed call to DeviceIoControl, error = %X\n",
				GetLastError());

	printf("Attempting write to device with 16 characters...\n");
	char outBuffer[50];
	for (DWORD i=0; i<sizeof(outBuffer); i++)
		outBuffer[i] = (char)i;
	DWORD outCount = 16;
	DWORD bW;
	status =
		WriteFile(hDevice, outBuffer, outCount, &bW, NULL);
	if (!status) {
		printf("Failed on call to WriteFile - error: %d\n",
			GetLastError() );
	}
	if (outCount == bW) {
		printf("Succeeded in writing %d bytes\n", outCount);
	} else {
		printf("Failed to write the correct number of bytes.\n"
			"Attempted to write %d bytes, but WriteFile reported %d bytes.\n",
			outCount, bW);
		return 3;
	}

	printf("Attempting to write 50 bytes to device...\n");
	outCount = 50;
	status =
		WriteFile(hDevice, outBuffer, outCount, &bW, NULL);
	if (!status) {
		printf("Failed on call to WriteFile - error: %X\n",
			GetLastError() );
		printf("Filter may not be installed?\n");
	}
	if (outCount == bW) {
		printf("Succeeded in writing %d bytes\n", outCount);
	} else {
		printf("Failed to write the correct number of bytes.\n"
			"Attempted to write %d bytes, but WriteFile reported %d bytes.\n",
			outCount, bW);
	}

	printf("Attempting to close device LODRV1...\n");
	status =
		CloseHandle(hDevice);
	if (!status) {
		printf("Failed on call to CloseHandle - error: %d\n",
			GetLastError() );
		return 6;
	}
	printf("Succeeded in closing device...exiting normally\n");
	return 0;
}


	