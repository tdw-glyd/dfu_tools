//#############################################################################
//#############################################################################
//#############################################################################
//
/// @file filename.h
/// @brief
/// @copyright 2024,2025 Glydways, Inc
/// @copyright https://glydways.com
//
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include "dfu_proto_api.h"

#define MAX_DEVLICE_LISTS           (4)
#define MAX_DEVICE_LIST_LEN         (32)
#define MAX_MAC_LEN                 (32)



#define MAX_DEVLICE_LISTS           (4)

/*
** Defines the structure of what each device
** we discovery looks like.
**
*/
#define DEV_LIST_ITEM_SIGNATURE     (0xA0148BE7)
typedef struct
{
    uint32_t            signature;
    dfuDeviceTypeEnum   deviceType;
    char                devMAC[MAX_MAC_LEN];
    uint8_t             deviceVariant;
    uint8_t             blVersionMajor;
    uint8_t             blVersionMinor;
    uint8_t             blVersionRevision;
    uint8_t             statusBits;
    uint8_t             coreImageMask;
    uint8_t             blank;
    time_t              timeStamp; // as each update is received
}devListItemStruct;


/*
** A pointer to one of these will be returned from
** "devlistInit()".  If none are available,
** the return will be NULL.
**
*/
typedef struct deviceList deviceList;



#if defined(__cplusplus)
extern "C" {
#endif

///
/// @fn: devlistInitLists
///
/// @details Initializez ALL the lists.
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
void devlistInitLists(void);

///
/// @fn: devlistReserve
///
/// @details Returns an opaque pointer to the first
///          list that is not being used.
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
deviceList* devlistReserve(void);

///
/// @fn: devlistRelease
///
/// @details Marks the list as "available"
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
bool devlistRelease(deviceList* list);

///
/// @fn: devlistAddItem
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
devListItemStruct *devlistAddItem(deviceList *list,
                                  dfuDeviceTypeEnum deviceType,
                                  uint8_t deviceVariant,
                                  char devMAC[MAX_MAC_LEN],
                                  uint8_t blVersionMajor,
                                  uint8_t blVersionMinor,
                                  uint8_t blVersionRevision,
                                  uint8_t statusBits,
                                  uint8_t coreImageMask);


///
/// @fn: devlistSearchByDeviceTypeAndVariant
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
devListItemStruct* devlistSearchByDeviceTypeAndVariant(deviceList* list,
                                                       dfuDeviceTypeEnum devType,
                                                       uint8_t devVariant);

///
/// @fn: devlistSearchByMAC
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
devListItemStruct* devlistSearchByMAC(deviceList* list, char* mac);

///
/// @fn: devlistReleaseItem
///
/// @details Mark the list item as "available"
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
bool devlistReleaseItem(deviceList* list, devListItemStruct *item);





#if defined(__cplusplus)
}
#endif
