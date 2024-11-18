//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_proto.c
**
** DESCRIPTION: Primary DFU protocol library.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "dfu_proto_api.h"
#include "dfu_proto_private.h"
#include "async_timer.h"

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*
** This is the pool of management structures that the
** client pulls from when the "dfuProtoInit()" is
** called.
**
** The number of these is configured in the "dfu_proto_config.h"
** header.
*/
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
static dfuProtocol dfuEnvs[MAX_PROTOCOL_INSTANCES];

/*
** Helper prototypes
**
*/
static void dfuInitEnv(dfuProtocol *dfu);
static bool dfuFreeEnv(dfuProtocol * dfu);
static void dfuExecPeriodicCommands(dfuProtocol *dfu);
static bool dfuDidNAKNoSession(dfuProtocol *dfu, dfuCommandsEnum command);
static void dfuTxMsg(dfuProtocol *dfu, uint8_t *txBuff, uint16_t txBuffLen,  dfuMsgTargetEnum target);

/*
** Internal Message-Handler Prototypes
**
*/
static bool internalMsgHandler_CMD_NEGOTIATE_MTU(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_BEGIN_RCV(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_ABORT_RCV(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_RCV_COMPLETE(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_RCV_DATA(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_REBOOT(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_DEVICE_STATUS(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_KEEP_ALIVE(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_BEGIN_SESSION(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_END_SESSION(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_IMAGE_STATUS(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool defaultCommandHandler(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*
   THIS MAPS THE INTERNAL HANDLERS TO EACH COMMAND. IT ALSO
   PROVIDES THE SIZES OF EACH VARIANT (command, response,
   ack, nak, etc.).  The message dispatcher uses these values
   to confirm the sizes are correct and if they are NOT,
   will invoke the user's error callback (if one was
   registered).
*/
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
static const dfuInternalHandlerDescriptorStruct internalMsgHandlers[MAX_COMMANDS+1] =
{
    {defaultCommandHandler, 				{0,0,0,0,0}				},
    {internalMsgHandler_CMD_NEGOTIATE_MTU, 	{3,3,1,1,0}				},
    {internalMsgHandler_CMD_BEGIN_RCV, 		{8,0,1,1,0}				},
    {internalMsgHandler_CMD_ABORT_RCV, 		{1,0,1,1,0}				},
    {internalMsgHandler_CMD_RCV_COMPLETE, 	{4,0,1,1,0}				},
    {internalMsgHandler_CMD_RCV_DATA, 		{MAX_MSG_LEN,0,1,1,0}	},
    {internalMsgHandler_CMD_REBOOT, 		{3,0,1,1,0}				},
    {internalMsgHandler_CMD_DEVICE_STATUS, 	{1,8,1,1,8}				},
    {internalMsgHandler_CMD_KEEP_ALIVE, 	{0,0,0,0,1}				},
    {internalMsgHandler_CMD_BEGIN_SESSION, 	{1,5,1,1,0}				},
    {internalMsgHandler_CMD_END_SESSION, 	{1,0,1,1,0}				},
    {internalMsgHandler_CMD_IMAGE_STATUS, 	{5,4,1,1,0}				},
    {defaultCommandHandler, 				{0,0,0,0,0}				},
    {defaultCommandHandler, 				{0,0,0,0,0}				},
    {defaultCommandHandler, 				{0,0,0,0,0}				},
    {defaultCommandHandler, 				{0,0,0,0,0}				}
};

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                         PRIVATE SUPPORT FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: dfuDidNAKNoSession
**
** DESCRIPTION: Called by the main receive-message dispatcher, this will
**              examine the current state of the "sessionState" and if
**              active AND the command was registered as only being
**              allowed during a session, this will send a NAK and
**              return a TRUE to indicate it NAK'd the command.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static bool dfuDidNAKNoSession(dfuProtocol *dfu, dfuCommandsEnum command)
{
    bool                        ret = false;

    if (VALID_ADMIN(dfu))
    {
        if (
               (dfu->supportedCommands[command].requiredSessionStates != SESSION_STATE_ANY) &&
               (dfu->supportedCommands[command].requiredSessionStates != dfu->sessionState)
           )
        {
            dfuSendSimpleNAK(dfu);
            ret = true;
        }
    }

    return (ret);
}

/*!
** FUNCTION: dfuInitEnv
**
** DESCRIPTION: Initialize an manager object to defaults.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS: Sets up initial command handlers, MTU size, timeouts,
**           etc.  Also validates the object so that it is
**           considered "in us"
**
*/
static void dfuInitEnv(dfuProtocol *dfu)
{
    if (dfu)
    {
        uint8_t                     index;

        memset(dfu, 0x00, sizeof(dfuProtocol));

        dfu->mtu = DEFAULT_MTU;
        dfu->toggle = HDR_TOGGLE_BIT_MASK; // always starts at "1"
        dfu->sessionTimeoutMS = IDLE_SESSION_TIMEOUT_MS;
        dfu->sessionState = SESSION_STATE_INACTIVE;
        dfu->lastCommand = 0xF;
        dfu->currentDriveState = DDS_IDLE;
        dfu->deviceStatus = 0x00;
        dfu->uptimeTimerRunning = false;

        // Default command-handler installation
        for (index = 0; index < MAX_COMMANDS+1; index++)
        {
            dfuRemoveCommandHandler(dfu, index);
        }

        // Init the periodic comamnd struct
        for (index = 0; index < MAX_PERIODIC_COMMANDS; index++)
        {
            dfu->periodicCommands[index].handler = NULL;
            dfu->periodicCommands[index].sessionStates = SESSION_STATE_ANY;
            dfu->periodicCommands[index].execIntervalMS = 0;
            dfu->periodicCommands[index].timerRunning = false;
            dfu->periodicCommands[index].userPtr = NULL;
            dfu->periodicCommands[index].signature = 0;
        }

        dfu->signature = DFU_ADMIN_SIGNATURE;
    }
}

/*!
** FUNCTION: Find an unused management object in the static list,
**           if any.  If found, initialize to defaults and validate
**           the object.  Return its address
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
static dfuProtocol * dfuAllocEnv(void)
{
    dfuProtocol *           ret = NULL;
    int                     index;

    LOCK_ENVS();
    for (index = 0; index < MAX_PROTOCOL_INSTANCES; index++)
    {
        if (dfuEnvs[index].signature != DFU_ADMIN_SIGNATURE)
        {
            dfuInitEnv(&dfuEnvs[index]);
            ret = &dfuEnvs[index];
            break;
        }
    }
    UNLOCK_ENVS();

    return (ret);
}

/*!
** FUNCTION: dfuFreeEnv
**
** DESCRIPTION: Cleans up the environment passed and "returns" it to
**              availability in the pool.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static bool dfuFreeEnv(dfuProtocol * dfu)
{
    bool                    ret = false;

    if (VALID_ADMIN(dfu))
    {
        dfuInitEnv(dfu);
        dfu->signature = 0x00000000;

        ret = true;
    }

    return (ret);
}


/*!
** FUNCTION: dfuTxMsg
**
** DESCRIPTION: This is called by all internal functions in order to
**              transmit data.  It "wraps" the client's call, so that
**              we're sure to toggle the "toggle" bit of the message
**              and save that state.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static void dfuTxMsg(dfuProtocol *dfu, uint8_t *txBuff, uint16_t txBuffLen,  dfuMsgTargetEnum target)
{
    if ( (VALID_ADMIN(dfu)) && (dfu->txFunct) && (txBuff) && (txBuffLen > 0) )
    {
         dfu->txFunct(dfu, txBuff, txBuffLen, target, dfu->userPtr);
         INVERT_TOGGLE(dfu);
    }
}

/*!
** FUNCTION: dfuExecPeriodicCommands
**
** DESCRIPTION: Runs the installed periodic handlers, if their timer
**              has expored.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static void dfuExecPeriodicCommands(dfuProtocol *dfu)
{
    if (VALID_ADMIN(dfu))
    {
        int                 index;

        for (index = 0; index < MAX_PERIODIC_COMMANDS; index++)
        {
            if (dfu->periodicCommands[index].signature == PERIODIC_COMMAND_SIGNATURE)
            {
                if (
                       (dfu->periodicCommands[index].handler) &&
                       (dfu->periodicCommands[index].execIntervalMS > 0) &&
                       (dfu->periodicCommands[index].timerRunning)
                   )
                {
                    if (TIMER_Finished(&dfu->periodicCommands[index].timer,
                                       dfu->periodicCommands[index].execIntervalMS))
                    {
                        // Run the handler now
                        dfu->periodicCommands[index].handler(dfu,
                                                             NULL,
                                                             0,
                                                             MSG_TYPE_UNSOLICITED,
                                                             dfu->periodicCommands[index].userPtr);

                        // Re-start the timer
                        TIMER_Start(&dfu->periodicCommands[index].timer);
                    }
                }
            }
        }
    }

    return;
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                     DEFAULT INTERNAL COMMAND HANDLERS
/*
    These are what the engine calls when it recieves a valid command.  They
    then decide if the client had registered their own handler anf if so,
    that handler is called.  If no handler has been registered they each
    perform whatever default actions apply.
*/
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

static bool internalMsgHandler_CMD_NEGOTIATE_MTU(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[CMD_NEGOTIATE_MTU].handler != NULL)
        {
            ret = dfu->supportedCommands[CMD_NEGOTIATE_MTU].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[CMD_NEGOTIATE_MTU].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_BEGIN_RCV(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[CMD_BEGIN_RCV].handler != NULL)
        {
            ret = dfu->supportedCommands[CMD_BEGIN_RCV].handler(dfu, msg, msgLen, msgType,  dfu->supportedCommands[CMD_BEGIN_RCV].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_ABORT_RCV(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[CMD_ABORT_RCV].handler != NULL)
        {
            ret = dfu->supportedCommands[CMD_ABORT_RCV].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[CMD_ABORT_RCV].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_RCV_COMPLETE(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[CMD_RCV_COMPLETE].handler != NULL)
        {
            ret = dfu->supportedCommands[CMD_RCV_COMPLETE].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[CMD_RCV_COMPLETE].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_RCV_DATA(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[CMD_RCV_DATA].handler != NULL)
        {
            ret = dfu->supportedCommands[CMD_RCV_DATA].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[CMD_RCV_DATA].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_REBOOT(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[CMD_REBOOT].handler != NULL)
        {
            ret = dfu->supportedCommands[CMD_REBOOT].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[CMD_REBOOT].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_DEVICE_STATUS(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[CMD_DEVICE_STATUS].handler != NULL)
        {
            ret = dfu->supportedCommands[CMD_DEVICE_STATUS].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[CMD_DEVICE_STATUS].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_KEEP_ALIVE(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[CMD_KEEP_ALIVE].handler != NULL)
        {
            ret = dfu->supportedCommands[CMD_KEEP_ALIVE].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[CMD_KEEP_ALIVE].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_BEGIN_SESSION(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[CMD_BEGIN_SESSION].handler != NULL)
        {
            ret = dfu->supportedCommands[CMD_BEGIN_SESSION].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[CMD_BEGIN_SESSION].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_END_SESSION(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[CMD_END_SESSION].handler != NULL)
        {
            ret = dfu->supportedCommands[CMD_END_SESSION].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[CMD_END_SESSION].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_IMAGE_STATUS(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[CMD_IMAGE_STATUS].handler != NULL)
        {
            ret = dfu->supportedCommands[CMD_IMAGE_STATUS].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[CMD_IMAGE_STATUS].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, msg, msgLen, msgType);
        }
    }

    return (ret);
}

/*!
** FUNCTION: defaultCommandHandler
**
** DESCRIPTION: This is installed as the default handler for
**              commands for which the client has not registered
**              their own handler.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static bool defaultCommandHandler(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
	(void)msg;
	(void)msgLen;
	(void)msgType;

#if (NAK_UNSUPPORTED_COMMANDS==1)
    return (dfuSendSimpleNAK(dfu));
#else
    return (true);
#endif
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                             PUBLIC API FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: dfuInit
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
dfuProtocol * dfuInit(dfuRxFunct rxFunct,
                      dfuTxFunct txFunct,
                      dfuErrFunct errFunct,
                      dfuUserPtr userPtr)
{
    dfuProtocol *               ret = NULL;

    if (
           (rxFunct) &&
           (txFunct)
       )
    {
        ret = dfuAllocEnv();
        if (ret)
        {
            ret->rxFunct = rxFunct;
            ret->txFunct = txFunct;
            ret->errFunct = errFunct;
            ret->userPtr = userPtr;

            // Start the "uptime" timer.
            TIMER_Start(&ret->uptimeTimer);
            ret->uptimeTimerRunning = true;
        }
    }

    return (ret);
}

/*!
** FUNCTION: dfuUnInit
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
bool dfuUnInit(dfuProtocol * dfu)
{
    bool                        ret = false;

    if (VALID_ADMIN(dfu))
    {
        ret = dfuFreeEnv(dfu);
    }

    return (ret);
}

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
dfuDriveStateEnum dfuDrive(dfuProtocol *dfu)
{
    dfuDriveStateEnum               ret = DDS_UNKNOWN;

    if (VALID_ADMIN(dfu))
    {
        uint8_t                 *msgPtr = NULL;
        uint16_t                 msgLen = 0;

        // Run any ready periodic commands
        dfuExecPeriodicCommands(dfu);

        // Call to receive a message.
        msgPtr = dfu->rxFunct(dfu, &msgLen, dfu->userPtr);

        /*
        ** If we got a message:
        **
        **   - Is its length less than or equal to our
        **     MTU?
        **
        */
        if ( (msgPtr != NULL) && (msgLen > 0) && (msgLen <= dfu->mtu) )
        {
            uint8_t                 toggle;
            dfuMsgTypeEnum          msgType;
            dfuErrorCodeEnum        errorCode = DFU_ERR_NONE;

            /*
            ** Parse out the:
            **
            **  1. Command
            **  2. Toggle
            **  3. Msg Type
            **
            */
            dfuParseMsgHdr(msgPtr, &dfu->lastCommand, &toggle, &msgType);

            // Command in range?
            if (dfu->lastCommand < MAX_COMMANDS)
            {
                /*
                ** If the command was registered as requiring a
                ** session to be active, and one is NOT, we will
                ** automatically NAK that command.
                **
                */
                if (!dfuDidNAKNoSession(dfu, dfu->lastCommand))
                {
                    // Update the Session timer to avoid expiration
                    TIMER_Start(&dfu->sessionTimer);

                    /*
                    ** Call the INTERNAL handler for the command.  It will in
                    ** turn call the client-registered version of that handler,
                    ** if one was registered.  If not, it will NAK the message.
                    **
                    ** (We pass NULL as the last argument, because the internal handlers will
                    **  pass the userPtr to any registered handlers)
                    */
                    if (msgLen <= dfu->mtu)
                    {
                    	if (msgLen == 0)
                    	{
                    		errorCode = DFU_ERR_MSG_TOO_SHORT;
                    	}
                    	else if (msgLen > internalMsgHandlers[dfu->lastCommand].msgTypeSizes[msgType])
                        {
                            errorCode = DFU_ERR_MSG_TOO_LONG;
                        }
                        else
                        {
                            ret = DDS_ERROR;
                    	    if (internalMsgHandlers[dfu->lastCommand].handler(dfu, msgPtr, msgLen, msgType))
                            {
                                ret = DDS_OK;
                            }
                        }
                    }
                    else
                    {
                        errorCode = DFU_ERR_MSG_EXCEEDS_MTU;
                    }
                }
                else
                {
                    errorCode = DFU_ERR_NO_SESSION;
                }
            }
            else
            {
                errorCode = DFU_ERR_INVALID_COMMAND;
            }

            /*
            ** Should we call the client's error handler?
            **
            */
            if (errorCode != DFU_ERR_NONE)
            {
                if (dfu->errFunct != NULL)
                {
                    dfu->errFunct(dfu, msgPtr, msgLen, errorCode, dfu->userPtr);
                }
            }
        }
    }

    return (ret);
}

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
                              dfuSessionStateEnum requiredSessionStates,
                              dfuUserPtr userPtr)
{
    bool                    ret = false;

    if (VALID_ADMIN(dfu))
    {
        if ( (command > 0) && (command <= MAX_COMMANDS) )
        {
            dfu->supportedCommands[command].command = command;
            dfu->supportedCommands[command].handler = handler;
            dfu->supportedCommands[command].requiredSessionStates = requiredSessionStates;
            dfu->supportedCommands[command].userPtr = userPtr;

            ret = true;
        }
    }

    return (ret);
}

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
bool dfuRemoveCommandHandler(dfuProtocol * dfu, dfuCommandsEnum command)
{
    return (dfuInstallCommandHandler(dfu,
                                     command,
                                     NULL,
                                     true,
                                     NULL));
}

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
                               dfuSessionStateEnum sessionStates,
                               dfuUserPtr userPtr)
{
    bool                    ret = false;

    if (VALID_ADMIN(dfu))
    {
        int         index;

        for (index = 0; index < MAX_PERIODIC_COMMANDS; index++)
        {
            if (dfu->periodicCommands[index].signature != PERIODIC_COMMAND_SIGNATURE)
            {
                if ( (handler != NULL) && (execIntervalMS > 0) )
                {
                    dfu->periodicCommands[index].handler = handler;
                    dfu->periodicCommands[index].sessionStates = sessionStates;
                    dfu->periodicCommands[index].execIntervalMS = execIntervalMS;
                    dfu->periodicCommands[index].userPtr = userPtr;
                    dfu->periodicCommands[index].signature = PERIODIC_COMMAND_SIGNATURE;

                    TIMER_Start(&dfu->periodicCommands[index].timer);
                    dfu->periodicCommands[index].timerRunning = true;

                    ret = true;
                    break;
                }
            }
        }
    }

    return (ret);
}

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
uint8_t * dfuBuildMsgHdr(dfuProtocol *dfu, uint8_t *msg, dfuCommandsEnum command, dfuMsgTypeEnum msgType)
{
    uint8_t             *ret = NULL;

    if  ( (VALID_ADMIN(dfu)) && (msg) )
    {
        // Add COMMAND
        *msg = (command << 4);
        *msg &= HDR_COMMAND_BIT_MASK;

        // Add current TOGGLE bit
        *msg |= dfu->toggle;

        // Add MSG TYPE
        *msg |= (HDR_MSG_TYPE_BIT_MASK & msgType);

        ret = msg;
    }

    return (ret);
}

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
bool dfuParseMsgHdr(uint8_t *msg, dfuCommandsEnum *cmd, uint8_t *toggle, dfuMsgTypeEnum *msgType)
{
    bool                    ret = false;

    if ( (msg) && (cmd) && (toggle) && (msgType) )
    {
    	*cmd = *msg;
    	*cmd >>= 4;
    	*cmd &= 0x0F;
        *toggle = (bool)(*msg & HDR_TOGGLE_BIT_MASK);
        *msgType = (*msg & HDR_MSG_TYPE_BIT_MASK);
    }

    return (ret);
}

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
bool dfuSendMsg(dfuProtocol *dfu, uint8_t *txBuff, uint16_t txBuffLen, dfuMsgTargetEnum target)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (txBuff) && (txBuffLen > 0) )
    {
        dfuTxMsg(dfu, txBuff, txBuffLen, target);
        ret = true;
    }

    return (ret);
}

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
bool dfuSendSimpleACK(dfuProtocol *dfu)
{
    bool                ret = true;

    if (VALID_ADMIN(dfu))
    {
        // We're ACKing the most recent command
        dfuBuildMsgHdr(dfu, dfu->internalMsgBuf, dfu->lastCommand, MSG_TYPE_ACK);

        // Send the NAK back the caller
        dfuTxMsg(dfu, dfu->internalMsgBuf, 1, DFU_TARGET_SENDER);
    }

    return (ret);
}

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
bool dfuSendSimpleNAK(dfuProtocol *dfu)
{
    bool                ret = true;

    if (VALID_ADMIN(dfu))
    {
        // We're NAKing the most recent command
        dfuBuildMsgHdr(dfu, dfu->internalMsgBuf, dfu->lastCommand, MSG_TYPE_NAK);

        // Send the NAK back the caller
        dfuTxMsg(dfu, dfu->internalMsgBuf, 1, DFU_TARGET_SENDER);
    }

    return (ret);
}

/*!
** FUNCTION: dfuSetSessionState
**
** DESCRIPTION: Something external has to set whether a session is
**              ACTIVE or INACTIVE.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool dfuSetSessionState(dfuProtocol *dfu, dfuSessionStateEnum sessionState)
{
    bool                    ret = false;

    if (
           (VALID_ADMIN(dfu)) &&
           ( (sessionState == SESSION_STATE_INACTIVE) || (sessionState == SESSION_STATE_ACTIVE) )
       )
    {
        dfu->sessionState = sessionState;
        ret = true;
    }

    return (ret);
}

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
bool dfuIsSessionActive(dfuProtocol * dfu)
{
    bool                    ret = false;

    if (VALID_ADMIN(dfu))
    {
        ret = (bool)(dfu->sessionState == SESSION_STATE_ACTIVE);
    }

    return (ret);
}

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
bool dfuSetDeviceStatusBits(dfuProtocol *dfu, uint8_t deviceStatusBits)
{
    bool                    ret = false;

    if (VALID_ADMIN(dfu))
    {
        dfu->deviceStatus |= deviceStatusBits;
        ret = true;
    }

    return (ret);
}

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
bool dfuClearDeviceStatusBits(dfuProtocol *dfu, uint8_t deviceStatusBits)
{
    bool                    ret = false;

    if (VALID_ADMIN(dfu))
    {
        dfu->deviceStatus &= ~deviceStatusBits;
        ret = true;
    }

    return (ret);
}

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
uint8_t dfuGetDeviceStatusBits(dfuProtocol *dfu)
{
    uint8_t                     ret = 0x00;

    if (VALID_ADMIN(dfu))
    {
        ret = dfu->deviceStatus;
    }

    return (ret);
}

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
bool dfuSetMTU(dfuProtocol *dfu, uint16_t mtu)
{
    bool                ret = false;

    if (VALID_ADMIN(dfu))
    {
        if ( (mtu > 0) && (mtu <= MAX_MSG_LEN) )
        {
            dfu->mtu = mtu;
            ret = true;
        }
    }

    return (ret);
}

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
uint16_t dfuGetMTU(dfuProtocol *dfu)
{
    uint16_t            ret = MAX_MSG_LEN;

    if (VALID_ADMIN(dfu))
    {
        ret = dfu->mtu;
    }

    return (ret);
}

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
uint16_t dfuGetUptimeMins(dfuProtocol *dfu)
{
    uint16_t                    ret = 0;

    if (VALID_ADMIN(dfu))
    {
        if (dfu->uptimeTimerRunning)
        {
            ASYNC_TIMER_STRUCT      timer;
            uint32_t                deltaMS;

            // Get delta between "now" and when we started
            TIMER_Start(&timer);
            deltaMS = TIMER_GetElapsedMillisecs(&dfu->uptimeTimer, &timer);
            if ( (deltaMS > 0) && (deltaMS >= ONE_MINUTE_MILLSECONDS) )
            {
                ret = (uint16_t)(deltaMS / ONE_MINUTE_MILLSECONDS);
            }
        }
    }

    return (ret);
}
