//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_proto_api.h
**
** DESCRIPTION: 
**
** REVISION HISTORY: 
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
** This include file configures the library for your
** particular needs.
**
*/
#include "dfu_proto_config.h"

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*
                 IMAGE INDEX RANGES
*/
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
#define IMAGE_INDEX_APP_LOW             (1)
#define IMAGE_INDEX_APP_HIGH            (96)
#define IMAGE_INDEX_RESERVED_LOW        (97)
#define IMAGE_INDEX_RESERVED_HIGH       (127)

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//
//              IMPORTANT POINTER TYPES
//
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/*
** A pointer to one of these is returned from the initialization of the library.
** All DFU library calls then pass that as the first parameter.
**
*/
typedef struct dfuProtocol dfuProtocol;

/*
** User pointer type. Wrapper over a void *
**
*/
typedef void *               dfuUserPtr;

/*
** BIT-MASKS
**
*/
#define DEVICE_STATUS_BIT_MASK             (0xFE) // If we use the last bit, this must be changed
#define DFU_DEV_STATUS_BIT_MASK_AP         (1<<7)
#define DFU_DEV_STATUS_BIT_MASK_SV         (1<<6)
#define DFU_DEV_STATUS_BIT_MASK_IC         (1<<5)
#define DFU_DEV_STATUS_BIT_MASK_IH         (1<<4)
#define DFU_DEV_STATUS_BIT_MASK_IM         (1<<3)
#define DFU_DEV_STATUS_BIT_MASK_SK         (1<<2)
#define DFU_DEV_STATUS_BIT_MASK_EK         (1<<1)
#define DFU_DEV_STATUS_BIT_MASK_UNUSED     (1<<0)

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//
//                PUBLIC ENUMERATIONS
//
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/*
** VALID COMMANDS
**
*/
typedef enum
{
    CMD_NEGOTIATE_MTU = 0x01,
    CMD_BEGIN_RCV = 0x02,
    CMD_ABORT_XFER = 0x03,
    CMD_RCV_COMPLETE = 0x04,
    CMD_RCV_DATA = 0x05,
    CMD_REBOOT = 0x06,
    CMD_DEVICE_STATUS = 0x07,
    CMD_KEEP_ALIVE = 0x08,
    CMD_BEGIN_SESSION = 0x09,
    CMD_END_SESSION = 0x0A,
    CMD_IMAGE_STATUS = 0x0B,
    CMD_BEGIN_SEND = 0x0C,
    CMD_SEND_DATA = 0x0D,
    CMD_INSTALL_IMAGE = 0x0E,

    CMD_LAST_COMMAND = 0x0F
}dfuCommandsEnum;

/*
** MESSAGE TYPES
**
*/
#define MAX_MSG_TYPES            (5)
typedef enum
{
    MSG_TYPE_COMMAND = 0x00,
    MSG_TYPE_RESPONSE = 0x01,
    MSG_TYPE_ACK = 0x02,
    MSG_TYPE_NAK = 0x03,
    MSG_TYPE_UNSOLICITED = 0x04
}dfuMsgTypeEnum;

/*
** RETURNS JUST THE MESSAGE TYPE
**
*/
#define MSG_TYPE(msg)  (*msg & 0x07)

typedef enum
{
    DDS_IDLE,
    DDS_ERROR,
    DDS_OK,
    DDS_SESSION_ACTIVE,
    DDS_SESSION_ENDED,
    DDS_SESSION_TIMEOUT,

    DDS_UNKNOWN = 0xFF
}dfuDriveStateEnum;

/*
** ERROR ENUMERATION
**
*/
typedef enum
{
    DFU_ERR_NONE = 0,
    DFU_ERR_INVALID_MSG_TYPE,
    DFU_ERR_INVALID_COMMAND,
    DFU_ERR_MSG_TOO_SHORT,
    DFU_ERR_MSG_TOO_LONG,
    DFU_ERR_MSG_EXCEEDS_MTU,
    DFU_ERR_NO_SESSION,
    DFU_ERR_SESSION_TIMED_OUT
}dfuErrorCodeEnum;

/*
** When sending a message, should it be to some 
** specific target, or ANY (broadcast)
**
*/
typedef enum
{
    DFU_TARGET_SENDER,
    DFU_TARGET_ANY
}dfuMsgTargetEnum;

