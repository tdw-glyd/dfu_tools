#include <stdio.h>
#include <stdlib.h>

#include "dfu_client_api.h"

int main()
{
    dfuClientAPI*       api = dfuClientAPIGet(INTERFACE_TYPE_ETHERNET,
                                              "eth0",
                                              "FOO");
    if (api)
    {
        uint64_t                counter;
        deviceInfoStruct*       devRecord;

        //ASYNC_TIMER_STRUCT    timer;
        //TIMER_Start(&timer);

        counter = 100000;
        do
        {
            dfuClientAPI_LL_IdleDrive(api);
        }while (--counter > 0);

        devRecord = dfuClientAPI_LL_GetFirstDevice(api);

        if (devRecord)
        {
            printf("\r\n ::: DEVICE LIST :::\r\n");
            do
            {
                printf("\r\n        Device Type: %02d", devRecord->deviceType);
                printf("\r\n     Device Variant: %02d", devRecord->deviceVariant);
                printf("\r\n Device Status Bits: 0x%02X", devRecord->statusBits);
                printf("\r\n    Core Image Mask: 0x%02X", devRecord->coreImageMask);
                printf("\r\n         BL Version: %d.%d.%d", devRecord->blVersionMajor, devRecord->blVersionMinor, devRecord->blVersionPatch);
                printf("\r\n         Time Stamp: %s", ctime(&devRecord->timestamp));

                devRecord = dfuClientAPI_LL_GetNextDevice(api);
                printf("\r\n\r\n");
            }while (devRecord != NULL);
        }

        dfuClientAPIPut(api);
    }
    return 0;
}
