//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_proto_config.h
**
** DESCRIPTION: Configuration header for the DFU protocol library.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once

/*
** HOW MANY SIMULTANEOUS INSTANCES OF THE PROTOCL WILL THERE BE?
**
*/
#define MAX_PROTOCOL_INSTANCES                  (2)

/*
** The default MTU (in bytes)
**
*/
#define DEFAULT_MTU                             (8)

/*
** How many minutes should we wait before timing
** out the session?
**
*/
#define IDLE_SESSION_TIMEOUT_MINS               (5)

/*
** How long to wait following SESSION_STARTING before
** the session state reaches SESSION_ACTIVE?
**
*/
#define SESSION_STARTING_TIMEOUT_MINS           (1)

/*
** Default maximum message size This is determined by
** the constraints of the physical interface such as
** CAN, Ethernet, etc.
**
*/
#define MAX_MSG_LEN                             (1500)

/*
** DEFINES HOW MANY PERIODIC COMMANDS CAN BE RUNNING
**
**A caller can install command handlers that will be
** executed by the library at the rate specified.
** This is useful for things like sending "I'm alive"
** or status messages, etc.
**
*/
#define MAX_PERIODIC_COMMANDS                   (2)


/*
** If set to "1", the engine will always send a NAK
** to commands received that have no user-installed
** handler.
**
*/
#define NAK_UNSUPPORTED_COMMANDS                (0)

/*
** Device-dependent MAC address length.  Size to
** handle the worst-case of all the interfaces
** you will support.
**
*/
#define MAX_PHYS_MAC_LEN                        (24)

#if defined(__cplusplus)
extern "C" {
#endif




#if defined(__cplusplus)
}
#endif
