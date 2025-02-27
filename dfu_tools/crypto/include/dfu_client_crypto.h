//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_client_crypto.h
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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "image_metadata.h"


#define DEFAULT_ENCRYPTED_CHALLENGE_FILENAME            ("./encrypted_chal.bin")

#if defined(__cplusplus)
extern "C" {
#endif

///
/// @fn: getDecryptedImageHeader
///
/// @details Decryptes the metadata of an image file into the 
///          caller's buffer.
///
/// @param[in]
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
AppImageHeaderStruct* getDecryptedImageHeader(char* imageFilename,
                                              char* keyFilename,
                                              uint8_t* headerBuf,
                                              uint32_t headerLen);

/*!
** FUNCTION: encryptWithPublicKey
**
** DESCRIPTION: Given a pointer to some data and its length, and the name
**              of any RSA public key (.pem file), this will encrypt that data.
**
**              If the "shouldSave" parameter is true, the function will
**              save the encrypted contents to a file of that name.  The
**              return value will be the address of the data originally passed
**              in.
**
**              If "shouldSave" is false, the data is not save to a file and
**              the address of the encrypted content buffer is returned. In
**              that case, THE CALLER MUST FREE THE RESULT BUFFER WHEN THEY
**              FINISH USING IT!
**
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
uint8_t * encryptWithPublicKey(const char *pubkey_filename,
                               void *valueToEncrypt,
                               uint32_t encryptLength,
                               bool shouldSave,
                               const char *output_filename);






#define ENCRYPT_CHALLENGE(challenge, pubkey_filename)  encryptWithPublicKey(pubkey_filename, challenge, 4, true, DEFAULT_ENCRYPTED_CHALLENGE_FILENAME)
#define DELETE_ENCRYPTED_CHALLENGE()            (remove((const char *)DEFAULT_ENCRYPTED_CHALLENGE_FILENAME))

#if defined(__cplusplus)
}
#endif
