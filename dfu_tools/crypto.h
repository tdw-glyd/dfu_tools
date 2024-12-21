//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: crypto.h
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

#if defined(__cplusplus)
extern "C" {
#endif

uint8_t * encryptDataWithPublicKey(const char *pubkey_filename,
                                   void *valueToEncrypt,
                                   uint32_t encryptLength,
                                   bool shouldSave,
                                   const char *output_filename);


#if defined(__cplusplus)
}
#endif
