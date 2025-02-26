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



#if defined(__cplusplus)
}
#endif
