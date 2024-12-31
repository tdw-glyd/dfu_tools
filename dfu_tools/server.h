//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: server.h
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

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include "dfu_client.h"

#define USE_UDP                             (1)

#if (USE_UDP==1)
    #define IP_UDP                          (1)
#else
    #define IP_TCP                          (2)
#endif


#define SERVER_LISTEN_TCP_PORT          (8080)
#define SERVER_LISTEN_UDP_PORT          (12345)
#define BUFFER_SIZE                     (2048)
#define MAX_CLIENTS                     (1)

/*
** Types for the callbacks from the server.
**
*/
typedef uint32_t (*MSG_HANDLER_CALLBACK)(dfuClientEnvStruct *dfuClient, int sockfd, struct sockaddr_in *clientAddr, uint8_t *data, uint32_t dataLen);
typedef void (*PERIODIC_HANDLER_CALLBACK)(dfuClientEnvStruct *dfuClient, int sockfd, void *userPtr);


#if defined(__cplusplus)
extern "C" {
#endif

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
              uint32_t callbackIntervalMS);


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
int32_t sendUDPData(int sockfd, struct sockaddr_in *clientAddr, uint8_t *msg, uint32_t msgLen);

#if defined(__cplusplus)
}
#endif
