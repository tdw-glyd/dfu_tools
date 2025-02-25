//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: iface_enet.c
**
** DESCRIPTION: Ethernet interface library for the DFU tools.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "iface_enet.h"
#include "dfu_client_config.h"
#include "dfu_proto_api.h"

//
// Only set to 1 if the message receiver needs to
// match the sending MAC
//
#define COMPARE_SRC_MAC                 (0)

static const uint8_t ENET_BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

#define ETH_INTERFACE_SIGNATURE     (0xEDAC0983)
struct ifaceEthEnvStruct
{
    uint32_t                    signature;
    dfuProtocol *               dfu;
    dfu_sock_t                  socketHandle;
    void *                      userPtr;
    uint8_t                     destMAC[6];
    uint8_t                     myMAC[6];
    char                        interfaceName[MAX_IFACE_NAME_LEN+1];
    uint8_t                     msgBuff[MAX_MSG_LEN+128];
};

/*
** Ethernet environment instances.
**
*/
static ifaceEthEnvStruct        enetEnvs[MAX_ETHERNET_INTERFACES];


/*
** Internal prototypes
**
*/
static bool dfuClientEthernetInitEnv(ifaceEthEnvStruct *env);
static ifaceEthEnvStruct * dfuClientEthernetAllocEnv(void);
static bool dfuClientEthernetFreeEnv(ifaceEthEnvStruct * env);
uint8_t *dfuClientEnetRxCallback(dfuProtocol * dfu, uint16_t * rxBuffLen, dfuUserPtr userPtr);
bool dfuClientEnetTxCallback(dfuProtocol * dfu, uint8_t *txBuff, uint16_t txBuffLen, dfuMsgTargetEnum target, dfuUserPtr userPtr);
void dfuClientEnetErrCallback(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuErrorCodeEnum error, dfuUserPtr userPtr);

#define VALID_ETH_ENV(env)   ( (env != NULL) && (env->signature == ETH_INTERFACE_SIGNATURE) )


// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                            PUBLIC API FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: dfClientEthernetInit
**
** DESCRIPTION: Initializes the Ethernet aspect of the tool.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
ifaceEthEnvStruct * dfuClientEthernetInit(dfuProtocol **callerDFU, const char *interfaceName, void *userPtr)
{
    ifaceEthEnvStruct *           ret = NULL;

    if (interfaceName)
    {
        ret = dfuClientEthernetAllocEnv();
        if (ret)
        {
            if (create_raw_socket(interfaceName, &ret->socketHandle))
            {
                // Save our interface name
                snprintf(ret->interfaceName, MAX_IFACE_NAME_LEN, "%s", interfaceName);

                // Sock our MAC address away.
                get_mac_address(interfaceName, &ret->socketHandle, ret->myMAC);

                // Get the protocol library set up.
                ret->dfu = dfuCreate(dfuClientEnetRxCallback,
                                     dfuClientEnetTxCallback,
                                     dfuClientEnetErrCallback,
                                     (void *)ret,
                                     0);
                if (ret->dfu)
                {
                    dfuSetMTU(ret->dfu, MAX_ETHERNET_MSG_LEN);
                    ret->userPtr = (void *)userPtr;
                    *callerDFU = ret->dfu;
                }
            }
            else
            {
                dfuClientEthernetUnInit(ret);
                ret = NULL;
            }
        }
    }

    return (ret);
}

