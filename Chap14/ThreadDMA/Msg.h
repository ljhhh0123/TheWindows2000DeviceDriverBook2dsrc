
//File: Msg.mc
//

//
//  Values are 32 bit values layed out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//
#define FACILITY_RPC_STUBS               0x3
#define FACILITY_RPC_RUNTIME             0x2
#define FACILITY_IO_ERROR_CODE           0x4
#define FACILITY_DRIVER_ERROR_CODE       0x7


//
// Define the severity codes
//
#define STATUS_SEVERITY_WARNING          0x2
#define STATUS_SEVERITY_SUCCESS          0x0
#define STATUS_SEVERITY_INFORMATIONAL    0x1
#define STATUS_SEVERITY_ERROR            0x3


//
// MessageId: MSG_LOGGING_ENABLED
//
// MessageText:
//
//  Event logging enabled for Driver.
//
#define MSG_LOGGING_ENABLED              ((NTSTATUS)0x60070001L)

//
// MessageId: MSG_DRIVER_STARTING
//
// MessageText:
//
//  Driver has successfully initialized.
//
#define MSG_DRIVER_STARTING              ((NTSTATUS)0x60070002L)

//
// MessageId: MSG_DRIVER_STOPPING
//
// MessageText:
//
//  Driver has unloaded.
//
#define MSG_DRIVER_STOPPING              ((NTSTATUS)0x60070003L)

//
// MessageId: MSG_ADDING_DEVICE
//
// MessageText:
//
//  Device has been added: %1 - Symbolic link name: %2.
//
#define MSG_ADDING_DEVICE                ((NTSTATUS)0x60070004L)

//
// MessageId: MSG_STARTING_DEVICE
//
// MessageText:
//
//  Device has been started: %1.
//
#define MSG_STARTING_DEVICE              ((NTSTATUS)0x60070005L)

//
// MessageId: MSG_STOPPING_DEVICE
//
// MessageText:
//
//  Device has been stopped: %1.
//
#define MSG_STOPPING_DEVICE              ((NTSTATUS)0x60070006L)

//
// MessageId: MSG_REMOVING_DEVICE
//
// MessageText:
//
//  Device is being removed: %1.
//
#define MSG_REMOVING_DEVICE              ((NTSTATUS)0x60070007L)

//
// MessageId: MSG_OPENING_HANDLE
//
// MessageText:
//
//  Opening handle to %1.
//
#define MSG_OPENING_HANDLE               ((NTSTATUS)0x60070008L)

//
// MessageId: MSG_CLOSING_HANDLE
//
// MessageText:
//
//  Closing handle to %1.
//
#define MSG_CLOSING_HANDLE               ((NTSTATUS)0x60070009L)

