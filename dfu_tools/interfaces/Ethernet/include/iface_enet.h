//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: iface_enet.h
**
** DESCRIPTION: DFU tool Ethernet interface support header.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once

#include "dfu_proto_api.h"
#include "ethernet_sockets.h"

// Add your types, definitions, macros, etc. here

/*
** Opaque Ethernet ENV struct.
**
*/
typedef struct ifaceEthEnvStruct ifaceEthEnvStruct;

#if defined(__cplusplus)
extern "C" {
#endif

/*!
** FUNCTION: dfuClientEthernetInit
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
ifaceEthEnvStruct * dfuClientEthernetInit(dfuProtocol **callerDFU, const char *interfaceName, void *userPtr);

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
bool dfuClientEthernetUnInit(ifaceEthEnvStruct *env);

/*!
** FUNCTION: dfuClientEthernetSetDest
**
** DESCRIPTION: Set the destination MAC into the env.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool dfuClientEthernetSetDest(ifaceEthEnvStruct * env, char *dest);

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
char* ifaceEthernetMACBytesToString(uint8_t* mac, uint8_t macLen, char* destStr, uint8_t destStrLen);

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
uint8_t* ifaceEthernetMACStringToBytes(char* macStr, uint8_t* mac, uint8_t macLen);

#if defined(__cplusplus)
}
#endif
