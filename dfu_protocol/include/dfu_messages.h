//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_messages.h
**
** DESCRIPTION: DFU message utilities header.
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
#include "dfu_proto_private.h"
#include "dfu_proto_api.h"


#if defined(__cplusplus)
extern "C" {
#endif

/*!
** FUNCTION: dfuBuildMsg_CMD_BEGIN_SESSION
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
uint8_t *dfuBuildMsg_CMD_BEGIN_SESSION(dfuProtocol * dfu,
                                       uint8_t * msg,
                                       dfuMsgTypeEnum msgType);


/*!
** FUNCTION: dfuDecodeMsg_CMD_BEGIN_SESSION
**
** DESCRIPTION: Decodes the BEGIN_SESSION and returns the 
**              challenge password in the "challengePW"
**              parameter.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuDecodeMsg_CMD_BEGIN_SESSION(dfuProtocol * dfu,
                                    uint8_t * msg,
                                    uint16_t msgLen,
                                    uint32_t *challengePW);

/*!
** FUNCTION: dfuBuildMsg_CMD_END_SESSION
**
** DESCRIPTION: Constructs the END_SESSION message in the caller's "msg"
**              target buffer
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint8_t * dfuBuildMsg_CMD_END_SESSION(dfuProtocol * dfu,
                                      uint8_t * msg,
                                      dfuMsgTypeEnum msgType);                                    

/*!
** FUNCTION: dfuBuildMsg_CMD_NEGOTIATE_MTU
**
** DESCRIPTION: Constructs a NEGOTIATE MTU message.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint8_t * dfuBuildMsg_CMD_NEGOTIATE_MTU(dfuProtocol *dfu, 
                                        uint8_t *msg, 
                                        uint16_t mtu,
                                        dfuMsgTypeEnum msgType);

/*!
** FUNCTION: dfuDecodeMsg_CMD_NEGOTIATE_MTU
**
** DESCRIPTION: Pulls the MTU from the message.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuDecodeMsg_CMD_NEGOTIATE_MTU(dfuProtocol *dfu,
                                    uint8_t *msg,
                                    uint16_t msgLen,
                                    uint16_t *mtu);                                        

/*!
** FUNCTION: dfuBuildMsg_CMD_REBOOT
**
** DESCRIPTION: Construct a REBOOT command.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint8_t * dfuBuildMsg_CMD_REBOOT(dfuProtocol * dfu,
								 uint8_t *msg,
                                 uint16_t rebootDelayMS,
								 dfuMsgTypeEnum msgType);

/*!
** FUNCTION: dfuBuildMsg_CMD_DEVICE_STATUS
**
** DESCRIPTION: Build the DEVICE STATUS message.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint8_t * dfuBuildMsg_CMD_DEVICE_STATUS(dfuProtocol * dfu,
								        uint8_t *msg,
                                        uint8_t *bootloaderVersion,  
                                        uint8_t deviceStatusBits,                                      
                                        dfuDeviceTypeEnum deviceType,
								        dfuMsgTypeEnum msgType);                                                                        



/*!
** FUNCTION: dfuBuildMsg_CMD_BEGIN_RCV
**
** DESCRIPTION: Constuct the BEGIN_RCV message using the paramters
**              the caller provides.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint8_t *dfuBuildMsg_CMD_BEGIN_RCV(dfuProtocol * dfu,
                                   uint8_t *msg,
                                   uint8_t imageIndex,
                                   bool isEncrypted,
                                   uint32_t imageSize,
                                   uint32_t imageAddress,
                                   dfuMsgTypeEnum msgType);

/*!
** FUNCTION: dfuDecodeMsg_CMD_BEGIN_RCV
**
** DESCRIPTION: Caller passes in pointers to the parameters that the
**              command provides and this will decode and save the
**              message data to those parameters.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuDecodeMsg_CMD_BEGIN_RCV(dfuProtocol * dfu,
                                uint8_t *msg,
                                uint16_t msgLen,
                                uint8_t *imageIndex,
                                bool *isEncrypted,
                                uint32_t *imageSize,
                                uint32_t *imageDestination);                                   

/*!
** FUNCTION: dfuBuildMsg_CMD_RCV_DATA
**
** DESCRIPTION: Just what it says.  Verifies that the data being
**              sent falls within the MTU
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint8_t * dfuBuildMsg_CMD_RCV_DATA(dfuProtocol * dfu,
                                   uint8_t * msg,
                                   uint8_t * data,
                                   uint16_t dataLen,
                                   dfuMsgTypeEnum msgType);

/*!
** FUNCTION: dufDecodeMsg_CMD_RCV_DATA
**
** DESCRIPTION: Returns a pointer to the data that was sent to us 
**              via the RCV_DATA command.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint8_t * dufDecodeMsg_CMD_RCV_DATA(dfuProtocol * dfu,
                                    uint8_t * msg,
                                    uint16_t msgLen);

/*!
** FUNCTION: dfuBuildMsg_CMD_RCV_COMPLETE
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
uint8_t * dfuBuildMsg_CMD_RCV_COMPLETE(dfuProtocol * dfu,
                                       uint8_t * msg,
                                       uint32_t totalTransferred,
                                       dfuMsgTypeEnum msgType);

/*!
** FUNCTION: dfuDecodeMsg_CMD_RCV_COMPLETE
**
** DESCRIPTION: Caller provides a pointer to a 32-bit int that will
**              receive he # of bytes that were transferred.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool dfuDecodeMsg_CMD_RCV_COMPLETE(dfuProtocol * dfu,
                                   uint8_t * msg,
                                   uint16_t msgLen,
                                   uint32_t *totalTransferred);                                       

#if defined(__cplusplus)
}
#endif