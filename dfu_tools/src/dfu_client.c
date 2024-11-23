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
static bool dfuClientInstallResponseHandler(dfuCommandsEnum command,
                                            dfuCommandHandler responseHandler,
                                            dfuClientEnvStruct * env);
static bool dfuClientRemoveResponseHandler(dfuClientEnvStruct * env);

/*
** How many library interaces can be active at the same time?
**
*/
#define MAX_INTERFACES   (MAX_ETHERNET_INTERFACES + MAX_CAN_INTERFACES)


/*
** Holds instance-specific data.
**
*/
#define DFU_CLIENT_ENV_SIGNATURE      (0x08E189AC)
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
    dfuCommandsEnum             transactionCommand;
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
#define VALID_ENV(env) ( (env != NULL) && (env->signature == DFU_CLIENT_ENV_SIGNATURE) )


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
** COMMENTS: Since the caller is expected to provide a valid message, we
**           extract the COMMAND value directly from it and use it to
**           let us install the response callback handler for that command
**           value and to match response to command, etc.  The caller of 
**           this function only needs to give us the message, the destination
**           and a few other bits for us to perform the transaction.
**
*/
bool dfuClientRawTransaction(dfuClientEnvStruct * env,
                             char *dest,
                             dfuCommandHandler responseHandler,
                             uint8_t *msg,
                             uint16_t msgLen,
                             bool broadcast,
                             uint32_t timeout)
{
    bool                            ret = false;

    if (
           (VALID_ENV(env)) &&
           (responseHandler) &&
           (env->dfu) &&
           (msg) &&
           (msgLen)
       )
    {
        ASYNC_TIMER_STRUCT                  timer;
        dfuCommandsEnum                     command;

        /*
        ** Extract the command value and validate
        **
        */
        command = CMD_FROM_MSG(msg);
        if (!VALID_CMD_ID(command))
        {
            return (ret);
        }

        // First, set up to send to the desired target
        dfuClientSetDestination(env, dest);

        // Now install the response handler.
        dfuClientInstallResponseHandler(command,
                                        responseHandler,
                                        env);

        /*
        ** Now, send the message and wait for the response handler to
        ** either indicate success, or a timeout to occur.
        */
        if (dfuSendMsg(env->dfu, msg, msgLen, (broadcast ? DFU_TARGET_ANY : DFU_TARGET_SENDER)))
        {
            dfuDriveStateEnum               driveState;

            // Init the transaction state
            dfuClientSetTransactionComplete(env, false);

            /*
            ** Loop, waiting for either a response or a timeout.
            **
            */
            TIMER_Start(&timer);
            do
            {
                // Drive, calling DFU main protocol "drive" method
                driveState = dfuDrive(env->dfu);

                /*
                ** If a response handler was invoked and it returns
                ** TRUE, we get back a "DDS_OK" that tells us the 
                ** transaction has completed.  If that doesn't
                ** happen within the timeout period, we exit with
                ** FALSE as a result.
                **
                */
                if (driveState == DDS_OK)
                {
                    ret = true;
                    dfuClientSetTransactionComplete(env, true);
                    break;
                }

                Sleep(0);
            } while (!TIMER_Finished(&timer, timeout));
        }

        // Automatically remove the response handler for the command value.
        dfuClientRemoveResponseHandler(env);
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
/*
** "Transactions" consist of two parts:
**
**    1. The publicly-visible specific transaction API function.  
**    2. The internally-visible function that will be called if and when a
**       response to that command arrives.
**
** 
** The first function uses the "dfu_message" library to construct the desired
** comamnd message, then calls the "raw transaction" function, passing in
** the newly-constructed message, the destination, the handler for the 
** response and a timeout for no-or-invalid response.  That function
** then calls the main DFU protocol engine "drive" function.  Since
** the interface will have installed media driver functions, "drive"
** will call the proper response handler we just installed when a response
** message arrives, and if that function likes what it sees, the tran
** transaction is considered complete. 
**
** Since the response handler callback is passed our client environment
** data, we can use that to set the received values, etc. as needed
** for that particular transaction.
**
*/

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

        // Build a message
        if (dfuBuildMsg_CMD_BEGIN_SESSION(GET_DFU(dfuClient),
                                          msg,
                                          MSG_TYPE_COMMAND))
        {
            uint32_t                challengePW;

            dfuClient->userPtr = &challengePW;
            if (dfuClientRawTransaction(dfuClient,
                                        dest,
                                        responseHandler_CMD_BEGIN_SESSION,
                                        msg,
                                        1,
                                        false,
                                        timeoutMS))
            {
                ret = challengePW;
            }
        }
    }

    return (ret);
}

