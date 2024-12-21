//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: image_xfer.h
**
** DESCRIPTION: Handles image transfers to a target.
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

#include "dfu_client.h"


#if defined(__cplusplus)
extern "C" {
#endif

bool xferImage(char *filenameStr,
               char *destStr,
               uint8_t imageIndex,
               uint32_t imageAddress,
               bool isEncrypted,
               dfuClientEnvStruct *dfuClient);
        

#if defined(__cplusplus)
}
#endif