/*
** DEVICE TYPES
**
**  THESE NEED TO BE UNIVERSAL! ALL SYSTEMS MUST USE
**  THE SAME NUMERIC VALUES !!!!!!!!
**
*/
typedef enum
{
    DFU_DEV_TYPE_ATP,
    DFU_DEV_TYPE_VCU,
    DFU_DEV_TYPE_TCM,
    DFU_DEV_TYPE_UWB,
    DFU_DEV_TYPE_LVPDU,
    DFU_DEV_TYPE_LATERAL,
    DFU_DEV_TYPE_SWITCH,
    DFU_DEV_TYPE_LOGGER
}dfuDeviceTypeEnum;

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//
//  THESE FUNCTION POINTER TYPES PROVIDE THE 
//  LIBRARY WITH RX, TX AND ERROR HANDLERS. THESE
//  ARE REGISTERED WITH THE LIBRARY WHEN IT IS 
//  INITIALIZED.  THIS ALLOWS THE LIBRARY TO BE
//  PLATFORM INDEPENDENT
//
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/*
** Library calls out to one of these to receive a message.
**
**    The function that is called is responsible for the buffer that holds the message
**    and must return a pointer to it if there is a message ready.  The function
**    also has to set the "rxBuffLen" value to let the library know how much data
**    is available.
**
** PARAMETERS:
**
**  - dfu: The instance object for the library
**  - rxBufLen: A pointer to a uint16 that will hold the length of the data if data were received.
**  - userPtr: The "txUserPtr" that the caller provided when it registered this handler.
**
*/
typedef uint8_t *(* dfuRxFunct)(dfuProtocol * dfu, uint16_t * rxBuffLen, dfuUserPtr userPtr);

/*
** Library calls out to one of these to transmit a message
**
**   The function that is called needs a pointer to a buffer that holds the
**   message that will be sent.  It also needs to provide the length of the
**   message and any optional "user" pointer that was provided when the handler
**   was originally registered.
**
**   The function receives the desired MSG TYPE so that it can determine 
**   whether to send the response as a
**
*/
typedef bool (* dfuTxFunct)(dfuProtocol * dfu, uint8_t *txBuff, uint16_t txBuffLen, dfuMsgTargetEnum target, dfuUserPtr userPtr);

/*
** Library calls out to one of these if it detects an error of some kind.
**
**   The function that is called is provided with the address of the message 
**   that has the error, along with its length.  Also provided is an error
**   code indicating what the library thought was wrong with the message.
**
*/
typedef void (* dfuErrFunct)(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuErrorCodeEnum error, dfuUserPtr userPtr);


// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//
//  USER MUST IMPLEMENT EVERY COMMAND HANDLER USING 
//  FUNCTIONS WITH THIS SIGNATURE
//
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/*
** Generic msg-handler function type that a client registers for each of the commands
** it supports.
**
** PARAMETERS:
**
**   - dfu: The instance object for the library
**   - *msg: A byte pointer to the message.
**   - The length of the message
**   - The TYPE of message (COMMAND, RESPONSE, NAK, ACK, UNSOLICITED)
**   - The "user" pointer that may have been saved when the handler was registered.
**
*/
typedef bool (* dfuCommandHandler)(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr);

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//          DFU PROTOCOL STATE STRUCTURES
/*
    Depending on what state the protocol is in, one
    of these state structures will be active.  
    The current state of a given instance of the 
    protocol engine will contain the union of all
    of the following, with one being acive at a 
    time.
*/
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

typedef struct
{
    uint32_t                signature;
}dfuIdleStateStruct;


typedef struct
{
    uint32_t                signature;
}dfuSessionStartingStateStruct;

typedef struct
{
    uint32_t                signature;
}dfuSessionStartedStateStruct;


typedef struct 
{
    uint32_t                signature;
}dfuReceivingImageStateStruct;


typedef union 
{
    dfuIdleStateStruct                  idleState;
    dfuSessionStartingStateStruct       sessionStartingState;
    dfuSessionStartedStateStruct        sessionStartedState;
    dfuReceivingImageStateStruct        receivingImageState;
}dfuProtocolStatesUnion;


// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                              HELPFUL MACROS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*
** Is the COMMAND value valid?
**
*/
#define VALID_CMD_ID(cmd)      (bool) ((cmd > 0) && (cmd <= CMD_LAST_COMMAND) )

/*
** Return the COMMAND value from the message header
**
*/
#define CMD_FROM_MSG(msg)       (((uint8_t)msg[0] & HDR_COMMAND_BIT_MASK) >> 4)

