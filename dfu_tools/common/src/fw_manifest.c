//#############################################################################
//#############################################################################
//#############################################################################
///
/// @copyright 2024 Glydways, Inc
/// @copyright https://glydways.com
///
/// @file fw_manifest.c
/// @brief
///
/// @details
///
//#############################################################################
//#############################################################################
//#############################################################################
#include "fw_manifest.h"

//
// Format strings for fetching image parameters. These build the keys
// needed to reference the desired KVP in the FW manifest.
//
#define FW_MANIFEST_IMAGE_FILENAME_FORMAT                   "image_%d_filename"
#define FW_MANIFEST_IMAGE_ADDRESS_FORMAT                    "image_%d_flash_address"
#define FW_MANIFEST_IMAGE_INDEX_FORMAT                      "image_%d_core_index"


///
/// @fn: openFWManifest
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
fkvpStruct* openFWManifest(fkvpStruct* fkvp, char* manifestPath)
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
/// @fn: closeFWManifest
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
bool closeFWManifest(fkvpStruct *fkvp)
{
    return fkvpEnd(fkvp);
}

///
/// @fn: getFWManifestValue
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
char* getFWManifestValue(fkvpStruct* fkvp, char *keyname)
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
/// @fn: getFWManifestCoreImageFilename
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
char* getFWManifestCoreImageFilename(fkvpStruct *fkvp, uint32_t index)
{
    char*               ret = NULL;

    if (fkvp)
    {
        char        buffer[64];

        snprintf(buffer, sizeof(buffer), FW_MANIFEST_IMAGE_FILENAME_FORMAT, index);
        ret = getFWManifestValue(fkvp, buffer);
    }

    return ret;
}

///
/// @fn: getFWManifestCoreImageIndexFlashAddress
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
uint32_t getFWManifestCoreImageFlashAddress(fkvpStruct *fkvp, uint32_t index)
{
    uint32_t                ret = 0;

    if (fkvp)
    {
        char        buffer[64];
        char*       valStr;

        snprintf(buffer, sizeof(buffer), FW_MANIFEST_IMAGE_ADDRESS_FORMAT, index);
        valStr = getFWManifestValue(fkvp, buffer);
        if (valStr != NULL)
        {
            ret = strtoul(valStr, NULL, 16);
        }
    }

    return ret;
}

///
/// @fn: getFWManifestCoreImageIndex
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
uint8_t getFWManifestCoreImageIndex(fkvpStruct* fkvp, uint32_t index)
{
    uint8_t                     ret = 255;

    if (fkvp)
    {
        char        buffer[64];
        char*       valStr;

        snprintf(buffer, sizeof(buffer), FW_MANIFEST_IMAGE_INDEX_FORMAT, index);
        valStr = getFWManifestValue(fkvp, buffer);
        if (valStr != NULL)
        {
            ret = strtoul(valStr, NULL, 16);
        }
    }

    return ret;
}

