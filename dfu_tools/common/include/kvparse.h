//#############################################################################
//#############################################################################
//#############################################################################
//
/// @file kvparse.h
/// @brief 
///
/// @details 
///
/// @copyright 2024,2025 Glydways, Inc
/// @copyright https://glydways.com
//
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/*
** Define to "1" in order to use non-destructive
** parsing.  This could be used with constant 
** strings, for example.  Not suitable when 
** running embedded.
**
*/
#define USE_ALLOCATION                  (0)


#if (USE_ALLOCATION==1)
    #define KVP_MEMALLOC(x)             MEMALLOC(x)
    #define KVP_MEMFREE(x)              MEMFREE(x)
#else
    #define KVP_MEMALLOC(x)
    #define KVP_MEMFREE(x)
#endif    

#define MAX_PARSED_KEYS         (40)
#define MAX_KVP_STRING_LEN      (1024)

//
// These are used to extract opaque payload information
// from a KVP line.
//
#define PAYLOAD_LEN_KEY          ("LEN")
#define PAYLOAD_LEN_KEY_2        ("LENGTH")
#define PAYLOAD_DATA_KEY         ("DATA")

/*
** Structure that is used to point to
** where the keys and values are in a
** serial buffer.
** Done with 16-bit alignment for devices
** that cannot do un-aligned multi-byte
** access.
*/
typedef struct
{
	uint16_t		 payloadLen;
	char*            pKey;
    char*            pValue;
}KVP;

/*
** Primary KVP structure that contains
** the count of parsed keys and values
** and the pointers to each key and each
** value
*/
typedef struct
{
    uint16_t       f_KVPCount;
    char*          f_Base;
    bool           allocated;
    int            f_EqualIndices[MAX_PARSED_KEYS];
    int            f_SpaceIndices[MAX_PARSED_KEYS];
    KVP            f_KVP[MAX_PARSED_KEYS];
}PARSED_KVP;

#ifdef __cplusplus
extern "C" {
#endif

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//                       PUBLIC EXPORTED PROTOTYPES
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

/*!
** FUNCTION: KVPARSE_parseKVP
**
** DESCRIPTION: Parses a message buffer and puts  pointers to the
**              various parts into the "pKVP" structure.
**              Returns the count of KVP items found and
**              if that's greater than zero, returns the address
**              of the KVP structure originally passed in.  This
**              being non-zero allows calling code to decide what
**              to do next.
**
** This parser simply manipulates the received message by putting NULLs
** in place of the "=" signs and at the end of each key and value,
** effectively splitting the message buffer up into multiple smaller strings.
** Then we save pointers to the keys and values in the KVP part of the overall
** data structure, along with the number of KVPs we saw.  This way we don't
** need to do any allocation on the fly and we can re-use the KVP and
** message buffers for any new message.
**
*/
PARSED_KVP *MKVPARSE_parseKVP(char *pBuffer, 
                              uint16_t bufLen, 
                              PARSED_KVP *pKVP, 
                              uint8_t *pKeyCount, 
                              int shouldAlloc, 
                              char *callerBuf, 
                              size_t callerBufSize);

/*
** Macros to generic KVP parsing names
**
*/                              
#define KVPARSE_parseKVP(a,b,c,d)                MKVPARSE_parseKVP(a,b,c,d,0,NULL,0)
#define KVPARSE_parseKVPAlloc(a,b,c,d)           MKVPARSE_parseKVP(a,b,c,d,1,NULL,0)
#define KVPARSE_parseKVPBuf(a,b,c,d,e,f)         MKVPARSE_parseKVP(a,b,c,d,0,e,f)


/*!
** FUNCTION: KVPARSE_unparseKVP 
**
** DESCRIPTION: Use the saved indices of '=' and SPACE chars
**              to restore the original string.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
int KVPARSE_unparseKVP(PARSED_KVP *pKVP);

/*!
** FUNCTION: KVPARSE_getValueForKey
** DESCRIPTION: Given a key to search for, this will walk the list
**              of parsed KVP and attempt to find it.  If it succeeds,
**              a pointer to the VALUE string is returned.
**
** NOTE: This search IS case-sensitive and does only a simple
**       linear search, since there can only be 10 key/value
**       pairs in one message.
**
*/
char *KVPARSE_getValueForKey(const char *pKey, PARSED_KVP *pKVP);

/*!
** FUNCTION: KVPARSE_findKey 
**
** DESCRIPTION: Searches for the named key and returns the address of
**              the entire KVP record associated with it.
**
** PARAMETERS:
**
** RETURNS: 
**
** COMMENTS:
** 
*/
KVP *KVPARSE_findKey(const char *pKey, PARSED_KVP *kvp);

/*!
** FUNCTION: KVPARSE_getKVPByIndex 
**
** DESCRIPTION: Returns a pointer to the element given by
**              "index", if it's in range.
**
** PARAMETERS:
**
** RETURNS: 
**
** COMMENTS:
** 
*/
KVP *KVPARSE_getKVPByIndex(PARSED_KVP *kvp, uint32_t index);

/*!
** FUNCTION: KVPARSE_firstKVP 
**
** DESCRIPTION: Returns the address of the first parsed KVP
**              item.
**
** PARAMETERS:
**
** RETURNS: 
**
** COMMENTS:
** 
*/
KVP *KVPARSE_firstKVP(PARSED_KVP *kvp);

/*!
** FUNCTION: KVPARSE_nextKVP 
**
** DESCRIPTION: Returns the next parsed KVP item
**
** PARAMETERS:
**
** RETURNS: 
**
** COMMENTS:
** 
*/
KVP *KVPARSE_nextKVP(PARSED_KVP *kvp, KVP *item);

/*!
** FUNCTION: KVPARSE_getPayloadLen
**
** DESCRIPTION: Searches a parsed KVP to see if there is a "DATA"
**              key present.  If there is, this will return the
**              length.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
uint16_t KVPARSE_getPayloadLen(PARSED_KVP *pKVP);

/*!
** FUNCTION: KVPARSE_dumpKVP  
**
** DESCRIPTION: Displays the contents of a parsed KVP structure
**
** PARAMETERS:
**
** RETURNS: 
**
** COMMENTS:
** 
*/
void KVPARSE_dumpKVP(PARSED_KVP *pKVP);

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//                               "HELPER" ITEMS
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

/*
**  Some Helpful macros
**
*/
#define KVP_KEYCOUNT(x)             (((PARSED_KVP *)(x))->f_KVPCount)

#ifdef __cplusplus
}
#endif