#if defined(__cplusplus)
extern "C" {
#endif

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                            PUBLIC API PROTOTYPES
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: dfuCreate
**
** DESCRIPTION: This MUST be called in order to start up the protocol engine.
**              The return is an opaque pointer to the ADMIN/state structure
**              for the instance.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
dfuProtocol * dfuCreate(dfuRxFunct rxFunct, 
                        dfuTxFunct txFunct,
                        dfuErrFunct errFunct, 
                        dfuUserPtr userPtr);

/*!
** FUNCTION: dfuDestroy
**
** DESCRIPTION: Clean up the library.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuDestroy(dfuProtocol * dfu);                      

/*!
** FUNCTION: dfuDrive
**
** DESCRIPTION: Main "pump" method for the protocol.  Handles
**              tx, rx, timeouts, etc.  Calls out to the 
**              message handlers, Tx/Rx handlers, error handlers
**              etc.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
dfuDriveStateEnum dfuDrive(dfuProtocol *dfu);

/*!
** FUNCTION: dfuInstallCommandHandler
**
** DESCRIPTION: For any given command, this allows the caller
**              to install a handler that will be called when
**              that command has been received.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuInstallCommandHandler(dfuProtocol * dfu, 
                              dfuCommandsEnum command,
                              dfuCommandHandler handler,
                              dfuUserPtr userPtr);

/*!
** FUNCTION: dfuRemoveCommandHandler
**
** DESCRIPTION: Allows a caller to remove handing for a given command.
**              The handler for that command will be replaced with the
**              default handler.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuRemoveCommandHandler(dfuProtocol * dfu, dfuCommandsEnum command);


/*!
** FUNCTION: dfuInstallPeriodicHandler
**
** DESCRIPTION: Allows a caller to install a command handler
**              that will be executed at a rate specified.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuInstallPeriodicHandler(dfuProtocol *dfu, 
                               dfuCommandHandler handler, 
                               uint32_t execIntervalMS, 
                               dfuUserPtr userPtr);


/*!
** FUNCTION: dfuBuildMsgHdr
**
** DESCRIPTION: Given a pointer to the top of a message, the command and the 
**              message type, this will construct  the header.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint8_t * dfuBuildMsgHdr(dfuProtocol *dfu, uint8_t *msg, dfuCommandsEnum command, dfuMsgTypeEnum msgType);

/*!
** FUNCTION: dfuParseMsgHdr
**
** DESCRIPTION: Passed a header pointer, this returns the value
**              of the:
**
**              - COMMAND
**              - TOGGLE
**              - MSG TYPE
** PARAMETERS: 
**
** RETURNS: True if the header pointer was not NULL
**
** COMMENTS: 
**
*/
bool dfuParseMsgHdr(uint8_t *msg, dfuCommandsEnum *cmd, uint8_t *toggle, dfuMsgTypeEnum *msgType);

/*!
** FUNCTION: dfuSendMsg
**
** DESCRIPTION: Externally-visible message transmit function.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuSendMsg(dfuProtocol *dfu, uint8_t *txBuff, uint16_t txBuffLen, dfuMsgTargetEnum target);

/*!
** FUNCTION: dfuSendSimpleACK 
**
** DESCRIPTION: Build and send an ACK
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuSendSimpleACK(dfuProtocol *dfu);

/*!
** FUNCTION: dfuSendSimpleNAK 
**
** DESCRIPTION: Build and send a NAK
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuSendSimpleNAK(dfuProtocol *dfu);

/*!
** FUNCTION: dfuIsSessionActive
**
** DESCRIPTION: Returns whether or not a session is active.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuIsSessionActive(dfuProtocol * dfu);

/*!
** FUNCTION: dfuSetSessionActive
**
** DESCRIPTION: 
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuSetSessionActive(dfuProtocol * dfu);


/*!
** FUNCTION: dfuSetSessionStarting
**
** DESCRIPTION: 
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuSetSessionStarting(dfuProtocol * dfu);

/*!
** FUNCTION: dfuSetSessionInActive
**
** DESCRIPTION: 
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuSetSessionInActive(dfuProtocol * dfu);

/*!
** FUNCTION: dfuIsSessionStarting
**
** DESCRIPTION: 
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuIsSessionStarting(dfuProtocol * dfu);

/*!
** FUNCTION: dfuSetDeviceStatusBits
**
** DESCRIPTION: Lets the caller SET one or more device status bits.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuSetDeviceStatusBits(dfuProtocol *dfu, uint8_t deviceStatusBits);

/*!
** FUNCTION: dfuClearDeviceStatusBits
**
** DESCRIPTION: Lets the caller CLEAR one or more device status bits.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuClearDeviceStatusBits(dfuProtocol *dfu, uint8_t deviceStatusBits);

/*!
** FUNCTION: dfuGetDeviceStatusBits
**
** DESCRIPTION: Returns a byte that holds the current device status bits.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint8_t dfuGetDeviceStatusBits(dfuProtocol *dfu);

/*!
** FUNCTION: dfuSetMTU 
**
** DESCRIPTION: Sets the value of the MTU.  Will NOT set it
**              if the value is > MAX_MSG_LEN or = 0
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuSetMTU(dfuProtocol *dfu, uint16_t mtu);

/*!
** FUNCTION: dfuGetMTU
**
** DESCRIPTION: Return the currently-set MTU value.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint16_t dfuGetMTU(dfuProtocol *dfu);

/*!
** FUNCTION: dfuGetUptimeMins
**
** DESCRIPTION: Returns the # of minutes the DFU mode has been running.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint16_t dfuGetUptimeMins(dfuProtocol *dfu);

/*!
** FUNCTION: dfuSetSessionState
**
** DESCRIPTION: This will OR the value of the "sessionStates"
**              bit-map in with the current value of the 
**              protocol session state.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuSetSessionState(dfuProtocol *dfu, uint8_t sessionStates);

/*!
** FUNCTION: dfuClearSessionState
**
** DESCRIPTION: OR's the complement of the "sessionState" bitmask
**              with the current value of the protocol session state.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuClearSessionState(dfuProtocol *dfu, uint8_t sessionStates);

#if defined(__cplusplus)
}
#endif
