//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: platform_sockets.h
**
** DESCRIPTION: Raw Ethernet sockets library.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "dfu_client_config.h"

#if !defined(_WIN32) && !defined(_WIN64)
    #include <arpa/inet.h>
    #include <net/if.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <linux/if_packet.h>
    #include <linux/if_ether.h>
    #include <linux/if_arp.h>
#else
    #undef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
    #include <pcap.h>
    #include <winsock2.h>
    #include <windows.h>
    #include <iphlpapi.h>
    #include <tchar.h>
    #include <ws2tcpip.h>
#endif // _WIN32


/*
** Portable socket handle struct.
**
*/
typedef struct
{
#if !defined(_WIN32) && !defined(_WIN64)
    int                 sockfd;
#else
    pcap_t *            handle;
#endif
    uint8_t             myMAC[6];
    uint8_t             buffer[2048];
}dfu_sock_t;



#if defined(__cplusplus)
extern "C" {
#endif


/*!
** FUNCTION: print_mac_address
**
** DESCRIPTION: Helper function to display MAC address.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
void print_mac_address(unsigned char *mac);

/*!
** FUNCTION: get_mac_address
**
** DESCRIPTION: Get's the MAC address of the named interface.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
int get_mac_address(const char *interface_name, dfu_sock_t * socketHandle, uint8_t *macAddress);

/*!
** FUNCTION: create_raw_socket
**
** DESCRIPTION: Create a raw socket and bind to an interface
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
dfu_sock_t * create_raw_socket(const char *interface_name, dfu_sock_t * socketHandle);

/*!
** FUNCTION: send_ethernet_message
**
** DESCRIPTION: Send a raw ethernet message.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
void send_ethernet_message(dfu_sock_t * socketHandle,
                           const char *interface_name,
                           uint8_t *dest_mac,
                           uint8_t *payload,
                           uint16_t payload_size);

/*!
** FUNCTION: receive_ethernet_message
**
** DESCRIPTION: Gets raw ehernet messages from the interface.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
uint8_t *receive_ethernet_message(dfu_sock_t *socketHandle, uint8_t *destBuff, uint16_t *destBuffLen, uint8_t *expected_src_mac);

#if defined(__cplusplus)
}
#endif

