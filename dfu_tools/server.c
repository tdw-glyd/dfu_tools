//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: server.c
**
** DESCRIPTION: dfu tools server module.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "server.h"
#include "async_timer.h"
#include "general_utils.h"


/*
** USE THE FOLLOWING LINE FROM A LINUX OR MAC COMMAND SHELL TO DO TESTING:
**
**   echo -n "Four score and 7 years ago" | nc -u -w1 192.168.86.46 12345
**
*/


/*
** For compatibility
**
*/
#ifdef _WIN32
typedef int socklen_t;
#endif

/*
** Local support prototypes
**
*/
static void serverPlatformInit(void);
static void serverPlatformCleanup(void);
static void close_socket(int socket);
static void set_non_blocking(int socket);
static int32_t _defaultMsgHandler(dfuClientEnvStruct *dfuClient, int sockfd, struct sockaddr_in *clientAddr, uint8_t *data, uint32_t dataLen);



// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                       PUBLIC API IMPLEMENTATIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$


#if defined(IP_UDP)

/*!
** FUNCTION: serverRun
**
** DESCRIPTION: Run the UDP server.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
int serverRun(dfuClientEnvStruct *dfuClient,
              MSG_HANDLER_CALLBACK msgHandler,
              PERIODIC_HANDLER_CALLBACK periodicCallback,
              void *userPtr,
              uint32_t callbackIntervalMS)
{
    int                         ret = -1;
    int                         sockfd;
    struct sockaddr_in          server_addr;
    struct sockaddr_in          client_addr;
    char                        buffer[BUFFER_SIZE];
    socklen_t                   addr_len = sizeof(client_addr);
    bool                        runServer = true;
    ASYNC_TIMER_STRUCT          periodicTimer;
    bool                        timerRunning = false;

    serverPlatformInit();

    if ( (periodicCallback) && (callbackIntervalMS) )
    {
        TIMER_Start(&periodicTimer);
        timerRunning = true;
    }

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        serverPlatformCleanup();
        return -1;
    }

    set_non_blocking(sockfd);

    // Prepare server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all network interfaces
    server_addr.sin_port = htons(SERVER_LISTEN_UDP_PORT);

    // Bind socket to address
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close_socket(sockfd);
        serverPlatformCleanup();
        return -1;
    }

    printf("Server is listening on port %d...\n", SERVER_LISTEN_UDP_PORT);

    // Loop to receive requests
    while (runServer)
    {
        int n = recvfrom(sockfd,
                         (char *)buffer,
                         BUFFER_SIZE,
                         0,
                         (struct sockaddr *)&client_addr,
                         &addr_len);
        if (n < 0)
        {
            // No data.  Go back to looking for data.
            continue;
        }

        buffer[n] = '\0';  // Null-terminate the received string
        printf("Received data: %s\n", buffer);

        /*
        ** If the caller provided a message handler, call it
        **
        */
        if (msgHandler)
        {
            // Invoke the caller's message-handler callback
            msgHandler(dfuClient, sockfd, &client_addr, (uint8_t *)buffer, strlen(buffer));
        }
        else
        {
            // Invoke the default message handler
            _defaultMsgHandler(dfuClient, sockfd, &client_addr, (uint8_t *)buffer, strlen(buffer));
        }

        /*
        ** If the caller provided an interval and a callback address,
        ** this will call that handler at the rate specified.
        **
        */
        if (timerRunning)
        {
            if (TIMER_Finished(&periodicTimer, callbackIntervalMS))
            {
                periodicCallback(dfuClient, sockfd, userPtr);
            }
        }
    }

    /*
    ** Clean up before we leave.
    **
    */
    ret = 0;
    close_socket(sockfd);
    serverPlatformCleanup();

    return (ret);
}

