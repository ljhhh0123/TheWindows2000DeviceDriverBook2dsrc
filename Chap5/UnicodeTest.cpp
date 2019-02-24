// Unicode.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "DDKTestEnv.h"
#include "Unicode.h"
#include "stdio.h"

int main(int argc, char* argv[])
{
	CUString strEmpty;
	CUString strOne("One");
	CUString strTwo(L"Two");

	CUString newCopyOfOne = strOne;
	CUString strTwoCopy;
	strTwoCopy = strTwo;

	CUString str2468("2468");
	ULONG ul2468 = str2468;
	CUString strxFF01("xFF01");
	ULONG ulxFF01 = strxFF01;
	CUString stro6622("o6622");
	ULONG ulo6622 = stro6622;

	CUString str2244(2244);

	CUString strOnePlusTwo = strOne + strTwo;

	printf("When comparing strTwo to strTwoCopy, we get %d\n",
				strTwo==strTwoCopy);
	printf("When comparing strOne to strTwo, we get %d\n",
				strOne==strTwo);

	wprintf(L"Casting strTwo to wchar_t*, we get: %s\n",
				(wchar_t*) strTwo);

	printf("Final results:\n");

	wprintf(L"strOne: %s\n", (PWSTR) strOne);
	wprintf(L"strTwo: %s\n", (PWSTR) strTwo);
	wprintf(L"strEmpty: %s\n", (PWSTR) strEmpty);
	wprintf(L"newCopyOfOne: %s\n", (PWSTR) newCopyOfOne);
	wprintf(L"strTwoCopy: %s\n", (PWSTR) strTwoCopy);
	wprintf(L"strOnePlusTwo: %s\n", (PWSTR) strOnePlusTwo);
	printf("Conversion of str2468 into ULONG = %d\n", ul2468);
	printf("Conversion of strxFF01 into ULONG = %x (%d)\n", ulxFF01, ulxFF01);
	printf("Conversion of stro6622 into ULONG = %o (%d)\n", ulo6622, ulo6622);

	wprintf(L"Conversion of 2244 into CUString = %s\n", (PWSTR) str2244);
	wprintf(L"On the fly conversion of 3366 into CUString = %s\n",
					(PWSTR)(CUString)3366);

	printf("Test of buffer access using [] operator:\n");
	for (int i=0; i<strOnePlusTwo.Length(); i++) {
		wprintf(L"%c ", strOnePlusTwo[i]);
		strOnePlusTwo[i] = L'A' + i;
	}
	wprintf(L"\nAfter replacing buffer, strOnePlusTwo = %s\n", (PWSTR)strOnePlusTwo);

	return 0;
}
