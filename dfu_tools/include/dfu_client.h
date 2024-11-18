//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_lib_support.h
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
#include <stddef.h>
#include "ethernet_sockets.h"
#include "dfu_proto_api.h"
#include "dfu_messages.h"
#include "dfu_client_config.h"

/*
** Opaque object reference used by library API functions.
**
*/
typedef struct dfuClientEnvStruct dfuClientEnvStruct;

/*
** Used to specify what the interface will be.
**
*/
typedef enum
{
    DFUCLIENT_INTERFACE_ETHERNET,
    DFUCLIENT_INTERFACE_CAN,
    DFUCLIENT_INTERFACE_UART
}dfuClientInterfaceTypeEnum;



#if defined(__cplusplus)
extern "C" {
#endif

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                        PUBLIC TRANSACTION PROTOTYPES
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: dfuClientTransaction_CMD_BEGIN_SESSION
**
** DESCRIPTION: Set up a session with the target.
**
** PARAMETERS:
**
** RETURNS: The challenge password send by the far end.
**
** COMMENTS:
**
*/
uint32_t dfuClientTransaction_CMD_BEGIN_SESSION(dfuClientEnvStruct *dfuClient,
                                                uint32_t timeoutMS,
                                                char *dest);

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
                                          char *dest);

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
                                                uint16_t myMTU);


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
                                        bool isEncrypted);

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
                                       uint32_t imageDataLen);


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
                                           uint32_t totalBytesXferred);

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                             PUBLIC API PROTOTYPES
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
dfuClientEnvStruct * dfuClientInit(dfuClientInterfaceTypeEnum interfaceType, const char * interfaceName);

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
bool dfuClientSetDestination(dfuClientEnvStruct * env, char *dest);

/*!
** FUNCTION: dfuClientRawTransaction
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
bool dfuClientRawTransaction(dfuClientEnvStruct * env, uint8_t *msg, uint16_t msgLen, bool broadcast, uint32_t timeout);

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
void dfuClientSetTransactionComplete(dfuClientEnvStruct *env, bool state);

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
dfuProtocol * dfuClientGetDFU(dfuClientEnvStruct * env);

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
bool dfuClientSetInternalMTU(dfuClientEnvStruct *env, uint16_t mtu);

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
uint16_t dfuClientGetInternalMTU(dfuClientEnvStruct *env);


/*
** Nice macros
**
*/
#define GET_DFU(env) (dfuClientGetDFU(env))

#if defined(__cplusplus)
}
#endif
