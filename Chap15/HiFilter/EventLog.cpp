//++
// File Name:
//		Eventlog.cpp
//
// Contents:
//		Routines that give kernel-mode drivers
//		access to the system Event Log.
//--

//
// Driver-specific header files...
//
#include "Driver.h"

//
// Data global to this module
//
static ULONG LogLevel;


//
// Forward declarations of local functions
//
ULONG
GetStringSize(
	IN PWSTR String
	);

//++
// Function:
//		InitializeEventLog
//
// Description:
//		This function sets things up so that
//		other parts of the driver can send
//		error messages to the Event Log.
//
// Arguments:
//		Address of Driver object
//
// Return Value:
//		(None)
//--
VOID
InitializeEventLog(
	IN PDRIVER_OBJECT pDriverObject
	)
{
	RTL_QUERY_REGISTRY_TABLE QueryTable[2];

	//
	// Fabricate a Registry query.
	//
	RtlZeroMemory( QueryTable, sizeof( QueryTable ));

	QueryTable[0].Name	= L"EventLogLevel";
	QueryTable[0].Flags	= RTL_QUERY_REGISTRY_DIRECT;
	QueryTable[0].EntryContext = &LogLevel;

	//
	// Look for the EventLogLevel value 
	// in the Registry.
	//
	if( !NT_SUCCESS( 
			RtlQueryRegistryValues(
					RTL_REGISTRY_SERVICES,
					L"HIFILTER\\Parameters",
					QueryTable,
					NULL, NULL )))
	{
		LogLevel = DEFAULT_LOG_LEVEL;
	}

	//
	// Log a message saying that logging
	// is enabled.
	//
	ReportEvent(
		LOG_LEVEL_DEBUG,
		MSG_LOGGING_ENABLED,
		ERRORLOG_INIT,
		(PVOID)pDriverObject,
		NULL,					// No IRP
		NULL, 0,				// No dump data
		NULL, 0 );				// No strings
}

