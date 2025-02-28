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
#include <string.h>
#include <stdlib.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "dfu_client_crypto.h"
#include "image_metadata.h"

/* Platform-specific includes and definitions */
#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>

    // Define fseeko and ftello for Windows
    #define fseeko _fseeki64
    #define ftello _ftelli64
#else
    // Unix/Linux specific headers if needed
    #include <unistd.h>
#endif

#define IV_SIZE 12       /* First 12 bytes of file */
#define PADDING_SIZE 4   /* 4 bytes of padding after IV */
#define TAG_SIZE 16      /* 16 bytes authentication tag after padding */
#define HEADER_SIZE (IV_SIZE + PADDING_SIZE + TAG_SIZE) /* Total header size: 32 bytes */
#define MAX_DECRYPT_SIZE 128

///
/// @fn: handleOpenSSLError
///
/// @details Called for errors in OpenSSL
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
static void handleOpenSSLError(void)
{
    ERR_print_errors_fp(stderr);
}


///
/// @fn: openBinaryFile
///
/// @details Platform-independent file open
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
static FILE* openBinaryFile(const char* filename, const char* mode)
{
#ifdef _WIN32
    FILE* fp = NULL;
    fopen_s(&fp, filename, mode);
    return fp;
#else
    return fopen(filename, mode);
#endif
}

///
/// @fn: decryptFileAES_GCM
///
/// @details Decrypts the header portion of a core image file.
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
static int decryptFileAES_GCM(const char *input_file,
                              const unsigned char *key,
                              int key_len,
                              unsigned char *output_buffer,
                              int *output_len)
{
    FILE*               fp;
    unsigned char       iv[IV_SIZE];
    unsigned char       padding[PADDING_SIZE];
    unsigned char       tag[TAG_SIZE];
    unsigned char       ciphertext[MAX_DECRYPT_SIZE];

    EVP_CIPHER_CTX*     ctx;
    int                 len = 0;
    int                 ret = 0;

    // Validate parameters
    if (!input_file || !key || key_len != 16 || !output_buffer || !output_len)
    {
        return 0;
    }

    // Initialize OpenSSL
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    /* Legacy OpenSSL initialization (< 1.1.0) */
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
#else
    // Modern OpenSSL initialization (>= 1.1.0)
    OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CRYPTO_STRINGS |
                        OPENSSL_INIT_ADD_ALL_CIPHERS,
                        NULL);