static bool responseHandler_CMD_BEGIN_SESSION(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuMsgTypeEnum msgType, dfuUserPtr userPtr)
{
    bool                  ret = false;
    dfuClientEnvStruct *  env = (dfuClientEnvStruct *)userPtr;

    if ( (VALID_ENV(env)) && (msgType == MSG_TYPE_ACK) )
    {
        dfuSetSessionActive(dfu);  // TEMPORARY
        ret = true;
    }

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

        // Build a message
        if (dfuBuildMsg_CMD_END_SESSION(GET_DFU(dfuClient),
                                        msg,
                                        MSG_TYPE_COMMAND))
        {
            if (dfuClientRawTransaction(dfuClient,
                                        dest,
                                        responseHandler_CMD_END_SESSION,
                                        msg,
                                        1,
                                        false,
                                        timeoutMS))
            {
                ret = true;
            }
        }
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

        // Build a message
        if (dfuBuildMsg_CMD_NEGOTIATE_MTU(GET_DFU(dfuClient),
                                          msg,
                                          myMTU,
                                          MSG_TYPE_COMMAND))
        {
            dfuClient->userPtr = &resultMTU; // save a pointer to the result value
            if (dfuClientRawTransaction(dfuClient,
                                        dest,
                                        responseHandler_CMD_NEGOTIATE_MTU,
                                        msg,
                                        3,
                                        false,
                                        timeoutMS))
            {
                ret = resultMTU;
            }
        }
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

        // Build a message
        if (dfuBuildMsg_CMD_BEGIN_RCV(GET_DFU(dfuClient),
                                      msg,
                                      imageIndex,
                                      isEncrypted,
                                      imageSize,
                                      imageAddress,
                                      MSG_TYPE_COMMAND))
        {
            ret = dfuClientRawTransaction(dfuClient,
                                          dest,
                                          responseHandler_CMD_BEGIN_RCV,
                                          msg,
                                          8,
                                          false,
                                          timeoutMS);
        }
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

        // Build the message
        if (dfuBuildMsg_CMD_RCV_DATA(GET_DFU(dfuClient),
                                     msg,
                                     imageData,
                                     imageDataLen,
                                     MSG_TYPE_COMMAND))
        {
            ret = dfuClientRawTransaction(dfuClient,
                                          dest,
                                          responseHandler_CMD_RCV_DATA,
                                          msg,
                                          dfuClientGetInternalMTU(dfuClient),
                                          false,
                                          timeoutMS);
        }
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

        if (dfuBuildMsg_CMD_RCV_COMPLETE(GET_DFU(dfuClient),
                                         msg,
                                         totalBytesXferred,
                                         MSG_TYPE_COMMAND))
        {
            ret = dfuClientRawTransaction(dfuClient,
                                          dest,
                                          responseHandler_CMD_RCV_COMPLETE,
                                          msg,
                                          4,
                                          false,
                                          timeoutMS);
        }
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

    return (ret);
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                         INTERNAL SUPPORT FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: dfuClientInstallResponseHandler
**
** DESCRIPTION: Installs the desired response handler for a specified command.
**              Saves the command so that we can automatically remove
**              the handler once a transaction completes (either with
**              success or failure)
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static bool dfuClientInstallResponseHandler(dfuCommandsEnum command,
                                            dfuCommandHandler responseHandler,
                                            dfuClientEnvStruct * env)
{
    bool                    ret = true;

    if (
           (env) &&
           (responseHandler) &&
           (VALID_CMD_ID(command))
       )
    {
        if (dfuInstallCommandHandler(GET_DFU(env),
                                     command,
                                     responseHandler,
                                     env))
        {
            // 
            // Save the command value so we can uninstall 
            // the response handler when the transaction
            // completes.
            //
            env->transactionCommand = command;
            ret = true;
        }
    }

    return (ret);
}

/*!
** FUNCTION: dfuClientRemoveResponseHandler
**
** DESCRIPTION: Removes any existing response handler for the most-recent
**              command transaction.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static bool dfuClientRemoveResponseHandler(dfuClientEnvStruct * env)
{
    bool                    ret = false;

    if (
           (env) &&
           (VALID_CMD_ID(env->transactionCommand))
       )
    {
        if (dfuRemoveCommandHandler(GET_DFU(env), env->transactionCommand))
        {
            // The the command value to invalid.
            env->transactionCommand = 0;
            ret = true;
        }
    }

    return (ret);
}

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
        env->signature = DFU_CLIENT_ENV_SIGNATURE;
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
        if (envs[index].signature != DFU_CLIENT_ENV_SIGNATURE)
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
            dfuDestroy(env->dfu);
        }

        ret = dfuClientInitEnv(env);
        env->signature = 0x00000000;
    }

    return (ret);
}

