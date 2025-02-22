//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: image_xfer.c
**
** DESCRIPTION: Handles sending an image to the target.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined (_WIN64)
    #include <conio.h>
#else
    #include <termios.h>
#endif

#include "image_xfer.h"
#include "general_utils.h"

// Base DFU protocol libraries
#include "dfu_proto_api.h"
#include "dfu_messages.h"

// Client-related libraries
#include "dfu_client.h"
#include "ethernet_sockets.h"
#include "async_timer.h"


#define XFER_TRANSACTION_TIMEOUT_MS             (5000)

/*!
** FUNCTION: xferImage
**
** DESCRIPTION: Transfer an image to the target.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool xferImage(char *filenameStr,
               char *destStr,
               uint8_t imageIndex,
               uint32_t imageAddress,
               bool isEncrypted,
               dfuClientEnvStruct *dfuClient)
{
    bool                            ret = false;
    uint32_t                        imageSize;
    ASYNC_TIMER_STRUCT              startTimer;
    ASYNC_TIMER_STRUCT              endTimer;

    if (
            (destStr) &&
            (filenameStr)
       )
    {
        imageSize = dfuToolGetFileSize(filenameStr);

        if (imageSize > 0)
        {
            FILE *                      handle;

            printf("\r\n *** IMAGE TRANSFER ***");
            printf("\r\n Sending       : %s", filenameStr);
            printf("\r\n File size     : %u bytes", imageSize);
            printf("\r\n Image Index   : %d", imageIndex);
            printf("\r\n FLASH Address : 0x%08X", imageAddress);
            printf("\r\n Encrypted     : %s", isEncrypted ? "yes" : "no");
            fflush(stdout);

            // Open the file
            handle = fopen(filenameStr, "rb");
            if (handle)
            {
                uint8_t                         buffer[1500];
                size_t                          bytesRead = 0;
                uint32_t                        totalSent = 0;
                uint32_t                        totalTransactions = 0;

                /*
                ** First, get the transfer started
                **
                */
                if (dfuClientTransaction_CMD_BEGIN_RCV(dfuClient,
                                                       XFER_TRANSACTION_TIMEOUT_MS,
                                                       destStr,
                                                       imageIndex,
                                                       imageSize,
                                                       imageAddress,
                                                       isEncrypted))
                {
                    TIMER_Start(&startTimer);
                    printf("\n\n");
                    fflush(stdout);

                    ret = true;

                    /*
                    ** Send all of the image file data
                    **
                    */
                    while ((bytesRead = fread(buffer, 1, dfuClientGetInternalMTU(dfuClient)-3, handle)) > 0)
                    {
                        printf("\r >> Exchange #: %5d. Sending [%4u] bytes...                     ", totalTransactions+1, (uint32_t)bytesRead);
                        fflush(stdout);

                        if (!dfuClientTransaction_CMD_RCV_DATA(dfuClient,
                                                               XFER_TRANSACTION_TIMEOUT_MS,
                                                               destStr,
                                                               buffer,
                                                               bytesRead))
                        {
                            printf("\r Client rejected image WRITE operation!          ");
                            ret = false;
                            break;
                        }
                        else
                        {
                            totalSent += bytesRead;
                            ++totalTransactions;
                        }
                    } // while

                    TIMER_Start(&endTimer);
                    printf("\r\n\r\n Sent [%u] bytes.  Total transactions: [%d]", totalSent, totalTransactions);

                    // Now send the "RCV_COMPLETE" transaction
                    if (!dfuClientTransaction_CMD_RCV_COMPLETE(dfuClient,
                                                               XFER_TRANSACTION_TIMEOUT_MS,
                                                               destStr,
                                                               totalSent))
                    {
                        printf("\r\n Target did not accept RCV_COMPLETE command!");
                        ret = false;
                    }

                    fflush(stdout);
                }
                else
                {
                    printf("\r\n Target did not accept BEGIN_RCV command!");
                }

                fclose(handle);
            }
            else
            {
                printf("\r\n Failed to open [%s]!", filenameStr);
            }

            printf("\r\n Total transfer time (mS): %u", (uint32_t)TIMER_GetElapsedMillisecs(&startTimer, &endTimer));
        }
        else
        {
            printf("\r\n File size was ZERO!");
        }
    }
    else
    {
        printf("\r\n Invalid or missing parameters!");
    }

    printf("\r\n");
    fflush(stdout);

    return (ret);
}

