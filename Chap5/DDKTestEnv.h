// DDKTestEnv.h
//
// Copyright (C) 2000 by Jerry Lozano
//

// Simple Win32 DDK Test Environment

#include "stdafx.h"

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;

#define IN
#define OUT
typedef DWORD NTSTATUS;

enum POOL_TYPE {PagedPool, NonPagedPool};

typedef struct _STRING {
    USHORT Length;
    USHORT MaximumLength;
    PCHAR Buffer;
} STRING;
typedef STRING *PSTRING;

typedef STRING ANSI_STRING;
typedef PSTRING PANSI_STRING;
typedef const char *PCSZ;

VOID ExFreePool(IN PVOID P);

ULONG RtlAnsiStringToUnicodeSize(PANSI_STRING AnsiString );

NTSTATUS 
  RtlAnsiStringToUnicodeString(
  IN OUT PUNICODE_STRING  DestinationString,
  IN PANSI_STRING  SourceString,
  IN BOOLEAN  AllocateDestinationString
  );

VOID 
  RtlInitUnicodeString(
  IN OUT PUNICODE_STRING  DestinationString,
  IN PCWSTR  SourceString
  );

VOID 
  RtlInitAnsiString(
  IN OUT PANSI_STRING  DestinationString,
  IN PCSZ  SourceString
  );

NTSTATUS 
  RtlAppendUnicodeStringToString(
  IN OUT PUNICODE_STRING  Destination,
  IN PUNICODE_STRING  Source
  );


PVOID ExAllocatePoolWithTag(IN POOL_TYPE  PoolType,
							IN SIZE_T  NumberOfBytes,
							IN ULONG  Tag );

BOOLEAN 
  RtlEqualUnicodeString(
  IN CONST UNICODE_STRING  *String1,
  IN CONST UNICODE_STRING  *String2,
  IN BOOLEAN  CaseInSensitive
  );

VOID 
  RtlCopyUnicodeString(
  IN OUT PUNICODE_STRING  DestinationString,
  IN PUNICODE_STRING  SourceString
  );

NTSTATUS
  RtlUnicodeStringToInteger(
  IN PUNICODE_STRING  String,
  IN ULONG  Base  OPTIONAL,
  OUT PULONG  Value
  );

NTSTATUS 
  RtlIntegerToUnicodeString(
  IN ULONG  Value,
  IN ULONG  Base  OPTIONAL,
  IN OUT PUNICODE_STRING  String
  );
