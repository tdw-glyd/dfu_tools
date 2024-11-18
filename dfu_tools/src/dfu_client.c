//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_lib_support.c
**
** DESCRIPTION: Provides registration, callbacks, etc.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#include "dfu_client.h"
#include "iface_enet.h"

static bool dfuClientInitEnv(dfuClientEnvStruct *env);
static dfuClientEnvStruct *dfuClientAllocEnv(void);
static bool dfuClientFreeEnv(dfuClientEnvStruct *env);

/*
** How many library interaces can be active at the same time?
**
*/
#define MAX_INTERFACES   (MAX_ETHERNET_INTERFACES + MAX_CAN_INTERFACES)


/*
** Holds instance-specific data.
**
*/
#define DFU_TOOL_ENV_SIGNATURE      (0x08E189AC)
struct dfuClientEnvStruct
{
    uint32_t                        signature;

    // References the current instance of the DFU protocol
    dfuProtocol *                   dfu;

    // Which interface type is this instance using?
    dfuClientInterfaceTypeEnum      interfaceType;

    // Holds the Ethernet interface environment
    ifaceEthEnvStruct *             ethernetEnv;

    // Transaction-based user pointer
    void *                          userPtr;

    // Holds the CAN interface environment
    // NOT YET IMPLEMENATED

    bool                        transactionComplete;
};

/*
** Contains concrete instances of the management environment.
**
*/
static dfuClientEnvStruct         envs[MAX_INTERFACES];


/*
** Handy macros
**
*/
#define VALID_ENV(env) ( (env != NULL) && (env->signature == DFU_TOOL_ENV_SIGNATURE) )


// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                            PUBLIC API FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: dfuClientInit
**
** DESCRIPTION: Sets up the library.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
dfuClientEnvStruct * dfuClientInit(dfuClientInterfaceTypeEnum interfaceType, const char * interfaceName)
{
    dfuClientEnvStruct *          env = dfuClientAllocEnv();

    if (VALID_ENV(env))
    {
        // Save the kind of interface we're initializing
        env->interfaceType = interfaceType;

        switch (env->interfaceType)
        {
            case DFUCLIENT_INTERFACE_ETHERNET:
                env->ethernetEnv = dfuClientEthernetInit(&env->dfu, interfaceName, (void *)&env);
                dfuClientSetInternalMTU(env, MAX_ETHERNET_MSG_LEN);
            break;

            case DFUCLIENT_INTERFACE_CAN:
                // Add equivalent CAN "init" function here
            break;

            case DFUCLIENT_INTERFACE_UART:
                // Add equivalent UART "init" function here
            break;

            default:
            break;
        }

        if (env->dfu == NULL)
        {
            dfuClientFreeEnv(env);
            env = NULL;
        }
    }

    return (env);
}

