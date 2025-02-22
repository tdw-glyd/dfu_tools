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


/*
** Local prototypes
**
*/
static fkvpStruct* openManifest(fkvpStruct* fkvp, char* manifestPath);
static bool closeManifest(fkvpStruct *fkvp);
static char* getManifestValue(fkvpStruct* fkvp, char* keyname);
static char* getManifestCoreImageIndexFilename(fkvpStruct *fkvp, int coreIndex);
static uint32_t getManifestCoreImageIndexFlashAddress(fkvpStruct *fkvp, int coreIndex);

/*
** Macros to access manifest values (using constant Key names above)
**
*/
#define FWMAN_CREATION_DATETIME(fkvp)   getManifestValue(fkvp, FW_MANIFEST_CREATION_DATETIME_KEY)
#define FWMAN_MANIFEST_VERSION(fkvp)    getManifestValue(fkvp, FW_MANIFEST_VERSION_KEY)
#define FWMAN_DEV_NAME(fkvp)            getManifestValue(fkvp, FW_MANIFEST_DEVICE_TYPE_NAME_KEY)
#define FWMAN_DEV_TYPE(fkvp)            (uint8_t)strtoul(getManifestValue(fkvp, FW_MANIFEST_DEVICE_TYPE_ID_KEY), NULL, 10)
#define FWMAN_DEV_VARIANT(fkvp)         (uint8_t)strtoul(getManifestValue(fkvp, FW_MANIFEST_DEVICE_VARIANT_ID_KEY), NULL, 10)
#define FWMAN_TARGET_MCU(fkvp)          getManifestValue(fkvp, FW_MANIFEST_TARGET_MCU_KEY)
#define FWMAN_SYSTEM_VERSION(fkvp)      getManifestValue(fkvp, FW_MANIFEST_SYSTEM_VERSION_KEY)
#define FWMAN_IMAGE_COUNT(fkvp)         (uint8_t)strtoul(getManifestValue(fkvp, FW_MANIFEST_CORE_IMAGE_COUNT_KEY), NULL, 10)
#define FWMAN_KEY_PATH(fkvp)            getManifestValue(fkvp, FW_MANIFEST_CHALLENGE_KEY_PATH_KEY)
#define FWMAN_IMAGE_FILENAME(fkvp, x)   getManifestCoreImageIndexFilename(fkvp, x)
#define FWMAN_IMAGE_ADDRESS(fkvp, x)    getManifestCoreImageIndexFlashAddress(fkvp, x)

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                            PUBLIC API FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

uint8_t fwupdProcessManifest(char* manifestPath)
{
    uint8_t                     ret = 0;

    if (
           (manifestPath) &&
           (strlen(manifestPath) > 0)
       )
    {
        fkvpStruct              fkvp;

        if (openManifest(&fkvp, manifestPath) != NULL)
        {
            uint8_t         devType = FWMAN_DEV_TYPE(&fkvp);
            uint8_t         devVariant = FWMAN_DEV_VARIANT(&fkvp);
            uint8_t         imageCount = FWMAN_IMAGE_COUNT(&fkvp);

            for (int index = 0; index < imageCount; index++)
            {
                char*           imageFilename = FWMAN_IMAGE_FILENAME(&fkvp, index);
                uint32_t        imageAddress = FWMAN_IMAGE_ADDRESS(&fkvp, index);

                // UNFINISHED!
                
            }


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
/// @fn: getManifestCoreImageIndexFilename
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
static char* getManifestCoreImageIndexFilename(fkvpStruct *fkvp, int coreIndex)
{
    char*               ret = NULL;

    if (fkvp)
    {
        char        buffer[64];

        snprintf(buffer, sizeof(buffer), "image_index_%d_filename", coreIndex);
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
static uint32_t getManifestCoreImageIndexFlashAddress(fkvpStruct *fkvp, int coreIndex)
{
    uint32_t                ret = 0;

    if (fkvp)
    {
        char        buffer[64];
        char*       valStr;

        snprintf(buffer, sizeof(buffer), "image_index_%d_flash_address", coreIndex);
        valStr = getManifestValue(fkvp, buffer);
        if (valStr != NULL)
        {
            ret = strtoul(valStr, NULL, 16);
        }
    }

    return ret;
}
