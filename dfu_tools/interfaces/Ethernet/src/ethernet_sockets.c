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

#if defined(_WIN32) || !defined(_WIN64)

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

uint8_t *receive_ethernet_message(dfu_sock_t *socketHandle, uint8_t *destBuff, uint16_t *destBuffLen, uint8_t *expected_src_mac)
{
    uint8_t             *ret = NULL;

    if ( (socketHandle) && (socketHandle->handle) && (destBuffLen) )
    {
        struct pcap_pkthdr         *hdr;
        const uint8_t              *pPacket;
        int                         result;

        result = pcap_next_ex(socketHandle->handle, &hdr, &pPacket);
        if ( (result == 1) && (pPacket != NULL) )
        {
            uint16_t            payloadLen;

            memcpy(&payloadLen, pPacket+12, 2);
            payloadLen = from_big_endian_16(payloadLen);

            memcpy(destBuff, pPacket, hdr->len);

            *destBuffLen = payloadLen;
            ret = destBuff;
        }
        else
        {
            // usleep(1000);
        }
    }

    return (ret);
}

#else

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


dfu_sock_t * create_raw_socket(const char *interface_name, dfu_sock_t * socketHandle)
{
    // Create a raw socket
    socketHandle->sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (socketHandle->sockfd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the specified interface
    struct sockaddr_ll addr = {0};
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = if_nametoindex(interface_name);
    addr.sll_protocol = htons(ETH_P_ALL);

    if (bind(socketHandle->sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        perror("bind");
        close(socketHandle->sockfd);
        exit(EXIT_FAILURE);
    }

    // Enable Ethernet broadcast support
    int broadcastEnable = 1;
    if (setsockopt(socketHandle->sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) == -1)
    {
        perror("setsockopt");
        close(socketHandle->sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Socket successfully bound to interface %s. Index: %d. Socket FD: %d\n", interface_name, addr.sll_ifindex, socketHandle->sockfd);  // Debugging output

    return socketHandle;
}

void send_ethernet_message(dfu_sock_t * socketHandle,
                           const char *interface_name,
                           uint8_t *dest_mac,
                           uint8_t *payload,
                           uint16_t payload_size)
{
    unsigned char buffer[ETH_FRAME_LEN];
    struct ifreq ifr;
    struct sockaddr_ll addr = {0};

    printf("\r\nSending to Socket FD: %d", socketHandle->sockfd);

    if (payload_size > (ETH_FRAME_LEN - 14))
    {
        fprintf(stderr, "Payload size too large.\n");
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }
    printf("Source MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
           (unsigned char)ifr.ifr_hwaddr.sa_data[0],
           (unsigned char)ifr.ifr_hwaddr.sa_data[1],
           (unsigned char)ifr.ifr_hwaddr.sa_data[2],
           (unsigned char)ifr.ifr_hwaddr.sa_data[3],
           (unsigned char)ifr.ifr_hwaddr.sa_data[4],
           (unsigned char)ifr.ifr_hwaddr.sa_data[5]);

    memcpy(buffer + 6, ifr.ifr_hwaddr.sa_data, 6); // Source MAC address

    // Ethernet type (custom protocol, e.g., 0x1234)
    uint16_t ethertype = to_little_endian_16(payload_size);
    memcpy(&buffer[12], &ethertype, 2);

    // Payload
    memcpy(buffer + 14, payload, payload_size);

    // Set the sockaddr_ll structure for sending
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = if_nametoindex(interface_name); // Get index directly in send function
    addr.sll_protocol = htons(ETH_P_ALL); // Use same protocol as EtherType
    addr.sll_halen = ETH_ALEN;
    memcpy(addr.sll_addr, dest_mac, ETH_ALEN);

    printf("Sending message to MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5]);

    // Send the packet
    if (sendto(socketHandle->sockfd, buffer, 14 + payload_size, 0, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    printf("Message sent successfully.\n");
}

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
uint8_t *receive_ethernet_message(dfu_sock_t *socketHandle, uint8_t *destBuff, uint16_t *destBuffLen, uint8_t *expected_src_mac)
{
    unsigned char buffer[ETH_FRAME_LEN];
    ssize_t numbytes;

    while (1)
    {
        // Receive data
        numbytes = recvfrom(socketHandle->sockfd, buffer, ETH_FRAME_LEN, 0, NULL, NULL);
        if (numbytes == -1)
        {
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }

        // Check if the source MAC matches the expected source MAC
        if (memcmp(buffer + 6, expected_src_mac, 6) == 0)
        {
            printf("Received message from ");
            print_mac_address(buffer + 6);
            printf("Payload: %s\n", buffer + 14);  // Assuming the payload is a string
        }
    }
}


#ifdef HGHGHGHG
int main() {
    const char *interface_name = "eth0";  // Change to the appropriate interface
    unsigned char dest_mac[6] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};  // Destination MAC address
    unsigned char src_mac[6] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};   // Expected source MAC for receiving
    unsigned char payload[] = "Hello, Ethernet!";

    // Create a raw socket
    int sockfd = create_raw_socket(interface_name);

    // Send raw Ethernet message
    send_ethernet_message(sockfd, interface_name, dest_mac, payload, sizeof(payload));

    // Receive raw Ethernet message
    receive_ethernet_message(sockfd, src_mac);

    // Close the socket
    close(sockfd);

    return 0;
}
#endif


#endif // _WIN32 && _WIN64
