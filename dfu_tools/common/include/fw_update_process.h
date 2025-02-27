//#############################################################################
//#############################################################################
//#############################################################################
//
/// @file fw_update_process.h
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
#include "dfu_client.h"
#include "dfu_client_api.h"


#define MAX_PATH_LEN                (512)

#if defined(__cplusplus)
extern "C" {
#endif


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
                                                 char* manifestPath);

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
                                           char* challengeKeyFilename);

#if defined(__cplusplus)
}
#endif