/*!
** FUNCTION: dfuClientEthernetUnInit
**
** DESCRIPTION: Clean up Ethernet-specific items.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool dfuClientEthernetUnInit(ifaceEthEnvStruct *env)
{
    bool                        ret = false;

    if (VALID_ETH_ENV(env))
    {
        // Do any clean up here
        dfuDestroy(env->dfu);

        // Free things
        ret = dfuClientEthernetFreeEnv(env);
    }

    return (ret);
}

/*!
** FUNCTION: dfuClientEthernetSetDest
**
** DESCRIPTION: Converts a STRING formatted MAC address into a byte
**              array that is saved to the environment as the DESTINATION
**              MAC
*
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool dfuClientEthernetSetDest(ifaceEthEnvStruct * env, char *dest)
{
    bool                    ret = false;

    if ( (VALID_ETH_ENV(env)) && (dest) )
    {
        ret = (ifaceEthernetMACStringToBytes(dest, env->destMAC, sizeof(env->destMAC)) != NULL) ? true : false;
    }

    return (ret);
}


// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                         INTERNAL SUPPORT FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: dfuClientEthernetAllocEnv
**
** DESCRIPTION: Search the env pool for an available environment. If found,
**              initialize it.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static ifaceEthEnvStruct * dfuClientEthernetAllocEnv(void)
{
    ifaceEthEnvStruct *                 ret = NULL;
    int                                 index;

    for (index = 0; index < MAX_ETHERNET_INTERFACES; index++)
    {
        if (enetEnvs[index].signature != ETH_INTERFACE_SIGNATURE)
        {
            if (dfuClientEthernetInitEnv(&enetEnvs[index]))
            {
                ret = &enetEnvs[index];
                break;
            }
        }
    }

    return (ret);
}

/*!
** FUNCTION: dfuClientEthernetFreeEnv
**
** DESCRIPTION: Clean up and return env to pool.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static bool dfuClientEthernetFreeEnv(ifaceEthEnvStruct * env)
{
    bool                    ret = false;

    if (VALID_ETH_ENV(env))
    {
        // Do any de-inits

        // Clean up
        dfuClientEthernetInitEnv(env);
        env->signature = 0x00000000;

        ret = true;
    }

    return (ret);
}

/*!
** FUNCTION: dfuClientEthernetInitEnv
**
** DESCRIPTION: Inits the environment item to defaults/
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static bool dfuClientEthernetInitEnv(ifaceEthEnvStruct *env)
{
    bool                        ret = false;

    if (env)
    {
        // Validate the item (makes it unavailable)
        env->signature = ETH_INTERFACE_SIGNATURE;

        ret = true;
    }

    return (ret);
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//         CALLBACKS THAT MUST BE PROVIDED TO THE DFU PROTOCOL LIBRARY
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

///
/// @fn: dfuClientEnetRxCallback
///
/// @details Fetch the most recent ethernet message from the interface.
///          Stash the SRC & DST MAC addresses for use with device
///          lists, etc.
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
uint8_t *dfuClientEnetRxCallback(dfuProtocol * dfu, uint16_t * rxBuffLen, dfuUserPtr userPtr)
{
    uint8_t                 *ret = NULL;
    ifaceEthEnvStruct       *env = (ifaceEthEnvStruct *)userPtr;

    if (
           (VALID_ETH_ENV(env)) &&
           (dfu) &&
           (rxBuffLen)
       )
    {
        uint8_t *   res = NULL;
        uint16_t    maxRxLen = sizeof(env->msgBuff);

        // Initial response length
        *rxBuffLen = 0;

        //
        // We send in the max length to receive.  Once the call completes,
        // "maxRxLen" should contain the length of what we received, including
        // the Ethernet header (src MAC, dst MAC, length)
        //
        res = receive_ethernet_message(&env->socketHandle, env->msgBuff, &maxRxLen, NULL);
        if ( (res) && (maxRxLen > 0) )
        {
            uint8_t                     srcMAC[6];

            // Save the length of what we just received to the caller.
            *rxBuffLen = maxRxLen;

            // Save the SRC and DST MAC to the engine
            dfuSetDstPhysicalID(dfu, env->destMAC, 6);
            memcpy(srcMAC, env->msgBuff + 6, 6);
            dfuSetSrcPhysicalID(dfu, srcMAC, 6);

        #if (COMPARE_SRC_MAC==1)
            if (memcmp(env->destMAC, srcMAC, 6) == 0)
        #endif
            {
                ret = (uint8_t *)&env->msgBuff[14];
            }
        }
    }

    return (ret);
}

///
/// @fn: dfuClientEnetTxCallback
///
/// @details Transmits a raw ethernet frame.
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
bool dfuClientEnetTxCallback(dfuProtocol * dfu, uint8_t *txBuff, uint16_t txBuffLen, dfuMsgTargetEnum target, dfuUserPtr userPtr)
{
    bool                    ret = false;
    ifaceEthEnvStruct *     env = (ifaceEthEnvStruct *) userPtr;

    if (
           (VALID_ETH_ENV(env)) &&
           (dfu) &&
           (txBuff) &&
           (txBuffLen > 0) &&
           (txBuffLen <= MAX_ETHERNET_MSG_LEN)
       )
    {
        uint8_t                 *pDst = env->destMAC; // default

        if (target == DFU_TARGET_ANY)
        {
            pDst = (uint8_t *)&ENET_BROADCAST_MAC[0];
        }

        send_ethernet_message(&env->socketHandle, env->interfaceName, pDst, txBuff, txBuffLen);
        ret = true;
    }

    return (ret);
}

void dfuClientEnetErrCallback(dfuProtocol * dfu, uint8_t *msg, uint16_t msgLen, dfuErrorCodeEnum error, dfuUserPtr userPtr)
{
    return;
}

///
/// @fn: ifaceEthernetMACBytesToString
///
/// @details Converts an array of MAC address bytes to a C
///          STRING, provided by the caller.
///          This is one of the REQUIRED MAC conversion
///          functions that each interface type (ETHERNET,
///          CAN, UART, ETC) must provided.
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
char* ifaceEthernetMACBytesToString(uint8_t* mac, uint8_t macLen, char* destStr, uint8_t destStrLen)
{
    char*                   ret = NULL;

    if (
           (mac) &&
           (macLen == 6) &&
           (destStr) &&
           (destStrLen >= 18)
       )
    {
        snprintf(destStr,
                 destStrLen,
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0],
                 mac[1],
                 mac[2],
                 mac[3],
                 mac[4],
                 mac[5]);

        ret = destStr;
    }

    return ret;
}

///
/// @fn: ifaceEthernetMACStringToBytes
///
/// @details Function to convert from an Ethernet MAC STRING, to
///          an array of bytes, provided by the caller.
///          This is one of the REQUIRED MAC conversion functions that
///          each interface type (ETHERNET, CAN, UART, ETC) must
///          provide.
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
uint8_t* ifaceEthernetMACStringToBytes(char* macStr, uint8_t* mac, uint8_t macLen)
{
    uint8_t*                ret = NULL;

    if (
           (macStr) &&
           (strlen(macStr) >= 17) &&
           (mac) &&
           (macLen >= 6)
       )
    {
        uint32_t                 temp[6];

        if (sscanf(macStr,
                   "%x:%x:%x:%x:%x:%x",
                   &temp[0],
                   &temp[1],
                   &temp[2],
                   &temp[3],
                   &temp[4],
                   &temp[5]) == 6)
        {
            for (int i = 0; i < 6; i++)
            {
                if (temp[i] > 255)
                {
                    return NULL;
                }
                mac[i] = (uint8_t)temp[i];
            }

            ret = mac;
        }
    }

    return (ret);
}



