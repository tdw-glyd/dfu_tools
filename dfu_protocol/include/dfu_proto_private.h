//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_proto_private.h
**
** DESCRIPTION: Private definitions, types, etc. for the DFU mode protocol
**              library.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once

#include "dfu_proto_api.h"
#include "async_timer.h"

// Add your types, definitions, macros, etc. here

#define LOCK_ENVS()
#define UNLOCK_ENVS()

/*
** Some time values expressed in milliseconds
**
*/
#define MILLISECONDS_PER_SECOND             (1000)
#define ONE_MINUTE_MILLSECONDS              (MILLISECONDS_PER_SECOND * 60)

/*
** Compute the session timeout milliseconds from the configured
** number of minutes.
**
*/
#define IDLE_SESSION_TIMEOUT_MS             (IDLE_SESSION_TIMEOUT_MINS * 60000)

// Set the maximum number of commands (and thus command handlers)
#define MAX_COMMANDS                        (15)

// What we toggle on each transmission
#define HDR_TOGGLE_BIT_MASK                 (0x08)

// Other HDR bit-masks
#define HDR_COMMAND_BIT_MASK                (0xF0)
#define HDR_MSG_TYPE_BIT_MASK               (0x07)

/*
** Describes the handler for each supported command.
**
*/
typedef struct
{
    /*
    ** If TRUE, the command will be NAK'd if
    ** no session has been established.
    **
    */
    dfuSessionStateEnum         requiredSessionStates;

    // Command enumeration value
    dfuCommandsEnum             command;

    // Called when this command received.
    dfuCommandHandler           handler;

    // User-provided generic pointer
    void *                      userPtr;
}dfuSupportedCommands;

/*
** Internal COMMAND handler function type
**
*/
typedef bool (* dfuInternalCommandHandler)(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);



/*
** This associates each internal command handler with the allowed message
** size for each variant of that command (response, NAK, ACK, etc...)
**
** The "msgTypeSizes" field is indexed by the msg type...
**
*/
typedef struct
{
    dfuInternalCommandHandler                   handler;
    size_t                     					msgTypeSizes[MAX_MSG_TYPES];
}dfuInternalHandlerDescriptorStruct;


/*
** PERIODIC COMMAND HANDLER MANAGEMENT STRUCT
**
** Structure type for commands that may be executed periodically by
** the engine.
**
*/
#define PERIODIC_COMMAND_SIGNATURE      (0xF103AB47)
typedef struct
{
    dfuCommandHandler           handler;
    void                       *userPtr;
    dfuSessionStateEnum         sessionStates;
    uint32_t                    execIntervalMS;
    ASYNC_TIMER_STRUCT          timer;
    bool                        timerRunning;
    uint32_t                    signature;
}dfuPeriodicCommandStruct;


/*
** The management structure, one for each instance.
**
*/
#define DFU_ADMIN_SIGNATURE         (0x481A4CBF)
struct dfuProtocol
{
    uint32_t                    signature;

    // Current state of the instance
    dfuDriveStateEnum           currentDriveState;

    // Holds the most recently-received command
    dfuCommandsEnum             lastCommand;

    /*
    ** Holds our callbacks
    **
    */
    dfuRxFunct                  rxFunct;
    dfuTxFunct                  txFunct;
    dfuErrFunct                 errFunct;
    void *                      userPtr;

    // Device status bit-map
    uint8_t                     deviceStatus;

    /*
    ** Payload-related fields
    **
    */
    uint16_t                    mtu;
    uint8_t                     toggle;

    /*
    ** Uptime timer
    **
    */
    ASYNC_TIMER_STRUCT          uptimeTimer;
    bool                        uptimeTimerRunning;

    /*
    ** Session fields
    **
    */
    ASYNC_TIMER_STRUCT          sessionTimer;
    uint32_t                    sessionTimeoutMS;
    dfuSessionStateEnum         sessionState;

    /*
    ** List of client-registered command handlers.
    ** If there is one registerd for a given command,
    ** the internal handler for that command will call
    ** the user's handler from this list.
    **
    */
    dfuSupportedCommands        supportedCommands[MAX_COMMANDS];

    /*
    ** Contains a list of commands handlers that will be executed at the
    ** rate given in each.
    **
    */
    dfuPeriodicCommandStruct    periodicCommands[MAX_PERIODIC_COMMANDS];

    /*
    ** We use this to send messages that have been internally
    ** generated.
    **
    */
    uint8_t                     internalMsgBuf[MAX_MSG_LEN];
};


/*
** Compute the session timout milliseconds from the configured
** number of minutes.
**
*/
#define IDLE_SESSION_TIMEOUT_MS             (IDLE_SESSION_TIMEOUT_MINS * 60000)

/*
** Macro to verify ADMIN struct that all the API calls use.
**
*/
#define VALID_ADMIN(x)   ( (x != NULL) && (x->signature == DFU_ADMIN_SIGNATURE) )

/*
** Macro to verify the MSG TYPE
**
*/
#define VALID_MSG_TYPE(x) (x <= MSG_TYPE_UNSOLICITED)

// Inverts the state of the "toggle" bit in the ADMIN
#define INVERT_TOGGLE(dfu)  (dfu->toggle = dfu->toggle ^ HDR_TOGGLE_BIT_MASK)




#if defined(__cplusplus)
extern "C" {
#endif




#if defined(__cplusplus)
}
#endif
