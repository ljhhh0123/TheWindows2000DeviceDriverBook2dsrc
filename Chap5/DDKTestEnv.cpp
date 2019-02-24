// DDKTestEnv.cpp
//
// Copyright (C) 2000 by Jerry Lozano
//

#include "stdafx.h"
#include <wchar.h>
#include "DDKTestEnv.h"

VOID ExFreePool(IN PVOID P) {
	free(P);
}

ULONG RtlAnsiStringToUnicodeSize(PANSI_STRING AnsiString ) {
	ULONG len =
		MultiByteToWideChar(CP_ACP, 0, AnsiString->Buffer, -1, NULL, 0);
	return len*2;
}

NTSTATUS 
  RtlAnsiStringToUnicodeString(
  IN OUT PUNICODE_STRING  DestinationString,
  IN PANSI_STRING  SourceString,
  IN BOOLEAN  AllocateDestinationString
  ) {
	int len =
		RtlAnsiStringToUnicodeSize(SourceString);
	if (AllocateDestinationString) {
		DestinationString->Buffer = (PWSTR)
			malloc(len);
		DestinationString->MaximumLength = len;
	}
	MultiByteToWideChar(CP_ACP, 0, SourceString->Buffer, -1, DestinationString->Buffer, len);
	DestinationString->Length = len-2;
	return 0;
}

VOID 
  RtlInitUnicodeString(
  IN OUT PUNICODE_STRING  DestinationString,
  IN PCWSTR  SourceString
  ) {
	DestinationString->Buffer = (PWSTR) SourceString;
	DestinationString->Length = wcslen(SourceString)*2;
	DestinationString->MaximumLength = DestinationString->Length + 2;
}

VOID 
  RtlInitAnsiString(
  IN OUT PANSI_STRING  DestinationString,
  IN PCSZ  SourceString
  ) {
	DestinationString->Buffer = (PSTR) SourceString;
	DestinationString->Length = strlen(SourceString);
	DestinationString->MaximumLength = DestinationString->Length + 1;
}

NTSTATUS 
  RtlAppendUnicodeStringToString(
  IN OUT PUNICODE_STRING  Destination,
  IN PUNICODE_STRING  Source
  ) {
	int srcLen = min(Destination->MaximumLength - Destination->Length - 2,
						Source->Length);
	Destination->Buffer[Destination->Length/2] = UNICODE_NULL;
	Destination->Length += srcLen;
	wcsncat(Destination->Buffer, Source->Buffer, srcLen/2);

	return 0;
}


PVOID ExAllocatePoolWithTag(IN POOL_TYPE  PoolType,
							IN SIZE_T  NumberOfBytes,
							IN ULONG  Tag ) {
	return malloc(NumberOfBytes);
}

BOOLEAN 
  RtlEqualUnicodeString(
  IN CONST UNICODE_STRING  *String1,
  IN CONST UNICODE_STRING  *String2,
  IN BOOLEAN  CaseInSensitive
  ) {
	if (String1->Length != String2->Length)
		return FALSE;
	if (CaseInSensitive)
		return wcsicmp(String1->Buffer, String2->Buffer) == 0;
	else
		return wcscmp(String1->Buffer, String2->Buffer) == 0;
}

VOID 
  RtlCopyUnicodeString(
  IN OUT PUNICODE_STRING  DestinationString,
  IN PUNICODE_STRING  SourceString
  ) {
	int copyLen = min(SourceString->Length, 
						DestinationString->MaximumLength);
	wcsncpy(DestinationString->Buffer, SourceString->Buffer, copyLen/2);
	DestinationString->Length = copyLen;
}

NTSTATUS
  RtlUnicodeStringToInteger(
  IN PUNICODE_STRING  String,
  IN ULONG  Base  OPTIONAL,
  OUT PULONG  Value
  ) {
	if (Base > 0 && Base <= 16) {
		*Value = 0;
		PWSTR next = String->Buffer;
		SHORT digitLimit = min(9, (SHORT)Base-1);
		while (next < &String->Buffer[String->Length/2]) {
			*Value *= Base;
			if (*next >= L'0' && *next <= L'0'+digitLimit)
				*Value += *next - L'0';
			if (Base > 10)
				if (*next >= L'A' && *next <= L'A' + Base-11)
					*Value += *next - L'A' + 10;
				else if (*next >= L'a' && *next <= L'a' + Base-11)
					*Value += *next - L'a' + 10;
			next++;
		}
	} else if (String->Buffer[0] == L'X'  ||
		String->Buffer[0] == L'x') {
		*Value = 0;
		PWSTR next = &String->Buffer[1];
		while (next < &String->Buffer[String->Length/2]) {
			*Value *= 16;
			if (*next >= L'0' && *next <= L'9')
				*Value += *next - L'0';
			else if (*next >= L'A' && *next <= L'F')
				*Value += *next - L'A' + 10;
			else if (*next >= L'a' && *next <= L'f')
				*Value += *next - L'a' + 10;
			next++;
		}
	} else if (String->Buffer[0] == L'O' ||
		String->Buffer[0] == L'o') {
		*Value = 0;
		PWSTR next = &String->Buffer[1];
		while (next < &String->Buffer[String->Length/2]) {
			*Value *= 8;
			if (*next >= L'0' && *next <= L'7')
				*Value += *next - L'0';
			next++;
		}
	} else {
		// the standard base 10 case
		*Value = _wtoi(String->Buffer);
	}
	return 0;
}

NTSTATUS 
  RtlIntegerToUnicodeString(
  IN ULONG  Value,
  IN ULONG  Base  OPTIONAL,
  IN OUT PUNICODE_STRING  String
  ) {
	WCHAR buffer[32];
	USHORT len =
		swprintf(buffer, L"%d", Value);
	len *= 2;	// now in bytes
	len = min(len, String->MaximumLength);
	wcsncpy(String->Buffer, buffer, len);
	String->Length = len;
	
	return 0;
}
