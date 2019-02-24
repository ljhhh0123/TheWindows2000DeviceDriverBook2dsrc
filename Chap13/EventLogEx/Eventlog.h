// File Name:
//		eventlog.h
//
// Contents:
//		Constants, structures, and function
//		declarations used for event logging.
//
#pragma once
//
// Header files
//
#include <ntddk.h>

//
// Various constants
//
// Error-logging verbosity levels
//
#define LOG_LEVEL_NONE			0
#define LOG_LEVEL_NORMAL		1
#define LOG_LEVEL_DEBUG			2

#define  DEFAULT_LOG_LEVEL	LOG_LEVEL_DEBUG

//
// Unique values identifying location
// of an error within the driver 
//
#define ERRORLOG_INIT				1
#define ERRORLOG_DISPATCH			2
#define ERRORLOG_STARTIO			3
#define ERRORLOG_CONTROLLER_CONTROL	4
#define ERRORLOG_ADAPTER_CONTROL	5
#define ERRORLOG_ISR				6
#define ERRORLOG_DPC_FOR_ISR		7
#define ERRORLOG_UNLOAD				8
#define ERRORLOG_PNP				9

//
// Largest number of insertion strings
// allowed in one message
//
#define MAX_INSERTION_STRINGS		20

//
// Prototypes for globally defined functions...
//
VOID
InitializeEventLog(
	IN PDRIVER_OBJECT pDriverObject
	);

BOOLEAN
ReportEvent(
	IN ULONG	MessageLevel,
	IN NTSTATUS	ErrorCode,
	IN ULONG	UniqueErrorValue,
	IN PVOID	pIoObject,
	IN PIRP		pIrp,
	IN ULONG	DumpData[],
	IN ULONG	DumpDataCount,
	IN PWSTR	Strings[],
	IN ULONG	StringCount
	);