//++
// Function:
//		ReportEvent
//
// Description:
//		This function puts a message in
//		the system Event log.
//
// Arguments:
//		LOG_LEVEL for this message
//		Error message code
//		Address of Driver or Device object
//		Address of IRP (or NULL)
//		Address of dump data array (or NULL)
//		Count of ULONGs in dump data array (or zero)
//		Address of array of insertion strings
//		Count of insertion strings (or zero)
//		
//
// Return Value:
//		TRUE if the error was logged
//		FALSE if there was a problem
//--
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
	)
{
	PIO_ERROR_LOG_PACKET pPacket;
	PDEVICE_EXTENSION pDE;
	PIO_STACK_LOCATION pIrpStack;
	PUCHAR pInsertionString;
	UCHAR PacketSize;
	UCHAR StringSize[ MAX_INSERTION_STRINGS ];
	ULONG i;

	//
	// If we're not logging, or the message
	// is out of range, don't do anything.
	//
	if( ( LogLevel == LOG_LEVEL_NONE ) ||
		( MessageLevel > LogLevel ))
	{
		return TRUE;
	}

	//
	// Start with minimum required packet size
	//
	PacketSize = sizeof( IO_ERROR_LOG_PACKET );

	//
	// Add in any dump data. Remember that the
	// standard error log packet already has one
	// slot in its DumpData array.
	//
	if( DumpDataCount > 0 )
		PacketSize += 
				(UCHAR)( sizeof( ULONG ) *
							( DumpDataCount - 1 )); 

	//
	// Determine total space needed for any 
	// insertion strings.
	//
	if( StringCount > 0 )
	{
		//
		// Take the lesser of what the caller sent
		// and what this routine can handle
		//
		if( StringCount > MAX_INSERTION_STRINGS )
			StringCount = MAX_INSERTION_STRINGS;

		for( i=0; i<StringCount; i++ )
		{
			//
			// Keep track of individual string sizes
			//
			StringSize[i] = 
				(UCHAR)GetStringSize( Strings[i] );

			PacketSize += StringSize[i];
		}
	}

	//
	// Try to allocate the packet
	//
	pPacket = (PIO_ERROR_LOG_PACKET)
		IoAllocateErrorLogEntry( 
					pIoObject,
					PacketSize );

	if( pPacket == NULL ) return FALSE;

	//
	// Fill in standard parts of the packet
	//
	pPacket->ErrorCode = ErrorCode;
	pPacket->UniqueErrorValue = UniqueErrorValue;

	//
	// If there's an IRP, then the IoObject must
	// be a Device object. In that case, use the
	// IRP and the Device Extension to fill in
	// additional parts of the packet. Otherwise,
	// set these additional fields to zero.
	//
	if( pIrp != NULL )
	{
		pIrpStack 
			= IoGetCurrentIrpStackLocation( pIrp );
		
		pDE = (PDEVICE_EXTENSION) 
				((PDEVICE_OBJECT)pIoObject)->
									DeviceExtension;

		pPacket->MajorFunctionCode = 
						pIrpStack->MajorFunction;

		pPacket->RetryCount = pDE->IrpRetryCount;

		pPacket->FinalStatus = pIrp->IoStatus.Status;

		pPacket->SequenceNumber = 
						pDE->IrpSequenceNumber;

		if( pIrpStack->MajorFunction == 
					IRP_MJ_DEVICE_CONTROL ||
			pIrpStack->MajorFunction ==
					IRP_MJ_INTERNAL_DEVICE_CONTROL )
		{
			pPacket->IoControlCode =
				pIrpStack->Parameters.
							DeviceIoControl.
								IoControlCode;
		}
		else pPacket->IoControlCode = 0;
	}
	else	// No IRP
	{
		pPacket->MajorFunctionCode = 0;
		pPacket->RetryCount = 0;
		pPacket->FinalStatus = 0;
		pPacket->SequenceNumber = 0;
		pPacket->IoControlCode = 0;
	}

	//
	// Add the dump data
	//
	if( DumpDataCount > 0 )
	{
		pPacket->DumpDataSize =
					(USHORT)( sizeof( ULONG ) * 
								DumpDataCount );

		for( i=0; i<DumpDataCount; i++ )
			pPacket->DumpData[i] = DumpData[i];
	}
	else pPacket->DumpDataSize = 0;

	//
	// Add the insertion strings
	//
	pPacket->NumberOfStrings = (USHORT)StringCount;

	//
	// The strings always go just after
	// the DumpData array in the packet
	//
	pPacket->StringOffset =
		sizeof( IO_ERROR_LOG_PACKET ) +
		( DumpDataCount - 1 ) * sizeof( ULONG );

	if( StringCount > 0 )
	{

		pInsertionString =
			(PUCHAR)pPacket + pPacket->StringOffset;

		for( i=0; i<StringCount; i++ )
		{
			//
			// Add each new string to the end
			// of the existing stuff
			//
			RtlCopyBytes(
				pInsertionString,
				Strings[i],
				StringSize[i] );

			pInsertionString += StringSize[i];
		}

	}

	//
	// Log the message
	//
	IoWriteErrorLogEntry( pPacket );

	return TRUE;
}

//++
// Function:
//		GetStringSize
//
// Description:
//		This function determines the amount of
//		space needed to hold a NULL-terminated
//		Unicode string.
//
// Arguments:
//		Address of string
//		
//
// Return Value:
//		Size (in bytes) of the string
//--
ULONG
GetStringSize(
	IN PWSTR String
	)
{
	UNICODE_STRING TempString;

	//
	// Use an RTL routine to get the length
	//
	RtlInitUnicodeString( &TempString, String );

	//
	// Size is actually two greater because
	// of the UNICODE_NULL at the end.
	//
	return( TempString.Length + sizeof( WCHAR));
}
