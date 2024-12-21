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
#include "sequence_ops.h"

#define SO_TRANSACTION_TIMEOUT_MS               (5000)


bool sequenceBeginSession(dfuClientEnvStruct * client,
                          uint8_t devType,
                          uint8_t devVariant,
                          char * destinationString)
{
    bool                    ret = false;

    if ( (client) && (destinationString) )
    {
        uint32_t                    challengePW;

        challengePW = dfuClientTransaction_CMD_BEGIN_SESSION(dfuClient,
                                                                devType,
                                                                devVariant,
                                                                SO_TRANSACTION_TIMEOUT_MS,
                                                                destinationString);
        if (challengePW != 0)
        {
            /*
            ** Now that we have the challenge password from the target,
            ** encrypt it to a file.
            **
            */

        }
    }

    return (ret);
}                          