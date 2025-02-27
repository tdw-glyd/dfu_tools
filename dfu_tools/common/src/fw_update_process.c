//#############################################################################
//#############################################################################
//#############################################################################
//
/// @file fw_update_process.c
/// @brief Handles a full firmware update sequence, using the manifest to
//         drive the whole sequence.
///
/// @details
///
/// @copyright 2024 Glydways, Inc
/// @copyright https://glydways.com
//
//#############################################################################
//#############################################################################
//#############################################################################
#include "fw_update_process.h"
#include "async_timer.h"
#include "sequence_ops.h"
#include "fw_manifest.h"
#include "dfu_client_api.h"


// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                            PUBLIC API FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

///
/// @fn: fwupdProcessFWManifestForDevice
///
/// @details
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
apiErrorCodeEnum fwupdProcessFWManifestForDevice(dfuClientEnvStruct* dfuClient,
                                                 uint8_t* deviceMAC,
                                                 uint8_t deviceMACLen,
                                                 char* manifestPath)
{
    apiErrorCodeEnum                     ret = API_ERR_UNKNOWN;

    if (
           (dfuClient) &&
           (manifestPath) &&
           (strlen(manifestPath) > 0) &&
           (deviceMAC) &&
           (deviceMACLen > 0)
       )
    {
        fkvpStruct              fkvp;

        if (openFWManifest(&fkvp, manifestPath) != NULL)
        {
            // Get the fixec parameters we need from the manifest
            dfuDeviceTypeEnum   devType = FWMAN_DEV_TYPE(&fkvp);
            uint8_t             devVariant = FWMAN_DEV_VARIANT(&fkvp);
            uint8_t             imageCount = FWMAN_IMAGE_COUNT(&fkvp);
            char                textBuf[MAX_PATH_LEN];

            // Build the path to the challenge key file
            snprintf(textBuf, sizeof(textBuf), "%s", manifestPath);
            dfuToolExtractPath(textBuf);
            strcat(textBuf, FWMAN_KEY_PATH(&fkvp));

            if (strlen(textBuf))
            {
                char                dest[24];

                dfuClientMacBytesToString(dfuClient,
                                          deviceMAC,
                                          deviceMACLen,
                                          dest,
                                          24);

                //
                // Establish a Session. If that succeeds,
                // begin updating the firmware.
                //
                if (sequenceBeginSession(dfuClient,
                                         devType,
                                         devVariant,
                                         dest,
                                         textBuf))
                {
                    //
                    // Transfer each image to the target,
                    // using the MAC address provided.
                    //
                    // !!! ALL IMAGE ID'S AND INDICES START AT 1 !!!
                    //
                    for (int index = 1; index <= imageCount; index++)
                    {
                        uint32_t        imageAddress = FWMAN_IMAGE_ADDRESS(&fkvp, index);
                        uint8_t         imageIndex = FWMAN_IMAGE_INDEX(&fkvp, index);

                        // Build the path to the manifest
                        snprintf(textBuf,
                                 sizeof(textBuf),
                                 "%s",
                                 manifestPath);
                        dfuToolExtractPath(textBuf);
                        strcat(textBuf, FWMAN_IMAGE_FILENAME(&fkvp, index));

                        if (
                               (strlen(textBuf) > 0) &&
                               (imageIndex < 255)
                           )
                        {
                            if (sequenceTransferAndInstallImage(dfuClient,
                                                                textBuf,
                                                                imageIndex,
                                                                imageAddress,
                                                                dest))
                            {
                                // Result is GOOD!
                                ret = API_ERR_NONE;
                            }
                            else
                            {
                                ret = API_ERR_IMAGE_INSTALLATION_FAILED;
                            }
                        }
                    }

                    // Now close the Session
                    sequenceEndSession(dfuClient, dest);
                }
                else
                {
                    ret = API_ERR_SESSION_START_REJECTED;
                }
            }

            // Clean up
            closeFWManifest(&fkvp);
        }
        else
        {
            ret = API_ERR_FW_MANIFEST;
        }
    }

    return ret;
}

///
/// @fn: fwupdInstallCoreImageFile
///
/// @details Given a device MAC, the core image file, decryption key
///          and challenge key, this will transfer the image file
///          to the target.
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
apiErrorCodeEnum fwupdInstallCoreImageFile(dfuClientEnvStruct* dfuClient,
                                           dfuDeviceTypeEnum deviceType,
                                           uint8_t deviceVariant,
                                           uint8_t imageIndex,
                                           uint32_t flashBaseAddress,
                                           uint8_t* mac,
                                           uint8_t macLen,
                                           char* imageFilename,
                                           char* challengeKeyFilename)
{
    apiErrorCodeEnum                ret = API_ERR_UNKNOWN;

    if (
           (dfuClient) &&
           (mac) &&
           (macLen > 0) &&
           (imageFilename) &&
           (challengeKeyFilename)
       )
    {
        // Verify the files exist
        if (
               (dfuToolGetFileSize(imageFilename) > 0) &&
               (dfuToolGetFileSize(challengeKeyFilename) > 0)
           )
        {
            char                            dest[24];

            // Convert the MAC
            dfuClientMacBytesToString(dfuClient,
                                      mac,
                                      macLen,
                                      dest,
                                      24);
            //
            // Now we know the device TYPE and VARIANT, due
            // to having the metadata for its image file.
            //
            // Establish a Session. If that succeeds,
            // begin updating the firmware.
            //
            if (sequenceBeginSession(dfuClient,
                                     (dfuDeviceTypeEnum)deviceType,
                                     (uint8_t)deviceVariant,
                                     dest,
                                     challengeKeyFilename))
            {

                if (sequenceTransferAndInstallImage(dfuClient,
                                                    imageFilename,
                                                    imageIndex,
                                                    flashBaseAddress,
                                                    dest))
                {
                    // Result is GOOD!
                    ret = API_ERR_NONE;
                }
                else
                {
                    ret = API_ERR_IMAGE_INSTALLATION_FAILED;
                }

                // Now close the Session
                sequenceEndSession(dfuClient, dest);
            }
            else
            {
                ret = API_ERR_SESSION_START_REJECTED;
            }
        }
        else
        {
            ret = API_ERR_MISSING_FILE;
        }
    }
    else
    {
        ret = API_ERR_INVALID_PARAMS;
    }

    return ret;
}

