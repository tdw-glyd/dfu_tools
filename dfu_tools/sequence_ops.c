//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: sequence_ops.c
**
** DESCRIPTION: Contains various transaction sequences that carry out the
**              different operations needed with remote targets.
**
** Example:
**     - Establishing a session requires the following sequence:
**
**          1. Send BEGIN_SESSION
**              - Receive a challenge password value from the target
**
**          2. Encrypt the challenge with the proper key, to a file.
**
**          3. Send a BEGIN_RCV, indicating Image ID #127
**
**          4. Send RCV_DATA commands to transfer the entire encrypted
**             challenge to the target.
**
**          5. Once the encrypted challenge key has been fully transferred
**             to the target, send the RCV_COMPLETE
**                  - Receive ACK
**
**          6. Send INSTALL_IMAGE to instruct the target to decrypt the
**             encrypted challenge we just sent, and them compare it
**             to what it sent to us.  If they match, a session is
**             established.
**
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#include <stdio.h>
#include "dfu_client_config.h"
#include "sequence_ops.h"
#include "dfu_client_crypto.h"
#include "image_xfer.h"

#define SO_TRANSACTION_TIMEOUT_MS               (5000)

#define IMAGE_INDEX_SESSION_PASSWORD            (127)


/*
** Internal support prototypes.
**
*/
static bool _imageIndexMustBeEncrypted(uint8_t imageIndex);
#define SHOULD_IMAGE_BE_ENCRYPTED(imageIndex) _imageIndexMustBeEncrypted(imageIndex)


/*!
** FUNCTION: sequenceBeginSession
**
** DESCRIPTION: Performs the necessary sequence of transactions to get a
**              Session set up.
**
**   1. Sends BEGIN_SESSION
**      - Target should send a 32-bit "challenge" password in the clear.
**
**   2. We receive the challenge, then:
**      - Negotiate the MTU with the target.
**      - Encrypt the received challenge to a file.
**
**   3. We transfer the encryped challenge to the target.
**
**   4. We tell the target to install the image.
**      - If the target succeeds in "installing" the encrypted
**        challenge, it will ACK the message. This indicates
**        the Session is now active.  "Installing" in this
**        case means that the target will decrypt the
**        encrypted challenge we sent back and compare that
**        with what it sent.  If they match, the target
**        assumes we're legit and sends an ACK. If the
**        decrypted challenge does NOT match what it sent,
**        it NAK's and the session is NOT active.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool sequenceBeginSession(dfuClientEnvStruct * dfuClient,
                          uint8_t devType,
                          uint8_t devVariant,
                          char * dest,
                          char * challengePubKeyFilename)
{
    bool                    ret = false;

    if ( (dfuClient) && (dest) )
    {
        uint32_t                    challengePW;

        /*
        ** Do a BEGIN_SESSION transaction.
        **
        */
        challengePW = dfuClientTransaction_CMD_BEGIN_SESSION(dfuClient,
                                                             devType,
                                                             devVariant,
                                                             SO_TRANSACTION_TIMEOUT_MS,
                                                             dest);
        if (challengePW != 0)
        {
            uint8_t *               encryptedChallenge = NULL;

            /*
            ** When we have sent a BEGIN_SESSION that results in success,
            ** we can now determine the MTU to use.
            **
            */
            sequenceNegotiateMTU(dfuClient, dest);

            /*
            ** Now that we have the challenge password from the target,
            ** encrypt it to a file.
            **
            */
            encryptedChallenge = ENCRYPT_CHALLENGE(&challengePW, challengePubKeyFilename);
            if (encryptedChallenge != NULL)
            {
                /*
                ** Transfer the image and if that succeeeds, tell
                ** the target to "insall" it.
                **
                */
                if (sequenceTransferAndInstallImage(dfuClient,
                                                    DEFAULT_ENCRYPTED_CHALLENGE_FILENAME,
                                                    IMAGE_INDEX_SESSION_PASSWORD,
                                                    0,
                                                    dest))
                {
                    //
                    // Successfully sent the encrypted challenge key. Delete
                    // its file and send the INSTALL_IMAGE command.  If that
                    // succeeds, the session has been established.
                    //
                    DELETE_ENCRYPTED_CHALLENGE();

                    ret = true;
                }
            }
        }
    }

    return (ret);
}

