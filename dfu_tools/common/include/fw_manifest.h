//#############################################################################
//#############################################################################
//#############################################################################
//
/// @file fw_manifest.h
/// @brief
///
/// @details
///
/// @copyright 2024,2025 Glydways, Inc
/// @copyright https://glydways.com
//
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "file_kvp.h"
#include "general_utils.h"


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


#if defined(__cplusplus)
extern "C" {
#endif

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
/// @tracereq(@req{xxxxxxx}}
///
fkvpStruct* openFWManifest(fkvpStruct* fkvp, char* manifestPath);

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
/// @tracereq(@req{xxxxxxx}}
///
bool closeFWManifest(fkvpStruct *fkvp);

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
/// @tracereq(@req{xxxxxxx}}
///
char* getFWManifestValue(fkvpStruct* fkvp, char *keyname);

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
/// @tracereq(@req{xxxxxxx}}
///
char* getFWManifestCoreImageFilename(fkvpStruct *fkvp, uint32_t index);

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
/// @tracereq(@req{xxxxxxx}}
///
uint32_t getFWManifestCoreImageFlashAddress(fkvpStruct *fkvp, uint32_t index);

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
/// @tracereq(@req{xxxxxxx}}
///
uint8_t getFWManifestCoreImageIndex(fkvpStruct* fkvp, uint32_t index);


/*
** Macros to access manifest values (using constant Key names above)
**
*/
#define FWMAN_CREATION_DATETIME(fkvp)   dfuToolStripQuotes(getFWManifestValue(fkvp, FW_MANIFEST_CREATION_DATETIME_KEY))
#define FWMAN_MANIFEST_VERSION(fkvp)    dfuToolStripQuotes(getFWManifestValue(fkvp, FW_MANIFEST_VERSION_KEY))
#define FWMAN_DEV_NAME(fkvp)            dfuToolStripQuotes(getFWManifestValue(fkvp, FW_MANIFEST_DEVICE_TYPE_NAME_KEY))
#define FWMAN_DEV_TYPE(fkvp)            (dfuDeviceTypeEnum)strtoul(getFWManifestValue(fkvp, FW_MANIFEST_DEVICE_TYPE_ID_KEY), NULL, 10)
#define FWMAN_DEV_VARIANT(fkvp)         (uint8_t)strtoul(getFWManifestValue(fkvp, FW_MANIFEST_DEVICE_VARIANT_ID_KEY), NULL, 10)
#define FWMAN_TARGET_MCU(fkvp)          dfuToolStripQuotes(getFWManifestValue(fkvp, FW_MANIFEST_TARGET_MCU_KEY))
#define FWMAN_SYSTEM_VERSION(fkvp)      getFWManifestValue(fkvp, FW_MANIFEST_SYSTEM_VERSION_KEY)
#define FWMAN_IMAGE_COUNT(fkvp)         (uint8_t)strtoul(getFWManifestValue(fkvp, FW_MANIFEST_CORE_IMAGE_COUNT_KEY), NULL, 10)
#define FWMAN_KEY_PATH(fkvp)            dfuToolStripQuotes(getFWManifestValue(fkvp, FW_MANIFEST_CHALLENGE_KEY_PATH_KEY))
#define FWMAN_IMAGE_FILENAME(fkvp, x)   dfuToolStripQuotes(getFWManifestCoreImageFilename(fkvp, x))
#define FWMAN_IMAGE_ADDRESS(fkvp, x)    getFWManifestCoreImageFlashAddress(fkvp, x)
#define FWMAN_IMAGE_INDEX(fkvp, x)      getFWManifestCoreImageIndex(fkvp, x)

#if defined(__cplusplus)
}
#endif