/*!
** FUNCTION: sendUDPData
**
** DESCRIPTION: Send data to the client
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
int32_t sendUDPData(int sockfd, struct sockaddr_in *clientAddr, uint8_t *msg, uint32_t msgLen)
{
    uint32_t                    ret = 0;

    if ( (sockfd) && (clientAddr) && (msg) && (msgLen) )
    {
        socklen_t                   addr_len = sizeof(struct sockaddr_in);

        printf("Sending default response: %s", msg);
        ret = sendto(sockfd, (char *)msg, msgLen+1, 0, (struct sockaddr *)clientAddr, addr_len);
    }

    return (ret);
}


// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                INTERNAL SUPPORT FUNCTION IMPLEMENTATIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: serverPlatformInit
**
** DESCRIPTION: 
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
static void serverPlatformInit(void)
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

/*!
** FUNCTION: serverPlatformCleanup
**
** DESCRIPTION: 
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
static void serverPlatformCleanup(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

/*!
** FUNCTION: close_socket
**
** DESCRIPTION: 
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
static void close_socket(int socket)
{
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

/*!
** FUNCTION: set_non_blocking
**
** DESCRIPTION: 
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
static void set_non_blocking(int socket)
{
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(socket, FIONBIO, &mode);
#else
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#endif
}

/*!
** FUNCTION: _defaultMsgHandler
**
** DESCRIPTION: If the server's caller didn't provide a message handler, this is
**              called to send a simple error KVP string.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
static int32_t _defaultMsgHandler(dfuClientEnvStruct *dfuClient, int sockfd, struct sockaddr_in *clientAddr, uint8_t *data, uint32_t dataLen)
{
    int32_t                     ret = 0;

    if ( (sockfd) && (data) && (dataLen) )
    {
        char            msg[1024];

        sprintf(msg, "%s\n", "RESULT=FAILED MSG=NO_MSG_HANDLER");
        ret = sendUDPData(sockfd, clientAddr, (uint8_t *)msg, strlen(msg));
    }

    return (ret);
}
#endif

#if defined(IP_TCP)
int serverRun(void)
{
    serverPlatformInit();

    int                 server_fd;
    int                 new_socket;
    int                 activity;
    int                 i;
    int                 valread;

    int                 client_sockets[MAX_CLIENTS] = {0};
    struct sockaddr_in  address;
    char                rx_buffer[BUFFER_SIZE] = {0};
    fd_set              readfds;
    struct timeval      tv;
    bool                runServer = true;;

    // Set up timeout for non-blocking "select"
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("Socket creation failed");
        serverPlatformCleanup();
        return -1;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        close_socket(server_fd);
        serverPlatformCleanup();
        return -1;
    }

    // Configure address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_LISTEN_TCP_PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        close_socket(server_fd);
        serverPlatformCleanup();
        return -1;
    }

    // Listen for connections
    if (listen(server_fd, MAX_CLIENTS) < 0)
    {
        perror("Listen failed");
        close_socket(server_fd);
        serverPlatformCleanup();
        return -1;
    }

    printf("Non-blocking server listening on port %d...\n", SERVER_LISTEN_PORT);

    // Set the server socket to non-blocking mode
    set_non_blocking(server_fd);

    while (runServer)
    {
        // Clear and set the file descriptor set
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_fd = server_fd;

        // Add client sockets to the set
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            if (client_sockets[i] > 0)
            {
                FD_SET(client_sockets[i], &readfds);
                if (client_sockets[i] > max_fd)
                {
                    max_fd = client_sockets[i];
                }
            }
        }

        // Wait for activity on any socket (timeout = NULL for no blocking)
        activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0)
        {
            perror("select");
            break;
        }

        // Check for new incoming connections
        if (FD_ISSET(server_fd, &readfds))
        {
            socklen_t addr_len = sizeof(address);
            new_socket = accept(server_fd, (struct sockaddr *)&address, &addr_len);
            if (new_socket >= 0)
            {
                printf("New connection established\n");

                // Add the new socket to the client list
                for (i = 0; i < MAX_CLIENTS; i++)
                {
                    if (client_sockets[i] == 0)
                    {
                        client_sockets[i] = new_socket;
                        set_non_blocking(new_socket); // Set new socket as non-blocking
                        break;
                    }
                }
            }
        }

        // Check for activity on client sockets
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            int client_fd = client_sockets[i];
            if (FD_ISSET(client_fd, &readfds))
            {
                valread = recv(client_fd, rx_buffer, BUFFER_SIZE - 1, 0);
                if (valread <= 0)
                {
                    // Connection closed
                    printf("Client disconnected\n");
                    close_socket(client_fd);
                    client_sockets[i] = 0;
                }
                else
                {
                    // Process received data
                    rx_buffer[valread] = '\0';
                    printf("Received: %s\n", rx_buffer);

                    // Respond with a static JSON message
                    char tx_buffer[BUFFER_SIZE];
                    snprintf(tx_buffer, BUFFER_SIZE, "{\"status\":\"success\",\"echo\":\"%s\"}", rx_buffer);
                    send(client_fd, tx_buffer, strlen(tx_buffer), 0);
                }
            }
        }
    }

    // Cleanup
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_sockets[i] > 0)
        {
            close_socket(client_sockets[i]);
        }
    }
    close_socket(server_fd);
    serverPlatformCleanup();
    return 0;
}
#endif
