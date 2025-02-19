#include <stdio.h>
#include <stdlib.h>
#include <sched.h>

#include "dfu_client_api.h"

int main()
{
    uint16_t mtu = 387;
    dfuClientAPI*       api = dfuClientAPIGet(INTERFACE_TYPE_ETHERNET,
                                              "enxc8a3621a2c16",
                                              "./public_key.pem",
                                              mtu);
    if (api)
    {
        uint64_t                counter;
        deviceInfoStruct*       devRecord;


//#ifdef HGHGHG
        uint8_t    MAC[6] = {0x66,0x55,0x44,0x33,0x22,0x11};
        for (;;)
        {

            dfuClientAPI_LL_BeginSession(api, 1, 5, MAC, 6);

            dfuClientAPI_LL_NegotiateMTU(api, 387);
        }

        //ASYNC_TIMER_STRUCT    timer;
        //TIMER_Start(&timer);
//#endif // HGHGHG

        counter = 150;
        do
        {
            dfuClientAPI_LL_IdleDrive(api);
        }while (--counter > 0);

        devRecord = dfuClientAPI_LL_GetFirstDevice(api);

        if (devRecord)
        {
            printf("\r\n\r\n     ::: DEVICE LIST :::\r\n");
            do
            {
                printf("\r\n        Device Type: %02d", devRecord->deviceType);
                printf("\r\n     Device Variant: %02d", devRecord->deviceVariant);
                printf("\r\n Device Status Bits: 0x%02X", devRecord->statusBits);
                printf("\r\n    Core Image Mask: 0x%02X", devRecord->coreImageMask);
                printf("\r\n         BL Version: %d.%d.%d", devRecord->blVersionMajor, devRecord->blVersionMinor, devRecord->blVersionPatch);
                printf("\r\n         Device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                       devRecord->physicalID[0],
                       devRecord->physicalID[1],
                       devRecord->physicalID[2],
                       devRecord->physicalID[3],
                       devRecord->physicalID[4],
                       devRecord->physicalID[5]);
                printf("\r\n         Time Stamp: %s", ctime(&devRecord->timestamp));


                dfuClientAPI_LL_BeginSession(api, 1, 5, devRecord->physicalID, 6);

                devRecord = dfuClientAPI_LL_GetNextDevice(api);
                printf("\r\n\r\n");
                fflush(stdout);
            }while (devRecord != NULL);
        }

        dfuClientAPIPut(api);
    }
    return 0;
}