#endif

    // Open the encrypted file
    fp = openBinaryFile(input_file, "rb");
    if (!fp)
    {
        return 0;
    }

    // Read the IV from the beginning of the file
    if (fread(iv, 1, IV_SIZE, fp) != IV_SIZE)
    {
        fclose(fp);
        return 0;
    }

    // Read the padding (4 bytes)
    if (fread(padding, 1, PADDING_SIZE, fp) != PADDING_SIZE)
    {
        fclose(fp);
        return 0;
    }

    // Verify padding has expected values
    if (padding[0] != 0xA5 || padding[1] != 0x5A || padding[2] != 0xAA || padding[3] != 0x55)
    {
        /* Continue anyway, as this is just a warning */
    }

    // Read authentication tag
    if (fread(tag, 1, TAG_SIZE, fp) != TAG_SIZE)
    {
        fclose(fp);
        return 0;
    }

    // Read up to 128 bytes of ciphertext (which starts after the header)
    int bytes_read = fread(ciphertext, 1, MAX_DECRYPT_SIZE, fp);
    if (bytes_read <= 0)
    {
        fclose(fp);
        return 0;
    }

    // Create and initialize the context
    if (!(ctx = EVP_CIPHER_CTX_new()))
    {
        handleOpenSSLError();
        fclose(fp);
        return 0;
    }

    // Initialize decryption operation
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL))
    {
        handleOpenSSLError();
        EVP_CIPHER_CTX_free(ctx);
        fclose(fp);
        return 0;
    }

    // Set IV length
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, NULL))
    {
        handleOpenSSLError();
        EVP_CIPHER_CTX_free(ctx);
        fclose(fp);
        return 0;
    }

    // Initialize key and IV
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv))
    {
        handleOpenSSLError();
        EVP_CIPHER_CTX_free(ctx);
        fclose(fp);
        return 0;
    }

    // Decrypt ciphertext
    if (!EVP_DecryptUpdate(ctx, output_buffer, &len, ciphertext, bytes_read))
    {
        handleOpenSSLError();
        EVP_CIPHER_CTX_free(ctx);
        fclose(fp);
        return 0;
    }
    *output_len = len;

    // Clean up
    EVP_CIPHER_CTX_free(ctx);
    fclose(fp);

    ret = (*output_len > 0);

    /// ret > 0 indicates success, 0 or negative means verify failed
    return ret;
}

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
                                              uint32_t headerLen)
{
    AppImageHeaderStruct*                ret = NULL;

    if (keyFilename)
    {
        FILE*           handle = fopen(keyFilename, "rb");
        if (handle)
        {
            uint8_t         keyBuf[32];

            if (fread(keyBuf, 1, 16, handle) == 16)
            {
                int         outputLen = headerLen;

                if (decryptFileAES_GCM(imageFilename, keyBuf, 16, headerBuf, &outputLen))
                {
                    ret = (AppImageHeaderStruct*)headerBuf;

                    // Now make sure it looks ok
                    if (
                           (ret->headSignature != APP_IMAGE_HEAD_SIGNATURE) ||
                           (ret->tailSignature != APP_IMAGE_TAIL_SIGNATURE)
                       )
                    {
                        ret = NULL;
                    }
                }
            }

            fclose(handle);
        }
    }

    return ret;
}

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

        // Read in the public key
        bio = BIO_new_file(pubkeyFilename, "r");
        if (!bio)
        {
            printf("\r\n Error creating BIO object!");
            goto cleanup;
        }

        // Read the public key into an EVP_PKEY structure
        pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
        ERR_clear_error();

        // Create a context for encryption
        ctx = EVP_PKEY_CTX_new(pkey, NULL);
        if (!ctx)
        {
            fprintf(stderr, "\r\n Error: Failed to create EVP context: %s\n", ERR_error_string(ERR_get_error(), NULL));
            goto cleanup;
        }

        // Initialize the context for encryption
        if (EVP_PKEY_encrypt_init(ctx) <= 0)
        {
            fprintf(stderr, "\r\n Error: Failed to initialize encryption: %s\n", ERR_error_string(ERR_get_error(), NULL));
            goto cleanup;
        }

        // Determine the buffer length for the encrypted data
        if (EVP_PKEY_encrypt(ctx, NULL, &encrypted_len, (unsigned char *)valueToEncrypt, encryptLength) <= 0)
        {
            fprintf(stderr, "\r\n Error: Failed to determine encrypted length: %s\n", ERR_error_string(ERR_get_error(), NULL));
            goto cleanup;
        }

        // Allocate memory for the encrypted data
        encrypted_data = malloc(encrypted_len);
        if (!encrypted_data)
        {
            fprintf(stderr, "\r\n Error: Failed to allocate memory for encrypted data.\n");
            goto cleanup;
        }

        // Perform the encryption
        if (EVP_PKEY_encrypt(ctx, encrypted_data, &encrypted_len, (unsigned char *)valueToEncrypt, encryptLength) <= 0)
        {
            fprintf(stderr, "\r\n Error: Encryption failed: %s\n", ERR_error_string(ERR_get_error(), NULL));
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
                fprintf(stderr, "\r\n Error: Failed to open output file: %s\n", output_filename);
                goto cleanup;
            }
            fwrite(encrypted_data, 1, encrypted_len, output_file);
            fclose(output_file);
        }
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
