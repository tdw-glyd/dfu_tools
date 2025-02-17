//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: platform_sockets.c
**
** DESCRIPTION: Raw Ethernet socket library.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#include "ethernet_sockets.h"
#include "dfu_platform_utils.h"

#define DEBUG_SOCKETS               (0)


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
void print_mac_address(unsigned char *mac)
{
    printf("\r\n%02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

#if defined(_WIN32) || defined(_WIN64)

static bool pcapDLLLoaded = false;

/*!
** FUNCTION: LoadNpcapDlls
**
** DESCRIPTION: To use raw sockets, we need to link with and use the NPCAP
**              library (DLL).  This loads that library.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
bool LoadNpcapDlls()
{
    bool                ret = false;

    if (!pcapDLLLoaded)
    {
        char errbuf[PCAP_ERRBUF_SIZE];
        _TCHAR npcap_dir[512];
        UINT len;

        len = GetSystemDirectory(npcap_dir, 480);
        if (len)
        {
            _tcscat_s(npcap_dir, 512, _T("\\Npcap"));

            if (SetDllDirectory(npcap_dir) != 0)
            {
                if (0 == pcap_init(PCAP_CHAR_ENC_LOCAL, errbuf))
                {
                    ret = true;
                    pcapDLLLoaded = true;
                }
            }
        }
    }

	return (ret);
}

/*!
** FUNCTION: get_mac_address
**
** DESCRIPTION: Windows form of the "get_mac_address".
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
int get_mac_address(const char *interface_name, dfu_sock_t * socketHandle, uint8_t *macAddress)
{
    int                     ret = 0;
    IP_ADAPTER_INFO         adapter_info[16];
    DWORD                   outBufLen = sizeof(adapter_info);

    if (GetAdaptersInfo(adapter_info, &outBufLen) == NO_ERROR)
    {
        PIP_ADAPTER_INFO        adapter = adapter_info;

        while (adapter)
        {
            // Check if adapter name matches the device name
            if (strstr(interface_name, adapter->AdapterName))
            {
                memcpy(macAddress, &adapter->Address[0], 6);
                ret = 6;
                break;
            }
            adapter = adapter->Next;
        }
    }

    return (ret);
}

/*!
** FUNCTION: create_raw_socket
**
** DESCRIPTION: Uses the NPCAP library to create an object that will let us
**              send raw Ethernet frames.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
dfu_sock_t *create_raw_socket(const char *interface_name, dfu_sock_t *socketHandle)
{
    dfu_sock_t *ret = NULL;

    if (socketHandle)
    {
        char errbuf[PCAP_ERRBUF_SIZE] = {0};

        LoadNpcapDlls();

        // Create the pcap handle
        socketHandle->handle = pcap_create(interface_name, errbuf);
        if (socketHandle->handle != NULL)
        {
            // Set parameters before activation
            if (pcap_set_snaplen(socketHandle->handle, 262144) == 0 &&
                pcap_set_promisc(socketHandle->handle, 1) == 0 &&
                pcap_set_timeout(socketHandle->handle, 10) == 0 &&
                pcap_set_immediate_mode(socketHandle->handle, 1) == 0)
            {
                // Activate the pcap handle
                if (pcap_activate(socketHandle->handle) == 0)
                {
                    // Set non-blocking mode
                    if (pcap_setnonblock(socketHandle->handle, 1, errbuf) != -1)
                    {
                        // Get MAC address
                        if (get_mac_address(interface_name, socketHandle, socketHandle->myMAC) == 6)
                        {
                            ret = socketHandle; // Successfully initialized
                        }
                        else
                        {
                            fprintf(stderr, "Failed to get MAC address\n");
                            pcap_close(socketHandle->handle);
                        }
                    }
                    else
                    {
                        fprintf(stderr, "Failed to set non-blocking mode: %s\n", errbuf);
                        pcap_close(socketHandle->handle);
                    }
                }
                else
                {
                    fprintf(stderr, "Failed to activate pcap handle: %s\n", pcap_geterr(socketHandle->handle));
                    pcap_close(socketHandle->handle);
                }
            }
            else
            {
                fprintf(stderr, "Failed to configure pcap parameters\n");
                pcap_close(socketHandle->handle);
            }
        }
        else
        {
            fprintf(stderr, "Failed to create pcap handle: %s\n", errbuf);
        }
    }

    return ret;
}

/*!
** FUNCTION: send_ethernet_message
**
** DESCRIPTION: Uses NPCAP to send raw Ethernet frames.
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
                           uint16_t payload_size)
{
    if ( (socketHandle) && (interface_name) )
    {
        uint16_t         payloadLen;
        int16_t          bytesSent;

        // Set the destination MAC address in the frame
        memset(socketHandle->buffer, 0, MAX_ETHERNET_MSG_LEN);
        memcpy(socketHandle->buffer, dest_mac, 6); // Destination MAC address

        // Get the source MAC address (the MAC of the interface)
        memcpy(&socketHandle->buffer[6], socketHandle->myMAC, 6);

    #if (DEBUG_SOCKETS==1)
        printf("\nGot MAC: ");
        for (int i = 0; i < 6; i++)
        {
            printf("%02X", socketHandle->buffer[6+i]);

            if (i < 5)
            {
                printf(":");
            }
        }
    #endif
        //
        // Payload will be less than 1500 bytes, so field is length,
        // not EtherType
        //
        payloadLen = to_big_endian_16(payload_size);
        memcpy(&socketHandle->buffer[12], &payloadLen, 2);

        // Copy the payload
        memcpy(socketHandle->buffer + 14, payload, payload_size);

        // Send packet
        bytesSent = pcap_inject(socketHandle->handle, socketHandle->buffer, 14 + payload_size);
        if (bytesSent < 0)
        {
            fprintf(stderr, "Error sending the packet! pcap_inject() result was < 0...[%d]", bytesSent);
        }
        else
        if (bytesSent != 14 + payload_size)
        {
            fprintf(stderr, "Error sending the packet: %s\n", pcap_geterr(socketHandle->handle));
        }
    #if (DEBUG_SOCKETS==1)
        else
        {
            printf("Packet sent successfully\n");
        }
    #endif
    }

    return;
}

///
/// @fn: receive_ethernet_message
///
/// @details Uses raw sockets to receive our Ethernet frame.
///
/// @param[in] socketHandle : The specific port to get data from
/// @param[in] destBuff: Where the received data is placed.
/// @param[in] destBuffLen: [IN]: The max frame length to receive.
//                          [OUT]: The # of bytes actually received.
/// @param[in] expected_src_mac: UNUSED
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
uint8_t *receive_ethernet_message(dfu_sock_t *socketHandle, uint8_t *destBuff, uint16_t *destBuffLen, uint8_t *expected_src_mac)
{
    uint8_t             *ret = NULL;

    (void)expected_src_mac;

    if (
           (socketHandle) &&
           (socketHandle->handle) &&
           (destBuffLen) &&
           (*destBuffLen > 14)
       )
    {
        struct pcap_pkthdr         *hdr;
        const uint8_t              *pPacket;
        int                         result;

        result = pcap_next_ex(socketHandle->handle, &hdr, &pPacket);
        if ( (result == 1) && (pPacket != NULL) )
        {
            uint16_t            payloadLen;

            if ((payloadLen + 14) <= *destBuffLen)
            {
                memcpy(&payloadLen, pPacket+12, 2);
                payloadLen = from_big_endian_16(payloadLen);

                memcpy(destBuff, pPacket, hdr->len);

                *destBuffLen = payloadLen;
                ret = destBuff;
            }
        }
    }

    return (ret);
}

#else // Linux versions below

///
/// @fn: get_mac_address
///
/// @details Get's the MAC address of the named interface.
///
/// @param[in] interface_name
/// @param[in] socketHandle
/// @param[in] mac_address (target for result)
/// @param[in]
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
int get_mac_address(const char *interface_name, dfu_sock_t * socketHandle, uint8_t *mac_address)
{
    struct ifreq ifr;

    // Copy the interface name to the ifr structure
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);

    // Use ioctl to get the MAC address
    if (ioctl(socketHandle->sockfd, SIOCGIFHWADDR, &ifr) == -1)
    {
        perror("ioctl");
        close(socketHandle->sockfd);
        return -1;
    }

    // Copy the MAC address (6 bytes)
    memcpy(mac_address, ifr.ifr_hwaddr.sa_data, 6);

    return 0;
}

///
/// @fn: create_raw_socket
///
/// @details Linux version of creating new socket
///
/// @param[in] interfaceName: Which interface to bind with.
/// @param[in] socketHandle: The handle to use for this
/// @param[in]
/// @param[in]
///
/// @returns The address of the socket handle if success. NULL
///          else.
///
/// @tracereq(@req{xxxxxxx}}
///
dfu_sock_t * create_raw_socket(const char *interface_name, dfu_sock_t * socketHandle)
{
    dfu_sock_t*                 ret = NULL;

    if (
           (interface_name != NULL) &&
           (strlen(interface_name) > 0) &&
           (socketHandle != NULL)
       )
    {
        // Create a raw socket
        socketHandle->sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        if (socketHandle->sockfd == -1)
        {
            perror("socket");
            return NULL;
        }

        int loopback_opt = 0;
        if (setsockopt(socketHandle->sockfd, SOL_PACKET, PACKET_LOOPBACK, &loopback_opt, sizeof(loopback_opt)) < 0)
        {
            perror("setsockopt PACKET_LOOPBACK");
            // return NULL;
        }

        int sock_opt = 1;
        if (setsockopt(socketHandle->sockfd, SOL_SOCKET, SO_DONTROUTE, &sock_opt, sizeof(sock_opt)) < 0)
        {
            perror("setsockopt SO_DONTROUTE");
            return NULL;
        }

        if (setsockopt(socketHandle->sockfd, SOL_SOCKET, SO_NO_CHECK, &sock_opt, sizeof(sock_opt)) < 0)
        {
            perror("setsockopt SO_NO_CHECK");
            return NULL;
        }

        // Bind to the interface given by "interface_name"
        if (setsockopt(socketHandle->sockfd, SOL_SOCKET, SO_BINDTODEVICE, interface_name, strlen(interface_name)) < 0)
        {
            perror("setsockupt SO_BINDTODEVICE");
            return NULL;
        }

        // Bind the socket to the specified interface
        struct sockaddr_ll addr = {0};

        addr.sll_family = AF_PACKET;
        addr.sll_ifindex = if_nametoindex(interface_name);
        addr.sll_protocol = 0;  // TDW TEST htons(ETH_P_ALL);
        addr.sll_halen = ETH_ALEN;
        addr.sll_hatype = 1;

        if (bind(socketHandle->sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        {
            perror("bind");
            close(socketHandle->sockfd);
            return NULL;
        }

        // Enable Ethernet broadcast support
        int broadcastEnable = 1;
        if (setsockopt(socketHandle->sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) == -1)
        {
            perror("setsockopt");
            close(socketHandle->sockfd);
            return NULL;
        }

        // Set for successful return
        ret = socketHandle;

        printf("Socket successfully bound to interface %s. Index: %d. Socket FD: %d\n", interface_name, addr.sll_ifindex, socketHandle->sockfd);  // Debugging output
    }

    return ret;
}

///
/// @fn: send_ethernet_message
///
/// @details Linux version of sending a raw ethernet message.
///
/// @param[in] socketHandle
/// @param[in] interface_name
/// @param[in] dest_mac
/// @param[in] payload
/// @param[in] payload_size
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
void send_ethernet_message(dfu_sock_t * socketHandle,
                           const char *interface_name,
                           uint8_t *dest_mac,
                           uint8_t *payload,
                           uint16_t payload_size)
{
    unsigned char buffer[ETH_FRAME_LEN];
    struct ifreq ifr;
    struct sockaddr_ll addr = {0};
    ssize_t bytesSent;

    if (payload_size > (ETH_FRAME_LEN - 14))
    {
        fprintf(stderr, "Payload size too large.\n");
        return;
    }

    // Set the destination MAC address in the frame
    memset(buffer, 0, ETH_FRAME_LEN);
    memcpy(buffer, dest_mac, ETH_ALEN); // Destination MAC address

    // Get the source MAC address (the MAC of the interface)
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0'; // Ensure null termination

    if (ioctl(socketHandle->sockfd, SIOCGIFHWADDR, &ifr) == -1)
    {
        perror("ioctl(SIOCGIFHWADDR)");
        return;
    }

    /*
    printf("Source MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
           (unsigned char)ifr.ifr_hwaddr.sa_data[0],
           (unsigned char)ifr.ifr_hwaddr.sa_data[1],
           (unsigned char)ifr.ifr_hwaddr.sa_data[2],
           (unsigned char)ifr.ifr_hwaddr.sa_data[3],
           (unsigned char)ifr.ifr_hwaddr.sa_data[4],
           (unsigned char)ifr.ifr_hwaddr.sa_data[5]);
    */

    memcpy(buffer + 6, ifr.ifr_hwaddr.sa_data, 6); // Source MAC address

    // Ethernet type (custom protocol, e.g., 0x1234)
    uint16_t ethertype = htons(payload_size); // TDW TEST to_little_endian_16(payload_size);
    memcpy(&buffer[12], &ethertype, 2);

    // Payload
    memcpy(buffer + 14, payload, payload_size);

    // Set the sockaddr_ll structure for sending
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = if_nametoindex(interface_name); // Get index directly in send function
    addr.sll_protocol = 0; // TDW TEST htons(ETH_P_ALL); // Use same protocol as EtherType
    addr.sll_halen = ETH_ALEN;
    addr.sll_hatype = 1;
    memcpy(addr.sll_addr, dest_mac, ETH_ALEN);

    /*
    printf("Sending message to MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5]);
    */


    // Padd to minimum, if necessary
    #define MIN_FRAME_LENGTH   (60)
    size_t frame_size = 14 + payload_size;
    if (frame_size < MIN_FRAME_LENGTH)
    {
        // Minimum size minus FCS
        memset(buffer + frame_size, 0, MIN_FRAME_LENGTH - frame_size);
        frame_size = MIN_FRAME_LENGTH;
    }

    // Send the packet
    int flags = 0;
    flags |= MSG_CONFIRM | MSG_EOR;
    bytesSent = sendto(socketHandle->sockfd, buffer, frame_size, flags, (struct sockaddr*)&addr, sizeof(addr));
    if (bytesSent == -1)
    {
        perror("sendto");
        return;
    }
}

///
/// @fn: receive_ethernet_message
///
/// @details Uses raw sockets to receive our Ethernet frame.
///
/// @param[in] socketHandle : The specific port to get data from
/// @param[in] destBuff: Where the received data is placed.
/// @param[in][out] destBuffLen: [IN]: The max frame length to receive.
//                               [OUT]: The # of bytes actually received.
/// @param[in] expected_src_mac: UNUSED
///
/// @returns
///
/// @tracereq(@req{xxxxxxx}}
///
uint8_t *receive_ethernet_message(dfu_sock_t *socketHandle, uint8_t *destBuff, uint16_t *destBuffLen, uint8_t *expected_src_mac)
{
    uint8_t*            ret = NULL;

    if (
           (socketHandle) &&
           (destBuff) &&
           (destBuffLen) &&
           (*destBuffLen > 14)
       )
    {
        unsigned char       buffer[ETH_FRAME_LEN];
        ssize_t             numbytes;
        uint16_t            payloadLen;

        // Receive data
        numbytes = recvfrom(socketHandle->sockfd, buffer, *destBuffLen, 0, NULL, NULL);
        if (numbytes == -1)
        {
            perror("recvfrom");
            return NULL;
        }

        if (numbytes > 14)
        {
            memcpy(&payloadLen, &buffer[12], 2);
            payloadLen = from_big_endian_16(payloadLen);

            if (
                  (numbytes > payloadLen) &&
                  (numbytes-payloadLen >= 14) &&
                  ((payloadLen + 14) <= *destBuffLen)
               )
            {
                // Copy the entire frame, including the source/dest/len
                memcpy(destBuff, buffer, payloadLen + 14);

                // Return the payload length part
                *destBuffLen = payloadLen;

                // Return the address of the caller's buffer when success.
                ret = destBuff;
            }
        }
    }

    return ret;
}

#endif // _WIN32 && _WIN64
