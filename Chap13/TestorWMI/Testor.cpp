// Testor for Chapter 13 WMI Example Parallel Port Driver

#include <windows.h>
#include <stdio.h>

int main() {
	HANDLE hDevice;
	BOOL status;

	printf("Beginning test of WMI Example Parallel Port Driver (CH13)...\n");

	hDevice =
		CreateFile("\\\\.\\WMIEX1",
					GENERIC_READ | GENERIC_WRITE,
					0,		// share mode none
					NULL,	// no security
					OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL,
					NULL );		// no template
	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Failed to obtain file handle to device: "
			"%s with Win32 error code: %d\n",
			"WMIEX1", GetLastError() );
		return 1;
	}

	printf("Succeeded in obtaining handle to WMIEX1 device.\n");

	printf("Attempting write to device...\n");
	char outBuffer[20];
	for (DWORD i=0; i<sizeof(outBuffer); i++)
		outBuffer[i] = (char)i;
	DWORD outCount = sizeof(outBuffer);
	DWORD bW;
	status =
		WriteFile(hDevice, outBuffer, outCount, &bW, NULL);
	if (!status) {
		printf("Failed on call to WriteFile - error: %d\n",
			GetLastError() );
		return 2;
	}
	if (outCount == bW) {
		printf("Succeeded in writing %d bytes\n", outCount);
		printf("OutBuffer was: \n");
		for (i=0; i<bW; i++)
			printf("%02X ", (UCHAR)outBuffer[i]);
		printf("\n");
	} else {
		printf("Failed to write the correct number of bytes.\n"
			"Attempted to write %d bytes, but WriteFile reported %d bytes.\n",
			outCount, bW);
		return 3;
	}

	printf("Attempting to read from device...\n");
	char inBuffer[80];
	DWORD inCount = sizeof(inBuffer);
	DWORD bR;
	status =
		ReadFile(hDevice, inBuffer, inCount, &bR, NULL);
	if (!status) {
		printf("Failed on call to ReadFile - error: %d\n",
			GetLastError() );
		return 4;
	}
	if (bR == bW) {
		printf("Succeeded in reading %d bytes\n", bR);
		printf("Buffer was: \n");
		for (i=0; i<bR; i++)
			printf("%02X ", (UCHAR)inBuffer[i]);
		printf("\n");
	} else {
		printf("Failed to read the correct number of bytes.\n"
			"Should have read %d bytes, but ReadFile reported %d bytes.\n",
			bW, inCount);
		return 5;
	}

	printf("Attempting to close device WMIEX1...\n");
	status =
		CloseHandle(hDevice);
	if (!status) {
		printf("Failed on call to CloseHandle - error: %d\n",
			GetLastError() );
		return 6;
	}
	printf("Succeeded in closing device...exiting normally\n");

	printf("\nEach time this test program is run,\n");
	printf("the WMI reporting mechanism for the driver should\n");
	printf("report three pieces of information:\n");
	printf("\tTotalTransfers: +40\n");
	printf("\tTotalReads: +1\n");
	printf("\tTotalWrites: +1\n");

	printf("\nTo see this data accumulate, use the WBEM Object Browser\n");
	printf("supplied on the Platform SDK (bin/WMI directory).\n");
	printf("It is an HTML file, Browser.HTM, that allows viewing\n");
	printf("of MOF data for a selected instance.  To use the Browser:\n");

	printf("\n1.  Double-click Browser.HTM from Windows Explorer.\n");
	printf("2.  Select the \\Root\\wmi namespace. Logon as current user.\n");
	printf("3.  Select the W2KDriverBook_WMIEX class.\n");
	printf("4.  Select the first instance.\n");
	printf("5.  View the TotalXxx fields after each run of this program.\n");

	printf("\nIf the W2KDriverBook_WMIEX class does not appear in the Browser list:\n");
	printf("run \"mofcomp -N:root\\wmi WMIEx.mof\" from a cmd prompt.\n");
	printf("This will add the driver's class into the WMI namespace.\n");
	
	return 0;
}


	