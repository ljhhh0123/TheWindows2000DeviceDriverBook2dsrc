;
;//File: Msg.mc
;//
;
MessageIdTypedef = NTSTATUS

SeverityNames = (
	Success		  = 0x0:STATUS_SEVERITY_SUCCESS
	Informational = 0x1:STATUS_SEVERITY_INFORMATIONAL
	Warning		  = 0x2:STATUS_SEVERITY_WARNING
	Error		  = 0x3:STATUS_SEVERITY_ERROR
	)

FacilityNames = (
	System		= 0x0
	RpcRuntime	= 0x2:FACILITY_RPC_RUNTIME
	RpcStubs	= 0x3:FACILITY_RPC_STUBS
	Io			= 0x4:FACILITY_IO_ERROR_CODE
	Driver	=	  0x7:FACILITY_DRIVER_ERROR_CODE
	)


MessageId=0x0001
Facility=Driver
Severity=Informational
SymbolicName=MSG_LOGGING_ENABLED
Language=English
Event logging enabled for LODriver Driver.
.

MessageId=+1
Facility=Driver
Severity=Informational
SymbolicName=MSG_DRIVER_STARTING
Language=English
LODriver Driver has successfully initialized.
.

MessageId=+1
Facility=Driver
Severity=Informational
SymbolicName=MSG_DRIVER_STOPPING
Language=English
LODriver Driver has unloaded.
.

MessageId=+1
Facility=Driver
Severity=Informational
SymbolicName=MSG_ADDING_DEVICE
Language=English
LODriver Device has been added: %1 - Symbolic link name: %2.
.

MessageId=+1
Facility=Driver
Severity=Informational
SymbolicName=MSG_STARTING_DEVICE
Language=English
LODriver Device has been started: %1.
.

MessageId=+1
Facility=Driver
Severity=Informational
SymbolicName=MSG_STOPPING_DEVICE
Language=English
LODriver Device has been stopped: %1.
.

MessageId=+1
Facility=Driver
Severity=Informational
SymbolicName=MSG_REMOVING_DEVICE
Language=English
LODriver Device is being removed: %1.
.

MessageId=+1
Facility=Driver
Severity=Informational
SymbolicName=MSG_OPENING_HANDLE
Language=English
Opening handle to %1.
.

MessageId=+1
Facility=Driver
Severity=Informational
SymbolicName=MSG_CLOSING_HANDLE
Language=English
Closing handle to %1.
.

MessageId=+1
Facility=Driver
Severity=Informational
SymbolicName=MSG_WRITE_REQUEST
Language=English
Write request (Dump Data is number of bytes).
.

MessageId=+1
Facility=Driver
Severity=Informational
SymbolicName=MSG_DEVICE_CONTROL_REQUEST
Language=English
Device Control Request (Dump Data is Buffer Size Info).
.
