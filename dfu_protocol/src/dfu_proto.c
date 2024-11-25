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
#include "dfu_messages.h"

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
static bool _dfuDidSessionTimeout(dfuProtocol * dfu);
static void dfuInitEnv(dfuProtocol *dfu);
static dfuProtocol * dfuReserveEnv(void);
static bool dfuReleaseEnv(dfuProtocol * dfu);
static void dfuExecPeriodicCommands(dfuProtocol *dfu);
static bool dfuShouldAllowCmdForSessionState(dfuProtocol *dfu, dfuCommandsEnum command);
static void dfuTxMsg(dfuProtocol *dfu, uint8_t *txBuff, uint16_t txBuffLen,  dfuMsgTargetEnum target);

/*
** Internal Message-Handler Prototypes
**
*/
static bool internalMsgHandler_CMD_NEGOTIATE_MTU(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_BEGIN_RCV(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_ABORT_XFER(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_RCV_COMPLETE(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_RCV_DATA(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_REBOOT(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_DEVICE_STATUS(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_KEEP_ALIVE(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_BEGIN_SESSION(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_END_SESSION(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_IMAGE_STATUS(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_BEGIN_SEND(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_SEND_DATA(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool internalMsgHandler_CMD_INSTALL_IMAGE(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);
static bool defaultCommandHandler(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType);

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*
                 THIS MAPS THE INTERNAL HANDLERS TO EACH COMMAND.

   It also provides the sizes of each varient (command, response, ack, nak, etc.).
   The message dispatcher uses these values to confirm the sizes are correct and if
   they are NOT, will invoke the user's error callback (if one was registered).

   This also sets the Session State(s) that are required for the command to be
   dispatched.  If the current protocol state doesn't match what a given command's
   bitmap of allowed Session States, no response will be sent.

   NOTE: Command value of ZERO is illegal and should NEVER be set! This means there are
         a maximum of 15 total commands available.
*/
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
static const dfuInternalHandlerDescriptorStruct internalMsgHandlers[MAX_COMMANDS+1] =
{
    {defaultCommandHandler, 				{0,0,0,0,0},			SESSION_STATE_ANY                                                     },
    {internalMsgHandler_CMD_NEGOTIATE_MTU, 	{3,3,1,1,0},			SESSION_STATE_STARTING | SESSION_STATE_ACTIVE                         },
    {internalMsgHandler_CMD_BEGIN_RCV, 		{8,0,1,1,0},			SESSION_STATE_ACTIVE                                                  },
    {internalMsgHandler_CMD_ABORT_XFER,     {1,0,1,1,0},			SESSION_STATE_ACTIVE                                                  },
    {internalMsgHandler_CMD_RCV_COMPLETE, 	{4,0,1,1,0},			SESSION_STATE_ACTIVE                                                  },
    {internalMsgHandler_CMD_RCV_DATA, 		{MAX_MSG_LEN,0,1,1,0},	SESSION_STATE_ACTIVE                                                  },
    {internalMsgHandler_CMD_REBOOT, 		{3,0,1,1,0},			SESSION_STATE_ACTIVE                                                  },
    {internalMsgHandler_CMD_DEVICE_STATUS, 	{1,8,1,1,8},			SESSION_STATE_INACTIVE | SESSION_STATE_ACTIVE                         },
    {internalMsgHandler_CMD_KEEP_ALIVE, 	{0,0,0,0,1},			SESSION_STATE_ACTIVE                                                  },
    {internalMsgHandler_CMD_BEGIN_SESSION, 	{1,5,1,1,0}, 			SESSION_STATE_INACTIVE | SESSION_STATE_ACTIVE | SESSION_STATE_STARTING},
    {internalMsgHandler_CMD_END_SESSION, 	{1,0,1,1,0},			SESSION_STATE_ACTIVE                                                  },
    {internalMsgHandler_CMD_IMAGE_STATUS, 	{5,4,1,1,0},			SESSION_STATE_ACTIVE                                                  },
    {internalMsgHandler_CMD_BEGIN_SEND, 	{2,8,1,1,0},			SESSION_STATE_ACTIVE                                                  },
    {internalMsgHandler_CMD_SEND_DATA, 	    {2,MAX_MSG_LEN,1,1,0},	SESSION_STATE_ACTIVE                                                  },
    {internalMsgHandler_CMD_INSTALL_IMAGE,	{1,1,1,1,0},			SESSION_STATE_ACTIVE                                                  },
    {defaultCommandHandler, 				{0,0,0,0,0},			SESSION_STATE_ACTIVE                                                  }
};

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                         PRIVATE SUPPORT FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: _dfuDidSessionTimeout
**
** DESCRIPTION: Checks the timer for either a SESSION_ACTIVE state
**              or SESSION_STARTING state.  The "SESSION_STARTING"
**              state timeout is shorter, since it should complete
**              much more quickly than what an "idle" session should.
**              
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
static bool _dfuDidSessionTimeout(dfuProtocol * dfu)
{
    bool                    ret = false;

    if (dfu->sessionState == SESSION_STATE_ACTIVE)
    {
        if (TIMER_Finished(&dfu->sessionTimer, (60000 * IDLE_SESSION_TIMEOUT_MINS)))
        {
            dfu->sessionState = SESSION_STATE_INACTIVE;
            ret = true;
        }
    }
    else
    if (dfu->sessionState == SESSION_STATE_STARTING)
    {
        if (TIMER_Finished(&dfu->sessionTimer, (600000 * SESSION_STARTING_TIMEOUT_MINS)))
        {
            dfu->sessionState = SESSION_STATE_INACTIVE;
            ret = true;
        }
    }

    return (ret);
}

/*!
** FUNCTION: dfuShouldAllowCmdForSessionState
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
static bool dfuShouldAllowCmdForSessionState(dfuProtocol *dfu, dfuCommandsEnum command)
{
    bool                        ret = false;

    if (VALID_ADMIN(dfu))
    {
        if (VALID_CMD_ID(command))
        {
        	if (
        			(internalMsgHandlers[command].requiredSessionStates & SESSION_STATE_ANY) ||
					(dfu->sessionState & internalMsgHandlers[command].requiredSessionStates)
			   )
        	{
        		ret = true;
        	}
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
            dfu->periodicCommands[index].execIntervalMS = 0;
            dfu->periodicCommands[index].timerRunning = false;
            dfu->periodicCommands[index].userPtr = NULL;
            dfu->periodicCommands[index].signature = 0;
        }

        dfu->signature = DFU_ADMIN_SIGNATURE;
    }
}

/*!
** FUNCTION: dfuReserveEnv
**
** Find an unused management object in the static list,
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
static dfuProtocol * dfuReserveEnv(void)
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
** FUNCTION: dfuReleaseEnv
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
static bool dfuReleaseEnv(dfuProtocol * dfu)
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
//
//                     DEFAULT INTERNAL COMMAND HANDLERS
/*
    These are what the engine calls when it receives a valid command.  They
    then decide if the client had registered their own handler and if so,
    that handler is called.  If no handler has been registered they each
    perform whatever default actions apply.  Most will simply do nothing.
*/
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

static bool internalMsgHandler_CMD_NEGOTIATE_MTU(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        /*
        ** This message will only be allowed when a session is "sta5rting" or ACTIVE
        **
        */
        if ( (dfu->sessionState & SESSION_STATE_STARTING) || (dfu->sessionState & SESSION_STATE_ACTIVE) )
        {
            /*
            ** Call the user's installed handler.
            **
            */
            if (dfu->supportedCommands[command].handler != NULL)
            {
                ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[command].userPtr);
            }
            else
            {
                uint8_t                 msg[128];

                /*
                ** If the user doesn't provide a handler, we send a default response
                **
                */
                if (dfuBuildMsg_CMD_NEGOTIATE_MTU(dfu,
                                                  msg,
                                                  DEFAULT_MTU,
                                                  MSG_TYPE_RESPONSE))
                {
                    // Send the response now
                    ret = dfuSendMsg(dfu, msg, 4, DFU_TARGET_SENDER);
                }
            }
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_BEGIN_RCV(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[command].handler != NULL)
        {
            ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType,  dfu->supportedCommands[command].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, command, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_ABORT_XFER(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[command].handler != NULL)
        {
            ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[command].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, command, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_RCV_COMPLETE(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[command].handler != NULL)
        {
            ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[command].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, command, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_RCV_DATA(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[command].handler != NULL)
        {
            ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[command].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, command, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_REBOOT(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[command].handler != NULL)
        {
            ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[command].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, command, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_DEVICE_STATUS(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[command].handler != NULL)
        {
            ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[command].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, command, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_KEEP_ALIVE(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[command].handler != NULL)
        {
            ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[command].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, command, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_BEGIN_SESSION(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[command].handler != NULL)
        {
            dfu->sessionState = SESSION_STATE_STARTING;
            ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[command].userPtr);
            if (!ret)
            {
                dfu->sessionState = SESSION_STATE_INACTIVE;
            }
        }
        else
        {
            dfu->sessionState = SESSION_STATE_STARTING;
            ret = true;
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_END_SESSION(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[command].handler != NULL)
        {
            ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[command].userPtr);
        }
        else
        {
            dfuSendSimpleACK(dfu);
            ret = true;
        }

        dfu->sessionState = SESSION_STATE_INACTIVE;
    }

    return (ret);
}

static bool internalMsgHandler_CMD_IMAGE_STATUS(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                    ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[command].handler != NULL)
        {
            ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[command].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, command, msg, msgLen, msgType);
        }
    }

    return (ret);
}

static bool internalMsgHandler_CMD_BEGIN_SEND(dfuProtocol * dfu, dfuCommandsEnum command,uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[command].handler != NULL)
        {
            ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[command].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, command, msg, msgLen, msgType);
        }
    }

    return (ret);
}


static bool internalMsgHandler_CMD_SEND_DATA(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[command].handler != NULL)
        {
            ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[command].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, command, msg, msgLen, msgType);
        }
    }

    return (ret);    
}

static bool internalMsgHandler_CMD_INSTALL_IMAGE(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
    bool                ret = false;

    if ( (VALID_ADMIN(dfu)) && (msg) && (msgLen > 0) && (VALID_MSG_TYPE(msgType)) )
    {
        if (dfu->supportedCommands[command].handler != NULL)
        {
            ret = dfu->supportedCommands[command].handler(dfu, msg, msgLen, msgType, dfu->supportedCommands[command].userPtr);
        }
        else
        {
            ret = defaultCommandHandler(dfu, command, msg, msgLen, msgType);
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
static bool defaultCommandHandler(dfuProtocol * dfu, dfuCommandsEnum command, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType)
{
	(void)msg;
	(void)msgLen;
	(void)msgType;
	(void)dfu;

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
                        dfuUserPtr userPtr)
{
    dfuProtocol *               ret = NULL;

    if (
           (rxFunct) &&
           (txFunct)
       )
    {
        ret = dfuReserveEnv();
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
bool dfuDestroy(dfuProtocol * dfu)
{
    bool                        ret = false;

    if (VALID_ADMIN(dfu))
    {
        ret = dfuReleaseEnv(dfu);
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

        /*
        ** See if the session or "session startup" state have
        ** timed-out.  If so, call any installed user
        ** error handler.
        **
        */
        if (_dfuDidSessionTimeout(dfu))
        {
            if (dfu->errFunct != NULL)
            {
                dfu->errFunct(dfu, 
                              NULL, 
                              0, 
                              DFU_ERR_SESSION_TIMED_OUT, 
                              dfu->userPtr);
                ret = DDS_SESSION_TIMEOUT;                              
            }            
        }

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
                ** Check to see if the command can be handled in our
                ** current state.  If so, go forward.  If not, ignore
                ** it completely.
                */
                if (dfuShouldAllowCmdForSessionState(dfu, dfu->lastCommand))
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
                    	    if (internalMsgHandlers[dfu->lastCommand].handler(dfu, dfu->lastCommand, msgPtr, msgLen, msgType))
                            {
                                // Retrigger the session time to keep it active
                                TIMER_Start(&dfu->sessionTimer);
                                ret = DDS_OK;
                            }
                            dfu->lastCommand = 0;
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
                              dfuUserPtr userPtr)
{
    bool                    ret = false;

    if (VALID_ADMIN(dfu))
    {
        if ( (command > 0) && (command <= MAX_COMMANDS) )
        {
            dfu->supportedCommands[command].command = command;
            dfu->supportedCommands[command].handler = handler;
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
        ret = (bool)(dfu->sessionState & SESSION_STATE_ACTIVE);
    }

    return (ret);
}

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
bool dfuSetSessionActive(dfuProtocol * dfu)
{
    bool                ret = false;

    if (VALID_ADMIN(dfu))
    {
        // Begin SESSION timer
        TIMER_Start(&dfu->sessionTimer);
        dfu->sessionState = SESSION_STATE_ACTIVE;
        ret = true;
    }

    return (ret);
}

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
bool dfuSetSessionStarting(dfuProtocol * dfu)
{
    bool                ret = false;

    if (VALID_ADMIN(dfu))
    {
        TIMER_Start(&dfu->sessionTimer);
        dfu->sessionState = SESSION_STATE_STARTING;
        ret = true;
    }

    return (ret);
}

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
bool dfuSetSessionInActive(dfuProtocol * dfu)
{
    bool                ret = false;

    if (VALID_ADMIN(dfu))
    {
        ret = dfuSetSessionState(dfu, SESSION_STATE_INACTIVE);
    }

    return (ret);
}

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
bool dfuIsSessionStarting(dfuProtocol * dfu)
{
    bool                    ret = false;

    if (VALID_ADMIN(dfu))
    {
        ret = (bool)(dfu->sessionState & SESSION_STATE_STARTING);
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
bool dfuSetSessionState(dfuProtocol *dfu, uint8_t sessionStates)
{
    bool                        ret = false;

    if (VALID_ADMIN(dfu))
    {
       dfu->sessionState |= sessionStates;
       ret = true;
    }

    return (ret);
}

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
bool dfuClearSessionState(dfuProtocol *dfu, uint8_t sessionStates)
{
    bool                        ret = false;

    if (VALID_ADMIN(dfu))
    {
        dfu->sessionState &= ~sessionStates;
        ret = true;
    }

    return (ret);
}