/*!
** FUNCTION: sequenceEndSession
**
** DESCRIPTION: Performs the set of operations needed to end a session
**              with the target.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool sequenceEndSession(dfuClientEnvStruct * dfuClient, char * dest)
{
    bool                            ret = false;

    if ( (dfuClient) && (dest) )
    {
        ret = dfuClientTransaction_CMD_END_SESSION(dfuClient,
                                                 SO_TRANSACTION_TIMEOUT_MS,
                                                 dest);
    }

    return (ret);
}

/*!
** FUNCTION: sequenceTransferAndInstallImage
**
** DESCRIPTION: Sends a file to the target and then instructs it to be
**              installed.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool sequenceTransferAndInstallImage(dfuClientEnvStruct * dfuClient,
                                     char *imageFilename,
                                     uint8_t imageIndex,
                                     uint32_t imageAddress,
                                     char *dest)
{
    bool                            ret = false;

    if ( (dfuClient) && (imageFilename) && (dest) && (imageIndex > 0) )
    {
        /*
        ** We encrypted the challenge.  Now transfer that back to
        ** the target.
        **
        */
        if (xferImage(imageFilename,
                      dest,
                      imageIndex,
                      imageAddress,
                      SHOULD_IMAGE_BE_ENCRYPTED(imageIndex),
                      dfuClient))
        {
            // Perform the "INSTALL_IMAGE" transaction.
            if (dfuClientTransaction_CMD_INSTALL_IMAGE(dfuClient,
                                                       SO_TRANSACTION_TIMEOUT_MS,
                                                       dest))
            {
                // Image installed successfully!
                ret = true;
            }
        }
    }

    return (ret);
}

/*!
** FUNCTION: sequenceNegotiateMTU
**
** DESCRIPTION: Negotiate the MTU that we will use for image transfer
**              operations.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool sequenceNegotiateMTU(dfuClientEnvStruct * dfuClient, char * dest)
{
    bool                        ret = false;

    if ( (dfuClient) && (dest) )
    {
        uint16_t                retMTU = 0;

        dfuClientSetDestination(dfuClient, dest);

        retMTU = dfuClientTransaction_CMD_NEGOTIATE_MTU(dfuClient,
                                                        SO_TRANSACTION_TIMEOUT_MS,
                                                        dest,
                                                        MAX_ETHERNET_MSG_LEN);
        if (retMTU > 0)
        {
            // Adjust our MTU if the client's is larger than ours
            retMTU = (retMTU > MAX_ETHERNET_MSG_LEN) ? MAX_ETHERNET_MSG_LEN : retMTU;

            // Set the internal MTU
            dfuClientSetInternalMTU(dfuClient, retMTU);
            
            ret = true;
        }
    }

    return (ret);
}

/*!
** FUNCTION: sequenceRebootTarget
**
** DESCRIPTION: Peforms the sequence to reboot a target.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool sequenceRebootTarget(dfuClientEnvStruct * dfuClient, char * dest, uint16_t rebootDelayMS)
{
    bool                        ret = false;
    uint16_t                    localRebootDelay = rebootDelayMS;

    if (localRebootDelay == 0)
    {
        localRebootDelay = 1000;
    }

    if ( (dfuClient) && (dest) )
    {
        // Do the transaction to REBOOT the target.
        ret = dfuClientTransaction_CMD_REBOOT(dfuClient,
                                            SO_TRANSACTION_TIMEOUT_MS,
                                            dest,
                                            localRebootDelay);
    }

    return (ret);
}

/*!
** FUNCTION: macroSequenceInstallImage
**
** DESCRIPTION: This is a "macro" sequence, in that it uses
**              other sequences to perform a top-level
**              operation.  In this case:
**
**   1. Sets up a session
**   2. Negotiates the MTU
**   3. Transfers the specified image to the target.
**   4. Instructs the target to install that image.
**   5. If TRUE, tells the target to reboot.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
bool macroSequenceInstallImage(dfuClientEnvStruct * dfuClient,
                               uint8_t devType,
                               uint8_t devVariant,
                               char * dest,
                               char * challengePubKeyFilename,
                               char *imageFilename,
                               uint8_t imageIndex,
                               uint32_t imageAddress,
                               bool shouldReboot,
                               uint16_t rebootDelayMS                              
                              )
{
    bool                        ret = false;
    
    if (sequenceBeginSession(dfuClient,
                             devType,
                             devVariant,
                             dest,
                             challengePubKeyFilename))
    {
        if (sequenceTransferAndInstallImage(dfuClient,
                                            imageFilename,
                                            imageIndex,
                                            imageAddress,
                                            dest))
        {
            ret = true;

            // Should we reboot the target now?
            if (shouldReboot)
            {
                ret = sequenceRebootTarget(dfuClient, dest, rebootDelayMS);
            }
        }                                                    
    }                                 

    return (ret);
}                              


// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                   INTERNAL SUPPORT FUNCTION IMPLEMENATIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: _imageIndexMustBeEncrypted
**
** DESCRIPTION: Given an image index, this indicates whether the associated
**              image must be encrypted.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static bool _imageIndexMustBeEncrypted(uint8_t imageIndex)
{
    bool                        ret = false;

    ret = ( ((imageIndex >= 1) && (imageIndex <= 96))  || (imageIndex == IMAGE_INDEX_SESSION_PASSWORD) );

    return (ret);
}