/*!
** FUNCTION: dfuClientRawTransaction
**
** DESCRIPTION: This allows a client to build a message, then pass it here to be sent
**              to the destination.  It doesn't care what the media type is.  Following
**              transmission, it starts a timer and begins driving the "drive()" function
**              of the DFU protocol library.  When that library recieves a response to the
**              message, the dfu library calls the handler that is installed to process it
**              which is then responsible to call the "dfutoolSetTransactionComplate()"
**              function, which sets a "trnsactionComplete" flag in our environment.
**              When this happens, or a timeout occurs, this function exits.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool dfuClientRawTransaction(dfuClientEnvStruct * env, uint8_t *msg, uint16_t msgLen, bool broadcast, uint32_t timeout)
{
    bool                            ret = false;

    if ( (VALID_ENV(env)) && (env->dfu) && (msg) && (msgLen) )
    {
        ASYNC_TIMER_STRUCT                  timer;

        // Send the message
        if (dfuSendMsg(env->dfu, msg, msgLen, (broadcast ? DFU_TARGET_ANY : DFU_TARGET_SENDER)))
        {
            dfuDriveStateEnum               driveState;

            dfuClientSetTransactionComplete(env, false);

            TIMER_Start(&timer);
            do
            {
                // Drive
                driveState = dfuDrive(env->dfu);

                // Check result
                if (env->transactionComplete == true)
                {
                    ret = (driveState == DDS_OK);
                    break;
                }

                //Sleep(0);
            } while (!TIMER_Finished(&timer, timeout));
        }
    }

    return (ret);
}

/*!
** FUNCTION: dfuClientSetDestination
**
** DESCRIPTION: The caller can set the destination of messages with this.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool dfuClientSetDestination(dfuClientEnvStruct * env, char *dest)
{
    bool                        ret = false;

    if ( (VALID_ENV(env)) && (dest) )
    {
        switch (env->interfaceType)
        {
            case DFUCLIENT_INTERFACE_ETHERNET:
                if (env->ethernetEnv)
                {
                    ret = dfuClientEthernetSetDest(env->ethernetEnv, dest);
                }
            break;

            case DFUCLIENT_INTERFACE_CAN:
            break;

            default:
            break;
        }
    }

    return (ret);
}

/*!
** FUNCTION: dfuClientGetDFU
**
** DESCRIPTION: Returns the DFU protocol instance pointer.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
dfuProtocol * dfuClientGetDFU(dfuClientEnvStruct * env)
{
    dfuProtocol *               ret = NULL;

    if (VALID_ENV(env))
    {
        ret = env->dfu;
    }

    return (ret);
}

/*!
** FUNCTION: dfuClientSetTransactionComplete
**
** DESCRIPTION: Caller can set the transaction state here.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
void dfuClientSetTransactionComplete(dfuClientEnvStruct *env, bool state)
{
    if (VALID_ENV(env))
    {
        env->transactionComplete = (bool)state;
    }

    return;
}

/*!
** FUNCTION: dfuClientSetInternalMTU
**
** DESCRIPTION: Sets the MTU that will be used until the next time
**              this is called.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool dfuClientSetInternalMTU(dfuClientEnvStruct *env, uint16_t mtu)
{
    bool                    ret = false;

    if (VALID_ENV(env))
    {
        ret = dfuSetMTU(env->dfu, mtu);
    }

    return (ret);
}

/*!
** FUNCTION: dfuClientGetInternalMTU
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
uint16_t dfuClientGetInternalMTU(dfuClientEnvStruct *env)
{
    uint16_t                ret = 0;

    if (VALID_ENV(env))
    {
        ret =  dfuGetMTU(env->dfu);
    }

    return (ret);
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                        PUBLIC TRANSACTION FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*
              BEGIN_SESSION transaction functions

          NOTE: This is a TEMPORARY version that does
          NOT perform the cryptographic operarions.
*/
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

static bool responseHandler_CMD_BEGIN_SESSION(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr);

/*!
** FUNCTION: dfuClientTransaction_CMD_BEGIN_SESSION
**
** DESCRIPTION: Set up a session with the target.
**
** PARAMETERS:
**
** RETURNS:  The challenge password send by the far end
**
** COMMENTS:
**
*/
uint32_t dfuClientTransaction_CMD_BEGIN_SESSION(dfuClientEnvStruct *dfuClient,
                                                uint32_t timeoutMS,
                                                char *dest)
{
    uint32_t                        ret = 0;

    if ( (VALID_ENV(dfuClient)) && (dest) )
    {
        uint8_t                      msg[128];

        // Set up to send to the desired target
        dfuClientSetDestination(dfuClient, dest);

        // Register the MTU handler
        dfuInstallCommandHandler(GET_DFU(dfuClient),
                                 CMD_BEGIN_SESSION,
                                 responseHandler_CMD_BEGIN_SESSION,
                                 SESSION_STATE_ANY,
                                 dfuClient);

        // Build a message
        if (dfuBuildMsg_CMD_BEGIN_SESSION(GET_DFU(dfuClient),
                                          msg,
                                          MSG_TYPE_COMMAND))
        {
            uint32_t                challengePW;

            dfuClient->userPtr = &challengePW;
            if (dfuClientRawTransaction(dfuClient, msg, 1, false, timeoutMS))
            {
                ret = challengePW;
            }
        }

        // Remove response handler
        dfuRemoveCommandHandler(GET_DFU(dfuClient), CMD_NEGOTIATE_MTU);
    }

    return (ret);
}

