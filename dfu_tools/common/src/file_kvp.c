//#############################################################################
//#############################################################################
//#############################################################################
//
/// @file file_kvp.c
/// @brief Parses a KVP file and passes the parsed KVP back to the caller
///        for each line it processes.
///
/// @details Callers can walk through each line of a KVP file, parsing
///          each and then use the parsed data. They can also do a higher-level
///          "search" operation that can walk the entire file to find a value
///          associated with a specified key.
///
/// @copyright 2024 Glydways, Inc
/// @copyright https://glydways.com
//
//#############################################################################
//#############################################################################
//#############################################################################
#include <string.h>
#include "file_kvp.h"
#include "general_utils.h"

// Simple macro to validate the fkvpStruct pointer
#define VALID_FKVP(kvp)  ( (kvp) && (((fkvpStruct*)kvp)->signature == FKVP_SIGNATURE) )

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                    PUBLIC API FUNCTION IMPLEMENTATIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

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
/// @tracereq @req{xxxxxxx}
///
fkvpStruct* fkvpBegin(char* kvpFilePath, fkvpStruct* kvp)
{
    fkvpStruct*                   ret = NULL;

    if (kvp)
    {
        kvp->handle = fopen(kvpFilePath, "r");

        if (kvp->handle != NULL)
        {
            fseek(kvp->handle, 0, SEEK_SET);
            memset(kvp->lineBuffer, 0x00, sizeof(kvp->lineBuffer));
            memset(&kvp->parsedKVP, 0x00, sizeof(PARSED_KVP));

            kvp->signature = FKVP_SIGNATURE;

            ret = kvp;
        }
    }

    return ret;
}

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
bool fkvpEnd(fkvpStruct* kvp)
{
    bool                ret = false;

    if (VALID_FKVP(kvp))
    {
        if (kvp->handle)
        {
            fclose(kvp->handle);
            kvp->handle = NULL;
        }

        kvp->signature = 0;
        ret = true;
    }

    return ret;
}

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
/// @returns A pointer to a PARSED_KVP structure that the caller
///          can use to retrieve values associated with specified
///          keys. That pointer can also be used to get the *count*
///          of keys that were parsed, using the "fkvpKeyCount()"
///          macro function.
///
/// @tracereq(@req{xxxxxxx}}
///
PARSED_KVP* fkvpNext(fkvpStruct* kvp)
{
    PARSED_KVP*             ret = NULL;

    if (VALID_FKVP(kvp))
    {
        if (!feof(kvp->handle))
        {
            bool                done = false;

            /*
            ** To get the next line, we need to:
            **
            **  - Verify we actually read something
            **  - Skip any comment lines
            **  - Parse the KVP line.
            **
            */
            do
            {
                memset(kvp->lineBuffer, 0x00, sizeof(kvp->lineBuffer));

                // Read a line of text from the file
                if (fgets(kvp->lineBuffer, MAX_KVP_LINE_LEN, kvp->handle) != NULL)
                {
                    uint8_t         keyCount = 0;

                    // Remove leading/trailing SPACE chars
                    TRIM(kvp->lineBuffer);

                    // Make sure the line is NOT a comment
                    if ( (strlen(kvp->lineBuffer) > 0) && (kvp->lineBuffer[0] != KVP_FILE_COMMENT_CHAR) )
                    {
                        /*
                        ** Parse the line
                        **
                        */
                        if (KVPARSE_parseKVP(kvp->lineBuffer,
                                            strlen(kvp->lineBuffer),
                                            &kvp->parsedKVP,
                                            &keyCount) != NULL)
                        {
                            ret = &kvp->parsedKVP;
                        }

                        done = true;
                    }
                }
                else
                    done = true;
            }while (!done);
        }
    }

    return ret;
}

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
char* fkvpFind(fkvpStruct* kvp, char* keyName, bool fromStart)
{
    char*                   ret = NULL;

    if (VALID_FKVP(kvp))
    {
        if ( (keyName) && (strlen(keyName) > 0) )
        {
            PARSED_KVP*         foundKVP;

            // Rewind the file pointer?
            if (fromStart)
            {
                fkvpRewind(kvp);
            }

            /*
            ** Walk the file, searching for a matching key.
            **
            */
            while (1)
            {
                foundKVP = fkvpNext(kvp);
                if (foundKVP)
                {
                    char*       found = fkvpGetValue(foundKVP, keyName);

                    if (found)
                    {
                        ret = found;
                        break;
                    }
                }
                else
                    break;
            }
        }
    }

    return ret;
}

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
bool fkvpRewind(fkvpStruct* kvp)
{
    bool            ret = false;

    if (VALID_FKVP(kvp))
    {
        if (kvp->handle != NULL)
        {
            fseek(kvp->handle, 0, SEEK_SET);
            memset(&kvp->parsedKVP, 0x00, sizeof(PARSED_KVP));
            ret = true;
        }
    }

    return ret;
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                       INTERNAL SUPPORT FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$


