//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: sequence_ops.h
**
** DESCRIPTION: Operation sequences header.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once

#include "dfu_client.h"



#if defined(__cplusplus)
extern "C" {
#endif


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
                          char * challengePubKeyFilename);

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
bool sequenceEndSession(dfuClientEnvStruct * dfuClient, char * dest);

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
                                     char *dest);

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
bool sequenceNegotiateMTU(dfuClientEnvStruct * dfuClient, char * dest);

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
bool sequenceRebootTarget(dfuClientEnvStruct * dfuClient, char * dest, uint16_t rebootDelayMS);

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
                              );

#if defined(__cplusplus)
}
#endif
