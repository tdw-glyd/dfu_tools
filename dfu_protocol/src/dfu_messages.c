//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_messages.c
**
** DESCRIPTION: DFU message utilities module.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#include <stddef.h>
#include <string.h>
#include "dfu_messages.h"
#include "dfu_platform_utils.h"


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
                                       dfuMsgTypeEnum msgType)
{
    uint8_t *                       ret = NULL;

    if ( (dfu) && (msg) )
    {
        dfuBuildMsgHdr(dfu, msg, CMD_BEGIN_SESSION, msgType);
        ret = msg;
    }

    return (ret);
}

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
                                    uint32_t *challengePW)
{
    bool                    ret = false;

    if ( (dfu) && (msg) && (msgLen) && (challengePW) )
    {
        uint8_t                 *lmsg = msg;
        uint32_t                 lchallengePW;

        ++lmsg;
        memcpy(&lchallengePW, lmsg, 4);
        *challengePW = from_little_endian_32(lchallengePW);
        ret = true;
    }

    return (ret);
}

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
                                      dfuMsgTypeEnum msgType)
{
    uint8_t *                       ret = NULL;

    if ( (dfu) && (msg) )
    {
        dfuBuildMsgHdr(dfu, msg, CMD_END_SESSION, msgType);
        ret = msg;
    }

    return (ret);
}

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
                                        dfuMsgTypeEnum msgType)
{
    uint8_t *                ret = NULL;

    if ( (dfu) && (msg) )
    {
        uint16_t                lMTU = mtu;
        uint8_t *               lmsg = msg;

        // Build the header
        dfuBuildMsgHdr(dfu, lmsg, CMD_NEGOTIATE_MTU, msgType);
        ++lmsg;  // move past header

        // Add the MTU
        lMTU = to_little_endian_16(mtu);
        memcpy(lmsg, &lMTU, sizeof(uint16_t));

        ret = msg;
    }

    return (ret);
}

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
                                    uint16_t *mtu)
{
    bool                ret = false;

    if ( (dfu) && (msg) && (msgLen) && (mtu) )
    {
    	uint16_t					lmtu;
    	uint8_t *                   lmsg = msg;

        ++lmsg; // skip header

        memcpy(&lmtu, lmsg, 2);
        *mtu = from_little_endian_16(lmtu);
        ret = true;
    }

    return (ret);
}

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
                                   dfuMsgTypeEnum msgType)
{
    uint8_t *                   ret = NULL;

    if ( (dfu) && (msg) )
    {
        uint8_t *               lmsg = msg;
        uint32_t				numTarget;

        dfuBuildMsgHdr(dfu, lmsg, CMD_BEGIN_RCV, msgType);
        ++lmsg;

        *lmsg = imageIndex;
        *lmsg <<= 1;
        *lmsg &= 0xFE;
        *lmsg |= (isEncrypted ? 0x01 : 0x00);

        // Add the image size
        ++lmsg;
        numTarget = to_little_endian_32(imageSize);
        numTarget &= 0x00FFFFFF;
        memcpy(lmsg, &numTarget, 3);

        lmsg += 3;
        numTarget = to_little_endian_32(imageAddress);
        numTarget &= 0x00FFFFFF;
        memcpy(lmsg, &numTarget, 3);

        ret = msg;
    }

    return (ret);
}


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
                                uint32_t *imageDestination)
{
    bool                        ret = false;

    if (
            (dfu) &&
            (msg) &&
            (msgLen) &&
            (imageIndex) &&
            (isEncrypted) &&
            (imageSize) &&
            (imageDestination)
       )
    {
        uint8_t             *current = msg;
        uint32_t			 numTarget;

        // Move to the image index/encrypted portion
        ++current;

        // Return if the image is encrypted
        *isEncrypted = (bool)(*current & 0x01);

        // Return the image index
        *imageIndex = *current >> 1;

        // Move to the first byte of the image size
        ++current;
        memcpy(&numTarget, current, 4);
        numTarget &= 0x00FFFFFF;
        *imageSize = from_little_endian_32(numTarget);

        current += 3;
        memcpy(&numTarget, current, 4);
        numTarget &= 0x00FFFFFF;
        *imageDestination = from_little_endian_32(numTarget);

        ret = true;

    }

    return (ret);
}

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
                                   dfuMsgTypeEnum msgType)
{
    uint8_t *                       ret = msg;

    if ( (dfu) && (msg) && (data) && (dataLen) )
    {
        uint8_t *           lmsg = msg;

        if (dataLen <= dfuGetMTU(dfu))
        {
            // Build the header
            dfuBuildMsgHdr(dfu, lmsg, CMD_RCV_DATA, msgType);
            ++lmsg;  // move past header

            // Copy the caller's data to the message
            memcpy(lmsg, data, dataLen);
        }
        else
            ret = NULL;
    }

    return (ret);
}

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
                                    uint16_t msgLen)
{
    uint8_t *                   ret = NULL;

    if ( (dfu) && (msg) && (msgLen) )
    {
        uint8_t *           lmsg = msg;

		++lmsg;
		ret = lmsg;
    }

    return (ret);
}


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
                                       dfuMsgTypeEnum msgType)
{
    uint8_t *                   ret = msg;

    if ( (dfu) && (msg) )
    {
        uint8_t *               lmsg = msg;
        uint32_t                ltransferred = to_little_endian_32(totalTransferred);

        dfuBuildMsgHdr(dfu, lmsg, CMD_RCV_COMPLETE, msgType);

         ++lmsg;
         memcpy(lmsg, &ltransferred, 3);
    }
    else
        ret = NULL;

    return (ret);
}

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
                                   uint32_t *totalTransferred)
{
    bool                        ret = false;

    if ( (dfu) && (msg) && (msgLen) && (totalTransferred) )
    {
        uint8_t *               lmsg = msg;

        ++lmsg;
        memcpy(totalTransferred, lmsg, 3);

        *totalTransferred &= 0x00FFFFFF;
        *totalTransferred = from_little_endian_32(*totalTransferred);

        ret = true;
    }

    return (ret);
}

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
								 dfuMsgTypeEnum msgType)
{
    uint8_t *                   ret = NULL;

    if (dfu)
    {
        uint16_t                ldelayMS = rebootDelayMS;
        uint8_t *               lmsg = msg;

        // Build the header
        dfuBuildMsgHdr(dfu, lmsg, CMD_REBOOT, msgType);
        ++lmsg;  // move past header

        // Add the MTU
        ldelayMS = to_little_endian_16(rebootDelayMS);
        memcpy(lmsg, &ldelayMS, sizeof(uint16_t));

        ret = msg;
    }

    return (ret);
}

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
								        uint8_t * msg,
                                        uint8_t * bootloaderVersion,
                                        uint8_t deviceStatusBits,
                                        dfuDeviceTypeEnum deviceType,
								        dfuMsgTypeEnum msgType)
{
    uint8_t *                       ret = NULL;

    if ( (dfu) && (msg) )
    {
        uint8_t *       lmsg = msg;
        uint8_t         devType;
        uint16_t        luptimeMins;

        // Build the header
        dfuBuildMsgHdr(dfu, lmsg, CMD_DEVICE_STATUS, msgType);
        ++lmsg;  // move past header

        // Add bootloader version (must be a 3-byte array MM.mm.rr)
        memcpy(lmsg, bootloaderVersion, 3);
        lmsg += 3;

        // Add in flags
        *lmsg = deviceStatusBits & DEVICE_STATUS_BIT_MASK;
        ++lmsg;

        // Add device type
        devType = (uint8_t)deviceType;
        devType <<= 3;
        *lmsg = devType;
        ++lmsg;

        // Uptime mins
        luptimeMins = to_little_endian_16(dfuGetUptimeMins(dfu));
        memcpy(lmsg, &luptimeMins, 2);

        ret = msg;
    }

    return (ret);
}

