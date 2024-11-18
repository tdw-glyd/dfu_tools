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

#if defined(__cplusplus)
}
#endif