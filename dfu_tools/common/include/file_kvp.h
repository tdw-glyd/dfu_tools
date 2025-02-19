//#############################################################################
//#############################################################################
//#############################################################################
//
/// @file file_kvp.h
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

#include <stdio.h>
#include <stdbool.h>
#include "kvparse.h"


#define MAX_KVP_LINE_LEN                (128)
#define KVP_FILE_COMMENT_CHAR           ('#')


/*
** Client provides the address of one of these with
** each call.
**
*/
#define FKVP_SIGNATURE          (0x001A77EB)
typedef struct
{
    uint32_t        signature;
    FILE*           handle;
    char            lineBuffer[MAX_KVP_LINE_LEN];
    PARSED_KVP      parsedKVP;
}fkvpStruct;

#if defined(__cplusplus)
extern "C" {
#endif


///
/// @fn: fkvpBegin
///
/// @details "opens" a file-kvp session.  Opens the target
///          file.
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
fkvpStruct* fkvpBegin(char* kvpFilePath, fkvpStruct* kvp);

///
/// @fn: fkvpEnd
///
/// @details Closes the file and invalidates the kvp structure.
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
bool fkvpEnd(fkvpStruct* kvp);

///
/// @fn: fkvpNext
///
/// @details Reads the next file line, then parses it.  If
///          that works, returns the address of the KVPARSE 
///          structure that the client can use to fetch
///          keys and values
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
PARSED_KVP* fkvpNext(fkvpStruct* kvp);

///
/// @fn: fkvpFind
///
/// @details Search a file of KVP's for the key you want.
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
char* fkvpFind(fkvpStruct* kvp, char* keyName, bool fromStart);

///
/// @fn: fkvpRewind
///
/// @details Reset the file pointer for an already-opened
///          file.
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
bool fkvpRewind(fkvpStruct* kvp);

/*
** API MACROS
**
*/
#define fkvpGetValue(kvp, keyName)     KVPARSE_getValueForKey((const char *)keyName, &((fkvpStruct *)kvp)->parsedKVP)
#define fkvpKeyCount(kvp)              KVP_KEYCOUNT(&((fkvpStruct *)kvp)->parsedKVP) 


#if defined(__cplusplus)
}
#endif