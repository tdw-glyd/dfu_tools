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
#include "file_kvp.h"
#include "dfu_client_api.h"
#include "async_timer.h"
#include "sequence_ops.h"
#include "dfu_client.h"

/*
** LIST OF SUPPORTED FIRMWARE MANIFEST KEYS
**
*/
#define FW_MANIFEST_CREATION_DATETIME_KEY                   ("creation_date_time")
#define FW_MANIFEST_VERSION_KEY                             ("firmware_manifest_version")
#define FW_MANIFEST_DEVICE_TYPE_NAME_KEY                    ("device_type_name")
#define FW_MANIFEST_DEVICE_TYPE_ID_KEY                      ("device_type_id")
#define FW_MANIFEST_DEVICE_VARIANT_ID_KEY                   ("device_variant_id")
#define FW_MANIFEST_TARGET_MCU_KEY                          ("target_mcu")
#define FW_MANIFEST_SYSTEM_VERSION_KEY                      ("system_version")
#define FW_MANIFEST_CORE_IMAGE_COUNT_KEY                    ("core_image_count")
#define FW_MANIFEST_CHALLENGE_KEY_PATH_KEY                  ("challenge_key_path")

//
// Format strings for fetching image parameters. These build the keys
// needed to reference the desired KVP in the FW manifest.
//
#define FW_MANIFEST_IMAGE_FILENAME_FORMAT                   "image_%d_filename"
#define FW_MANIFEST_IMAGE_ADDRESS_FORMAT                    "image_%d_flash_address"
#define FW_MANIFEST_IMAGE_INDEX_FORMAT                      "image_%d_index"

/*
** Local prototypes
**
*/
static fkvpStruct* openManifest(fkvpStruct* fkvp, char* manifestPath);
static bool closeManifest(fkvpStruct *fkvp);
static char* getManifestValue(fkvpStruct* fkvp, char* keyname);
static char* getManifestCoreImageFilename(fkvpStruct *fkvp, uint32_t index);
static uint32_t getManifestCoreImageFlashAddress(fkvpStruct *fkvp, uint32_t index);
static uint8_t getManifestCoreImageIndex(fkvpStruct* fkvp, uint32_t index);

/*
** Macros to access manifest values (using constant Key names above)
**
*/
#define FWMAN_CREATION_DATETIME(fkvp)   getManifestValue(fkvp, FW_MANIFEST_CREATION_DATETIME_KEY)
#define FWMAN_MANIFEST_VERSION(fkvp)    getManifestValue(fkvp, FW_MANIFEST_VERSION_KEY)
#define FWMAN_DEV_NAME(fkvp)            getManifestValue(fkvp, FW_MANIFEST_DEVICE_TYPE_NAME_KEY)
#define FWMAN_DEV_TYPE(fkvp)            (dfuDeviceTypeEnum)strtoul(getManifestValue(fkvp, FW_MANIFEST_DEVICE_TYPE_ID_KEY), NULL, 10)
#define FWMAN_DEV_VARIANT(fkvp)         (uint8_t)strtoul(getManifestValue(fkvp, FW_MANIFEST_DEVICE_VARIANT_ID_KEY), NULL, 10)
#define FWMAN_TARGET_MCU(fkvp)          getManifestValue(fkvp, FW_MANIFEST_TARGET_MCU_KEY)
#define FWMAN_SYSTEM_VERSION(fkvp)      getManifestValue(fkvp, FW_MANIFEST_SYSTEM_VERSION_KEY)
#define FWMAN_IMAGE_COUNT(fkvp)         (uint8_t)strtoul(getManifestValue(fkvp, FW_MANIFEST_CORE_IMAGE_COUNT_KEY), NULL, 10)
#define FWMAN_KEY_PATH(fkvp)            getManifestValue(fkvp, FW_MANIFEST_CHALLENGE_KEY_PATH_KEY)
#define FWMAN_IMAGE_FILENAME(fkvp, x)   getManifestCoreImageFilename(fkvp, x)
#define FWMAN_IMAGE_ADDRESS(fkvp, x)    getManifestCoreImageFlashAddress(fkvp, x)
#define FWMAN_IMAGE_INDEX(fkvp, x)      getManifestCoreImageIndex(fkvp, x)

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
uint8_t fwupdProcessFWManifestForDevice(dfuClientEnvStruct* dfuClient,
                                        dfuDeviceTypeEnum deviceType,
                                        uint8_t deviceVariant,
                                        uint8_t* deviceMAC,
                                        uint8_t deviceMACLen,
                                        char* manifestPath)
{
    uint8_t                     ret = 0;

    if (
           (dfuClient) &&
           (manifestPath) &&
           (strlen(manifestPath) > 0) &&
           (deviceMAC) &&
           (deviceMACLen > 0)
       )
    {
        fkvpStruct              fkvp;

        if (openManifest(&fkvp, manifestPath) != NULL)
        {
            // Get the fixec parameters we need from the manifest
            dfuDeviceTypeEnum   devType = FWMAN_DEV_TYPE(&fkvp);
            uint8_t             devVariant = FWMAN_DEV_VARIANT(&fkvp);
            uint8_t             imageCount = FWMAN_IMAGE_COUNT(&fkvp);
            char*               challengeKeyPath = FWMAN_KEY_PATH(&fkvp);

            if (
                   (deviceType == devType) &&
                   (deviceVariant == devVariant) &&
                   (challengeKeyPath)
               )
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
                                         deviceType,
                                         deviceVariant,
                                         dest,
                                         challengeKeyPath))
                {
                    //
                    // Transfer each image to the target,
                    // using the MAC address provided.
                    //
                    for (int index = 0; index < imageCount; index++)
                    {
                        char*           imageFilename = FWMAN_IMAGE_FILENAME(&fkvp, index);
                        uint32_t        imageAddress = FWMAN_IMAGE_ADDRESS(&fkvp, index);
                        uint8_t         imageIndex = FWMAN_IMAGE_INDEX(&fkvp, index);

                        if (
                               (imageFilename != NULL) &&
                               (imageIndex < 255) 
                           )
                        {
                            if (sequenceTransferAndInstallImage(dfuClient,
                                                                imageFilename,
                                                                imageIndex,
                                                                imageAddress,
                                                                dest))                                     
                            {
                                // Result is GOOD!
                            }
                        }
                    }

                    // Now close the Session
                    sequenceEndSession(dfuClient, dest);
                }
            }

            // Clean up
            closeManifest(&fkvp);
        }
    }

    return ret;
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                         INTERNAL SUPPORT FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

