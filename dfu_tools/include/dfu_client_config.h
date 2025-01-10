//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_client_config.h
**
** DESCRIPTION:
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once

/*
** Set this to the max active Ethernet interfaces
**
*/
#define MAX_ETHERNET_INTERFACES                                      (2U)

/*
** How big of an Ethernet frame can we send?
**
*/
#define MAX_ETHERNET_MSG_LEN                                         (384+3U)

/*
** Set this to the max CAN interfaces
**
*/
#define MAX_CAN_INTERFACES                                           (3U)

/*
** What is the maximum size of an interface name?
**
*/
#define MAX_IFACE_NAME_LEN                                           (32U)



#if defined(__cplusplus)
extern "C" {
#endif




#if defined(__cplusplus)
}
#endif
