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

/*
** If "1", we sign the challenge password from the device. If
** "0", we encrypt it.
**
*/
#define CHALLENGE_SIGNED                                    (1)
#define CHALLENGE_ENCRYPTED                                 (2)

// Which way to handle the challenge?
#define CHALLENGE_HANDLING                                  CHALLENGE_SIGNED


#define DEFAULT_ENCRYPTED_CHALLENGE_FILENAME            ("./encrypted_chal.bin")
#define DEFAULT_SIGNED_CHALLENGE_FILENAME               ("./signed_chal.bin")

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


///
/// @fn: signChallengeWithPrivateKey
///
/// @details
///
/// @param[in] privateKeyFile: Path to the PEM-encoded private key file
/// @param[in] challenge: The 32-bit challenge value to sign
/// @param[in] saveToFile: Boolean flag indicating whether to save the signature to a file
/// @param[in] outputFile: If saveToFile is 1, the path to save the signature to
///
/// @returns If saveToFile is 0, returns pointer to the signature buffer.
///          If saveToFile is 1, returns NULL after saving signature to file.
///         Returns NULL on any failure.
///
/// @tracereq(@req{xxxxxxx}}
///
uint8_t* signChallengeWithPrivateKey(const char* privateKeyFile,
                                     uint32_t* challenge,
                                     bool saveToFile,
                                     const char* outputFile);


#define SIGN_CHALLENGE(challenge, key_filename) signChallengeWithPrivateKey(key_filename, challenge, true, DEFAULT_SIGNED_CHALLENGE_FILENAME);
#define DELETE_SIGNED_CHALLENGE()               (remove((const char*)DEFAULT_SIGNED_CHALLENGE_FILENAME))

#define ENCRYPT_CHALLENGE(challenge, pubkey_filename)  encryptWithPublicKey(pubkey_filename, challenge, 4, true, DEFAULT_ENCRYPTED_CHALLENGE_FILENAME)
#define DELETE_ENCRYPTED_CHALLENGE()            (remove((const char *)DEFAULT_ENCRYPTED_CHALLENGE_FILENAME))


#if (CHALLENGE_HANDLING==CHALLENGE_SIGNED)
    #define HANDLE_CHALLENGE(a, b)      SIGN_CHALLENGE(a, b)
    #define DELETE_CHALLENGE            DELETE_SIGNED_CHALLENGE
    #define SIGNATURE_FILENAME          DEFAULT_SIGNED_CHALLENGE_FILENAME
#elif (CHALLENGE_HANDLING==CHALLENGE_ENCRYPTED)
    #define HANDLE_CHALLENGE(a, b)      ENCRYPT_CHALLENGE(a, b)
    #define DELETE_CHALLENGE            DELETE_ENCRYPTED_CHALLENGE
    #define SIGNATURE_FILENAME          DEFAULT_ENCRYPTED_CHALLENGE_FILENAME
#endif

#if defined(__cplusplus)
}
#endif
