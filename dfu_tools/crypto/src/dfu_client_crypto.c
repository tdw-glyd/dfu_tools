//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_client_crypto.c
**
** DESCRIPTION: Crypto module that uses openSSL.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "dfu_client_crypto.h"


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
uint8_t * encryptWithPublicKey(const char *pubkeyFilename,
                               void *valueToEncrypt,
                               uint32_t encryptLength,
                               bool shouldSave,
                               const char *output_filename)
{
    uint8_t                 *ret = NULL;
    EVP_PKEY                *pkey = NULL;
    EVP_PKEY_CTX            *ctx = NULL;
    BIO                     *bio = NULL;
    unsigned char           *encrypted_data = NULL;
    size_t                   encrypted_len = 0;

    if (
           (pubkeyFilename) &&
           (valueToEncrypt) &&
           (encryptLength)
       )
    {

        // Initialize openSSL
        OPENSSL_init_crypto(0, NULL);
        printf("\r\nOpenSSL Version: %s", OpenSSL_version(OPENSSL_VERSION));

        // Read in the public key
        bio = BIO_new_file(pubkeyFilename, "r");
        if (!bio)
        {
            printf("\r\nError creating BIO object!");
            goto cleanup;
        }

        // Read the public key into an EVP_PKEY structure
        pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
        ERR_clear_error();

        // Create a context for encryption
        ctx = EVP_PKEY_CTX_new(pkey, NULL);
        if (!ctx)
        {
            fprintf(stderr, "Error: Failed to create EVP context: %s\n", ERR_error_string(ERR_get_error(), NULL));
            goto cleanup;
        }

        // Initialize the context for encryption
        if (EVP_PKEY_encrypt_init(ctx) <= 0)
        {
            fprintf(stderr, "Error: Failed to initialize encryption: %s\n", ERR_error_string(ERR_get_error(), NULL));
            goto cleanup;
        }

        // Determine the buffer length for the encrypted data
        if (EVP_PKEY_encrypt(ctx, NULL, &encrypted_len, (unsigned char *)valueToEncrypt, encryptLength) <= 0)
        {
            fprintf(stderr, "Error: Failed to determine encrypted length: %s\n", ERR_error_string(ERR_get_error(), NULL));
            goto cleanup;
        }

        // Allocate memory for the encrypted data
        encrypted_data = malloc(encrypted_len);
        if (!encrypted_data)
        {
            fprintf(stderr, "Error: Failed to allocate memory for encrypted data.\n");
            goto cleanup;
        }

        // Perform the encryption
        if (EVP_PKEY_encrypt(ctx, encrypted_data, &encrypted_len, (unsigned char *)valueToEncrypt, encryptLength) <= 0)
        {
            fprintf(stderr, "Error: Encryption failed: %s\n", ERR_error_string(ERR_get_error(), NULL));
            goto cleanup;
        }

        //
        // Should we save the encrypted value?
        //
        if ( (shouldSave) && (output_filename != NULL) )
        {
            // Write the encrypted data to the output file
            FILE *output_file = fopen(output_filename, "wb");
            if (!output_file)
            {
                fprintf(stderr, "Error: Failed to open output file: %s\n", output_filename);
                goto cleanup;
            }
            fwrite(encrypted_data, 1, encrypted_len, output_file);
            fclose(output_file);
        }

        printf("\r\nEncryption successful.");

    }

cleanup:
    if (bio) BIO_free(bio);
    if (pkey) EVP_PKEY_free(pkey);
    if (ctx) EVP_PKEY_CTX_free(ctx);
    if (encrypted_data)
    {
        if (shouldSave)
        {
            free(encrypted_data);
            printf(" Encrypted data written to: %s\n", output_filename);
            ret = (uint8_t *)valueToEncrypt;
        }
        else
        {
            // In this case, the client must free this buffer!
            ret = encrypted_data;
        }
    }

    EVP_cleanup();
    ERR_free_strings();


    return(ret);
}
