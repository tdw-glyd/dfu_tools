//#############################################################################
//#############################################################################
//#############################################################################
//
/// @file dev_list.c
/// @brief Routines to manage device lists (data gotten from receipt of
///        DEVICE_STATUS messages)
/// @copyright 2024 Glydways, Inc
/// @copyright https://glydways.com
//
//#############################################################################
//#############################################################################
//#############################################################################
#include <string.h>
#include "dev_list.h"


/*
** Defines the structure of a single list of "devListItemStruct"s
**
*/
#define DEV_LIST_SIGNATURE          (0x11048AE0)
struct deviceList
{
    uint32_t           signature;
    devListItemStruct  devList[MAX_DEVICE_LIST_LEN+1];
};

/*
** Holds the list of lists.  Each item in this
** list holds a list of devices.
**
*/
static deviceList mDevList[MAX_DEVLICE_LISTS];

/*
** Helpful macros
**
*/
#define VALID_DEVICE_LIST(l)        ( ((deviceList*)l)->signature == DEV_LIST_SIGNATURE)
#define VALID_LIST_ITEM(i)          ( ((devListItemStruct*)i)->signature == DEV_LIST_ITEM_SIGNATURE)


/*
** Internal prototypes
**
*/
static devListItemStruct* devlistFindFirstFreeItem(deviceList* list);


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
void devlistInitLists(void)
{
    for (int i = 0; i < MAX_DEVLICE_LISTS; i++)
    {
        memset(&mDevList[i], 0x00, sizeof(deviceList));
    }

    return;
}

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
deviceList* devlistReserve(void)
{
    deviceList*         ret = NULL;

    for (int i = 0; i < MAX_DEVLICE_LISTS; i++)
    {
        if (VALID_DEVICE_LIST(&mDevList[i]))
        {
            memset(&mDevList[i], 0x00, sizeof(mDevList[i].devList));
            mDevList[i].signature = DEV_LIST_SIGNATURE;
            ret = &mDevList[i];
            break;
        }
    }

    return ret;
}

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
bool devlistRelease(deviceList* list)
{
    bool                ret = false;

    if (
           (list) &&
           (list->signature == DEV_LIST_SIGNATURE)
       )
    {
        list->signature = 0x00;
        ret = true;
    }

    return ret;
}

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
                                  uint8_t coreImageMask)
{
    devListItemStruct*              ret = NULL;

    if (VALID_DEVICE_LIST(list))
    {
        /*
        ** If there's already a match for the device,
        ** just update it.  Otherwise, add a new item
        **
        */
        ret = devlistSearchByDeviceTypeAndVariant(list,
                                                  deviceType,
                                                  deviceVariant);

        if (ret == NULL)
        {
            ret = devlistFindFirstFreeItem(list);
        }

        if (
               (ret) &&
               (VALID_LIST_ITEM(ret))
           )
        {
            ret->blVersionMajor = blVersionMajor;
            ret->blVersionMinor = blVersionMinor;
            ret->blVersionRevision = blVersionRevision;
            ret->statusBits = statusBits;
            ret->coreImageMask = coreImageMask;
            memcpy(ret->devMAC, devMAC, MAX_MAC_LEN);
        }
    }

    return ret;
}

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
                                                       uint8_t devVariant)
{
    devListItemStruct*                  ret = NULL;

    if (VALID_DEVICE_LIST(list))
    {
        for (int i = 0; i < MAX_DEVICE_LIST_LEN; i++)
        {
            if (
                   (list->devList[i].signature == DEV_LIST_ITEM_SIGNATURE) &&
                   (list->devList[i].deviceType == devType) &&
                   (list->devList[i].deviceVariant == devVariant)
               )
            {
                ret = &list->devList[i];
                break;
            }
        }
    }

    return ret;
}

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
devListItemStruct* devlistSearchByMAC(deviceList* list, char* mac)
{
    devListItemStruct*          ret = NULL;

    if (
           (VALID_DEVICE_LIST(list)) &&
           (mac != NULL) &&
           (strlen(mac) < MAX_MAC_LEN)
       )
    {
        for (int i = 0; i < MAX_DEVICE_LIST_LEN; i++)
        {
            if (
                   (list->devList[i].signature == DEV_LIST_ITEM_SIGNATURE) &&
                   (strncmp(mac, list->devList[i].devMAC, MAX_MAC_LEN)==0)
               )
            {
                ret = &list->devList[i];
                break;
            }
        }
    }

    return ret;
}

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
bool devlistReleaseItem(deviceList* list, devListItemStruct *item)
{
    bool            ret = false;

    if (
           (VALID_DEVICE_LIST(list)) &&
           (VALID_LIST_ITEM(item))
       )
    {
        item->signature = 0;
        ret = true;
    }

    return ret;
}

///
/// @fn: devlistFindFirstFreeItem
///
/// @details Searches the given list for a free element. If
///          found, marks it as "in use"
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
static devListItemStruct* devlistFindFirstFreeItem(deviceList* list)
{
    devListItemStruct*          ret = NULL;

    if (VALID_DEVICE_LIST(list))
    {
        for (int i = 0; i < MAX_DEVICE_LIST_LEN; i++)
        {
            if (list->devList[i].signature != DEV_LIST_ITEM_SIGNATURE)
            {
                ret = &list->devList[i];
                ret->signature = DEV_LIST_ITEM_SIGNATURE;
                break;
            }
        }
    }

    return ret;
}