///
/// @fn: openManifest
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
static fkvpStruct* openManifest(fkvpStruct* fkvp, char* manifestPath)
{
    fkvpStruct*             ret = NULL;

    if (
           (fkvp) &&
           (manifestPath) &&
           (strlen(manifestPath))
       )
    {
        ret = fkvpBegin(manifestPath, fkvp);
    }

    return ret;
}

///
/// @fn: closeManifest
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
static bool closeManifest(fkvpStruct *fkvp)
{
    return fkvpEnd(fkvp);
}

///
/// @fn: getManifestValue
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
static char* getManifestValue(fkvpStruct* fkvp, char *keyname)
{
    char*               ret = NULL;

    if (
           (fkvp) &&
           (keyname) &&
           (strlen(keyname) > 0)
       )
    {
        ret = fkvpFind(fkvp, keyname, true);
    }

    return ret;
}

///
/// @fn: getManifestCoreImageFilename
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
static char* getManifestCoreImageFilename(fkvpStruct *fkvp, uint32_t index)
{
    char*               ret = NULL;

    if (fkvp)
    {
        char        buffer[64];

        snprintf(buffer, sizeof(buffer), FW_MANIFEST_IMAGE_FILENAME_FORMAT, index);
        ret = getManifestValue(fkvp, buffer);
    }

    return ret;
}

///
/// @fn: getManifestCoreImageIndexFlashAddress
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
static uint32_t getManifestCoreImageFlashAddress(fkvpStruct *fkvp, uint32_t index)
{
    uint32_t                ret = 0;

    if (fkvp)
    {
        char        buffer[64];
        char*       valStr;

        snprintf(buffer, sizeof(buffer), FW_MANIFEST_IMAGE_ADDRESS_FORMAT, index);
        valStr = getManifestValue(fkvp, buffer);
        if (valStr != NULL)
        {
            ret = strtoul(valStr, NULL, 16);
        }
    }

    return ret;
}

///
/// @fn: getManifestCoreImageIndex
///
/// @details Retrieve the core image index value from the manifest,
///          based on the "index" parameter
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
static uint8_t getManifestCoreImageIndex(fkvpStruct* fkvp, uint32_t index)
{
    uint8_t                     ret = 255;

    if (fkvp)
    {
        char        buffer[64];
        char*       valStr;

        snprintf(buffer, sizeof(buffer), FW_MANIFEST_IMAGE_INDEX_FORMAT, index);
        valStr = getManifestValue(fkvp, buffer);
        if (valStr != NULL)
        {
            ret = strtoul(valStr, NULL, 16);
        }        
    }

    return ret;
}