static bool responseHandler_CMD_BEGIN_SESSION(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr)
{
    bool                  ret = false;
    dfuClientEnvStruct *  env = (dfuClientEnvStruct *)userPtr;

    if ( (VALID_ENV(env)) && (msgType == MSG_TYPE_ACK) )
    {
        ret = true;
    }

     dfuClientSetTransactionComplete(env, true);

    return (ret);
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*
              END_SESSION transaction functions
*/
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

static bool responseHandler_CMD_END_SESSION(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr);

/*!
** FUNCTION: dfuClientTransaction_CMD_END_SESSION
**
** DESCRIPTION: Terminate a session
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool dfuClientTransaction_CMD_END_SESSION(dfuClientEnvStruct *dfuClient,
                                          uint32_t timeoutMS,
                                          char *dest)
{
    bool                            ret = false;

    if ( (VALID_ENV(dfuClient)) && (dest) )
    {
        uint8_t                      msg[128];

        // Set up to send to the desired target
        dfuClientSetDestination(dfuClient, dest);

        // Register the MTU handler
        dfuInstallCommandHandler(GET_DFU(dfuClient),
                                CMD_END_SESSION,
                                responseHandler_CMD_END_SESSION,
                                SESSION_STATE_ANY,
                                dfuClient);

        // Build a message
        if (dfuBuildMsg_CMD_END_SESSION(GET_DFU(dfuClient),
                                        msg,
                                        MSG_TYPE_COMMAND))
        {
            if (dfuClientRawTransaction(dfuClient, msg, 1, false, timeoutMS))
            {
                ret = true;
            }
        }

        // Remove response handler
        dfuRemoveCommandHandler(GET_DFU(dfuClient), CMD_NEGOTIATE_MTU);
    }

    return (ret);
}

static bool responseHandler_CMD_END_SESSION(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr)
{
    bool                  ret = false;
    dfuClientEnvStruct *  env = (dfuClientEnvStruct *)userPtr;

    if ( (VALID_ENV(env)) && (msgType == MSG_TYPE_ACK) )
    {
        ret = true;
    }

     dfuClientSetTransactionComplete(env, true);

    return (ret);
}


// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*
              NEGOTIATE_MTU transaction functions
*/
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

static bool responseHandler_CMD_NEGOTIATE_MTU(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr);

/*!
** FUNCTION: responseHandler_CMD_NEGOTIATE_MTU
**
** DESCRIPTION: Callback that is registered for the response to a
**              NEGOTIATE_MTU command.  What is
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
uint16_t dfuClientTransaction_CMD_NEGOTIATE_MTU(dfuClientEnvStruct *dfuClient,
                                                uint32_t timeoutMS,
                                                char *dest,
                                                uint16_t myMTU)
{
    uint16_t                        ret = 0;

    if ( (VALID_ENV(dfuClient)) && (dest) && (myMTU) )
    {
        uint8_t                      msg[128];
        uint16_t                     resultMTU;

        // Set up to send to the desired target
        dfuClientSetDestination(dfuClient, dest);

        // Register the MTU handler
        dfuInstallCommandHandler(GET_DFU(dfuClient),
                                CMD_NEGOTIATE_MTU,
                                responseHandler_CMD_NEGOTIATE_MTU,
                                SESSION_STATE_ANY,
                                dfuClient);

        // Build a message
        if (dfuBuildMsg_CMD_NEGOTIATE_MTU(GET_DFU(dfuClient),
                                        msg,
                                        myMTU,
                                        MSG_TYPE_COMMAND))
        {
            dfuClient->userPtr = &resultMTU; // save a pointer to the result value
            if (dfuClientRawTransaction(dfuClient, msg, 3, false, timeoutMS))
            {
                ret = resultMTU;
            }
        }

        // Remove response handler
        dfuRemoveCommandHandler(GET_DFU(dfuClient), CMD_NEGOTIATE_MTU);
    }

    return (ret);
}

/*!
** FUNCTION: responseHandler_CMD_NEGOTIATE_MTU
**
** DESCRIPTION: Callback that is registered for the response to a
**              NEGOTIATE_MTU command.  What is
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static bool responseHandler_CMD_NEGOTIATE_MTU(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr)
{
    bool                  ret = true;
    dfuClientEnvStruct *  env = (dfuClientEnvStruct *)userPtr;
    uint16_t              mtu = 0;

    if ( (VALID_ENV(env)) && (msgType == MSG_TYPE_RESPONSE) )
    {
        // Decompose the message
        if (dfuDecodeMsg_CMD_NEGOTIATE_MTU(dfu,
                                           msg,
                                           msgLen,
                                          &mtu))
        {
            // Save result
            if (env->userPtr)
            {
                *((uint16_t *)env->userPtr) = mtu;
            }
        }

        // Finish the transaction
        dfuClientSetTransactionComplete(env, true);
    }

    return (ret);
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*
               BEGIN_RCV transaction functions
*/
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

static bool responseHandler_CMD_BEGIN_RCV(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr);

/*!
** FUNCTION: dfuClientTransaction_CMD_BEGIN_RCV
**
** DESCRIPTION: Performs the "BEGIN RCV" transactions and returns the result
**              as TRUE if the target responded with a simple ACK. False else...
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool dfuClientTransaction_CMD_BEGIN_RCV(dfuClientEnvStruct *dfuClient,
                                        uint32_t timeoutMS,
                                        char *dest,
                                        uint8_t imageIndex,
                                        uint32_t imageSize,
                                        uint32_t imageAddress,
                                        bool isEncrypted)
{
    bool                        ret = false;

    if ( (VALID_ENV(dfuClient)) && (imageIndex) && (imageSize) )
    {
        uint8_t                      msg[128];

        // Set up to send to the desired target
        dfuClientSetDestination(dfuClient, dest);

        // Register the MTU handler
        dfuInstallCommandHandler(GET_DFU(dfuClient),
                                CMD_BEGIN_RCV,
                                responseHandler_CMD_BEGIN_RCV,
                                SESSION_STATE_ANY,
                                dfuClient);

        // Build a message
        if (dfuBuildMsg_CMD_BEGIN_RCV(GET_DFU(dfuClient),
                                      msg,
                                      imageIndex,
                                      isEncrypted,
                                      imageSize,
                                      imageAddress,
                                      MSG_TYPE_COMMAND))
        {
            ret = dfuClientRawTransaction(dfuClient, msg, 8, false, timeoutMS);
        }

        // Remove response handler
        dfuRemoveCommandHandler(GET_DFU(dfuClient), CMD_BEGIN_RCV);
    }

    return (ret);
}

static bool responseHandler_CMD_BEGIN_RCV(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr)
{
    bool                  ret = false;
    dfuClientEnvStruct *  env = (dfuClientEnvStruct *)userPtr;

    if ( (VALID_ENV(env)) && (msgType == MSG_TYPE_ACK) )
    {
        ret = true;
    }

     dfuClientSetTransactionComplete(env, true);

    return (ret);
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*
                 RCV_DATA transaction functions
*/
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

static bool responseHandler_CMD_RCV_DATA(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr);

/*!
** FUNCTION: dfuClientTransaction_CMD_RCV_DATA
**
** DESCRIPTION: Send a block of an image to the destination.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool dfuClientTransaction_CMD_RCV_DATA(dfuClientEnvStruct *dfuClient,
                                       uint32_t timeoutMS,
                                       char *dest,
                                       uint8_t *imageData,
                                       uint32_t imageDataLen)
{
    bool                    ret = false;

    if ( (VALID_ENV(dfuClient)) && (imageData) && (imageDataLen) && (dest) )
    {
        uint8_t                     msg[2048];

        memset(msg, 0xFF, sizeof(msg));

        // Set up to send to the desired target
        dfuClientSetDestination(dfuClient, dest);

        // Set up to handle the response
        dfuInstallCommandHandler(GET_DFU(dfuClient),
                                 CMD_RCV_DATA,
                                 responseHandler_CMD_RCV_DATA,
                                 SESSION_STATE_ANY,
                                 dfuClient);

        // Build the message
        if (dfuBuildMsg_CMD_RCV_DATA(GET_DFU(dfuClient),
                                     msg,
                                     imageData,
                                     imageDataLen,
                                     MSG_TYPE_COMMAND))
        {
            ret = dfuClientRawTransaction(dfuClient,
                                          msg,
                                          dfuClientGetInternalMTU(dfuClient),
                                          false,
                                          timeoutMS);
        }

        // Remove response handler
        dfuRemoveCommandHandler(GET_DFU(dfuClient), CMD_RCV_DATA);
    }

    return (ret);
}

static bool responseHandler_CMD_RCV_DATA(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr)
{
    bool                  ret = false;
    dfuClientEnvStruct *  env = (dfuClientEnvStruct *)userPtr;

    if ( (VALID_ENV(env)) && (msgType == MSG_TYPE_ACK) )
    {
        ret = true;
    }

    dfuClientSetTransactionComplete(env, true);

    return (ret);
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*
               RCV_COMPLETE transaction functions
*/
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
static bool responseHandler_CMD_RCV_COMPLETE(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr);


/*!
** FUNCTION: dfuClientTransaction_CMD_RCV_COMPLETE
**
** DESCRIPTION: Send the final "RCV_COMPLETE" message, including the
**              total # of bytes transferred.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool dfuClientTransaction_CMD_RCV_COMPLETE(dfuClientEnvStruct *dfuClient,
                                           uint32_t timeoutMS,
                                           char *dest,
                                           uint32_t totalBytesXferred)
{
    bool                            ret = false;

    if ( (VALID_ENV(dfuClient)) && (totalBytesXferred) && (dest) )
    {
        uint8_t                     msg[128];

        // Set up to send to the desired target
        dfuClientSetDestination(dfuClient, dest);

        // Set up to handle the response
        dfuInstallCommandHandler(GET_DFU(dfuClient),
                                CMD_RCV_COMPLETE,
                                responseHandler_CMD_RCV_COMPLETE,
                                SESSION_STATE_ANY,
                                dfuClient);

        if (dfuBuildMsg_CMD_RCV_COMPLETE(GET_DFU(dfuClient),
                                         msg,
                                         totalBytesXferred,
                                         MSG_TYPE_COMMAND))
        {
            ret = dfuClientRawTransaction(dfuClient,
                                          msg,
                                          4,
                                          false,
                                          timeoutMS);
        }

        // Remove response handler
        dfuRemoveCommandHandler(GET_DFU(dfuClient), CMD_RCV_COMPLETE);
    }

    return (ret);
}


static bool responseHandler_CMD_RCV_COMPLETE(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr)
{
    bool                  ret = false;
    dfuClientEnvStruct *  env = (dfuClientEnvStruct *)userPtr;

    if ( (VALID_ENV(env)) && (msgType == MSG_TYPE_ACK) )
    {
        ret = true;
    }

    dfuClientSetTransactionComplete(env, true);

    return (ret);
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                         INTERNAL SUPPORT FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: dfuClientInitEnv
**
** DESCRIPTION: Initialize the instance data.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static bool dfuClientInitEnv(dfuClientEnvStruct *env)
{
    bool                ret = false;

    if (env)
    {
        env->dfu = NULL;
        env->signature = DFU_TOOL_ENV_SIGNATURE;
        ret = true;
    }

    return (ret);
}

/*!
** FUNCTION: dfuClientAllocEnv
**
** DESCRIPTION: Search the ENV pool for an avaialable env item.
**              If found, initialize it and return a pointer to
**              that object.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static dfuClientEnvStruct *dfuClientAllocEnv(void)
{
    dfuClientEnvStruct *  ret = NULL;
    int                   index;

    for (index = 0; index < MAX_INTERFACES; index++)
    {
        if (envs[index].signature != DFU_TOOL_ENV_SIGNATURE)
        {
            ret = &envs[index];
            if (dfuClientInitEnv(&envs[index]))
            {
                ret = &envs[index];
                break;
            }
        }
    }

    return (ret);
}

/*!
** FUNCTION: dfuClientFreeEnv
**
** DESCRIPTION: Release the environment back to the pool
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static bool dfuClientFreeEnv(dfuClientEnvStruct *env)
{
    bool                        ret = false;

    if (VALID_ENV(env))
    {
        if (env->dfu)
        {
            dfuUnInit(env->dfu);
        }

        ret = dfuClientInitEnv(env);
        env->signature = 0x00000000;
    }

    return (ret);
}

