//#############################################################################
//#############################################################################
//#############################################################################
//
/// @file kvparse.c
/// @brief
///
/// @details
///
/// @copyright 2024 Glydways, Inc
/// @copyright https://glydways.com
//
//#############################################################################
//#############################################################################
//#############################################################################
#include "kvparse.h"
#include "general_utils.h"


/*
** Internal support method prototypes
**
*/
static KVP *_KVPARSE_findKey(const char *key, PARSED_KVP *kvp);

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//                       EXPORTED API IMPLEMENTATIONS
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
PARSED_KVP *MKVPARSE_parseKVP(char *pBuffer, uint16_t bufLen, PARSED_KVP *pKVP, uint8_t *pKeyCount, int shouldAlloc, char *callerBuf, size_t callerBufSize)
{
    uint16_t                Index;
    char                   *pCurrent = NULL;
    char                   *pKey = NULL;
    char                   *end;
    uint16_t                payloadLen = 0;
    int                     equalIndex;
    int                     spaceIndex;
    char                   *pBuf;

    if (
          (pBuffer) &&
          (pKVP) &&
          (bufLen > 0) &&
          (bufLen < MAX_KVP_STRING_LEN)
       )
    {
        if (pKeyCount != NULL)
            *pKeyCount = 0;

        memset(pKVP, 0x00, sizeof(PARSED_KVP));

        // Reset the indices of the located '=' and SPACE chars
        for (Index = 0; Index < MAX_PARSED_KEYS; Index++)
        {
            pKVP->f_EqualIndices[Index] = -1;
            pKVP->f_SpaceIndices[Index] = -1;
        }
        equalIndex = 0;
        spaceIndex = 0;

        pKVP->f_KVPCount = 0;

        pKVP->f_Base = pBuffer;
        pKVP->allocated = false;

    #if (USE_ALLOCATION==1)
        if (shouldAlloc)
        {
            pKVP->f_Base = (char *)KVP_MEMALLOC(bufLen+4);
            if (pKVP->f_Base)
            {
                pKVP->allocated = true;
                memcpy(pKVP->f_Base, pBuffer, bufLen);
            }
        }
        else
    #endif
        {
            if ( (callerBuf != NULL) && (callerBufSize > 0) )
            {
                size_t          toCopy = (bufLen <= callerBufSize) ? bufLen : callerBufSize;
                memset(callerBuf, 0x00, callerBufSize);
                memcpy(callerBuf, pBuffer, toCopy);
                pKVP->f_Base = callerBuf;
            }
        }

        pBuf = pKVP->f_Base;
        end = pBuf;
        end += bufLen;

        pCurrent = pBuf;
        Index = 0;

        while (Index < MAX_PARSED_KEYS)
        {
            //***
            // Find equal sign.
            // Make sure we don't run past end of buffer.
            //***
            while ( (*pCurrent != 0x00) && (pCurrent <= (pBuf+bufLen) ) && (*pCurrent != '=') )
            {
                ++pCurrent;
            }

            //***
            // Found, now find value and key parts
            //***
            if (*pCurrent == '=')
            {
                pKey = pCurrent;
                *pKey = 0x00; // replace equal sign

                // Track where we replaced '=' chars
                pKVP->f_EqualIndices[equalIndex++] = pCurrent-pBuf;

                //***
                // Save value pointer
                //***
                ++pCurrent;
                pKVP->f_KVP[Index].pValue = pCurrent;

                // If we aren't accumulating a "DATA" value...
                if (payloadLen == 0)
                {
                    // See if the VALUE is enclosed in quotes. If
                    // so, we need to treat everyting in between
                    // as literal and do NOT modify.
                    if ( (*pCurrent == 0x22) || (*pCurrent == 0x2C) )
                    {
                        //***
                        // Move pCurrent to the end of the current value
                        //***
                        ++pCurrent;
                        while ( (*pCurrent != 0x22) && (*pCurrent != 0x2C) && (*pCurrent != 0x00) ) ++pCurrent;
                    }

                    // Move to the next key (find the SPACE) or the end of the buffer.
                    while ( (*pCurrent != 0x20) && (*pCurrent != 0x00) ) ++pCurrent;

                    *pCurrent = 0x00; // replace SPACE. make it appear as its own string by terminating

                    // Track where we replaced SPACE chars
                    pKVP->f_SpaceIndices[spaceIndex++] = pCurrent-pBuf;

                    // Next byte in message...
                    ++pCurrent;
                }

                //***
                // Find beginning of key.
                // We have to back "pKey" up by one, since it is
                // currently pointing at a 0x00 char that replaced
                // the "="
                //***
                pKey--;

                //***
                // Now, back "pKey" to the first SPACE or the first char in the
                // buffer
                //***
                while ( (pKey > pBuf) && (*pKey != 0x20) && (*pKey != 0x00) )
                    pKey--;

                //***
                // Verify that the character at the pointer
                // is valid.
                //***
                if (
                      (*pKey < 0x30) ||
                      ( (*pKey > 0x39) && (*pKey < 0x41) ) ||
                      ( (*pKey > 0x5A) && (*pKey < 0x61) ) ||
                      ( *pKey > 0x7A)
                   )
                {
                	// Check to be sure we aren't at the
                	// beginning of the buffer
                	if (pKey != pBuf)
                		++pKey;
                }

                //***
                // Save pointer to key
                //***
                pKVP->f_KVP[Index].pKey = pKey;

                /*
                ** Check for the "LEN" key and if found, save the length into the
                ** local "dataLen" variable for us to use.
                **
                */
                if ((
                       (dfuToolStricmp(pKVP->f_KVP[Index].pKey, PAYLOAD_LEN_KEY) == 0) ||
                       (dfuToolStricmp(pKVP->f_KVP[Index].pKey, PAYLOAD_LEN_KEY_2) == 0)
                    ) &&
                    (
                       (pKVP->f_KVP[Index].pValue != NULL)
                    ))
                {

                    payloadLen = (uint16_t)strtoul(pKVP->f_KVP[Index].pValue, NULL, 10);
                }
                else
                // If the current key is the DATA key and there was a previous LEN key
                // (whose value we saved), then move the "pCurrent" past it.  This
                // allows there to be other KVP data following payload data.
                if (
                       (dfuToolStricmp(pKVP->f_KVP[Index].pKey, PAYLOAD_DATA_KEY) == 0) &&
                       (payloadLen > 0)
                   )
                {
                    // Save the data length to this element now.
                    pKVP->f_KVP[Index].payloadLen = payloadLen;

                    // Skip to end of the DATA portion
                    pCurrent += payloadLen;

                    // Clear for next set of text KVP...
                    payloadLen = 0;
                }

                //***
                // This is what we return to the caller. It is the
                // number of KVP items found in this string
                //***
                ++pKVP->f_KVPCount;

                if (pKeyCount != NULL)
                    *pKeyCount = pKVP->f_KVPCount;

                if (pCurrent >= end)
                {
                    return (pKVP);
                }
            }
            else
            if (pKVP->f_KVPCount > 0)
            {
                return (pKVP);
            }

            ++Index;
            pKVP->f_KVP[Index].payloadLen = 0;
        }

        if (pKVP->f_KVPCount > 0)
            return (pKVP);
    }

    return (NULL);
}

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
int KVPARSE_unparseKVP(PARSED_KVP *pKVP)
{
    int                         ret = 0;

    if (pKVP)
    {
        if (!pKVP->allocated)
        {
            char                    *current = pKVP->f_Base;
            int                      index;

            for (index = 0; index < MAX_PARSED_KEYS; index++)
            {
                if (pKVP->f_EqualIndices[index] >= 0)
                {
                    current = pKVP->f_Base + pKVP->f_EqualIndices[index];
                    *current = '=';
                }

                if (pKVP->f_SpaceIndices[index] >= 0)
                {
                    current = pKVP->f_Base + pKVP->f_SpaceIndices[index];
                    *current = 0x20;
                }
            }
        }
        else
        {
            KVP_MEMFREE(pKVP->f_Base);
        }

        ret = 1;
    }

    return (ret);
}

/*!
** FUNCTION: KVPARSE_getValueForKey
**
** DESCRIPTION: Given a key to search for, this will walk the list
**              of parsed KVP and attempt to find it.  If it succeeds,
**              a pointer to the VALUE string is returned.
**
** NOTE: This search is NOT case-sensitive and does only a simple
**       linear search, since there can only be 10 key/value
**       pairs in one message.
**
*/
char *KVPARSE_getValueForKey(const char *pKey, PARSED_KVP *pKVP)
{
    KVP             *found = NULL;

    if (
          (pKey) &&
          (pKVP) &&
          (pKVP->f_KVPCount > 0)
       )
    {
        found = _KVPARSE_findKey(pKey, pKVP);
        if (found)
        {
            return (found->pValue);
        }
    }

    return (NULL);
}

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
KVP *KVPARSE_findKey(const char *pKey, PARSED_KVP *kvp)
{
    return (_KVPARSE_findKey(pKey, kvp));
}

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
KVP *KVPARSE_getKVPByIndex(PARSED_KVP *kvp, uint32_t index)
{
    KVP             *returnVal = NULL;

    if (
          (kvp) &&
          (index < kvp->f_KVPCount)
       )
    {
        returnVal = &kvp->f_KVP[index];
    }

    return (returnVal);
}

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
KVP *KVPARSE_firstKVP(PARSED_KVP *kvp)
{
    if (
          (kvp) &&
          (kvp->f_KVPCount > 0)
       )
    {
        return (&kvp->f_KVP[0]);
    }

    return (NULL);
}

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
KVP *KVPARSE_nextKVP(PARSED_KVP *kvp, KVP *item)
{
    KVP                 *retVal = NULL;
    if (
          (kvp) &&
          (item)
       )
    {
        uint8_t                 *bytePtr = (uint8_t *)item;
        uint8_t                 *last;

        // Compute the address of the last parsed record
        last = (uint8_t *)&kvp->f_KVP[0];
        last += (kvp->f_KVPCount * sizeof(KVP));

        // Move the "item" pointer forward by one
        bytePtr += sizeof(KVP);

        // If the new address falls into range of what was parsed,
        // set the return up.
        if (bytePtr <= last)
            retVal = (KVP *)bytePtr;
    }

    return (retVal);
}

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
uint16_t KVPARSE_getPayloadLen(PARSED_KVP *pKVP)
{
    KVP                 *found;

    if (pKVP)
    {
        found = _KVPARSE_findKey(PAYLOAD_DATA_KEY, pKVP);
        if (found)
        {
            return (found->payloadLen);
        }
    }

    return (0);
}

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
void KVPARSE_dumpKVP(PARSED_KVP *pKVP)
{
    int             index;

    if (pKVP)
    {
        printf("\r\n>>> KVP DUMP <<<\r\n");
        printf("\r\n   [%d] Keys found.\r\n", pKVP->f_KVPCount);
        fflush(stdout);

        for (index = 0; index < MAX_PARSED_KEYS; index++)
        {
            if (pKVP->f_KVP[index].pKey != NULL)
            {
                if (pKVP->f_KVP[index].pValue != NULL)
                {
                    printf("\r\n%02d. KEY: %s, VALUE: %s",
                           index,
                           pKVP->f_KVP[index].pKey,
                           pKVP->f_KVP[index].pValue);
                }
                else
                {
                    printf("\r\n%02d. KEY: %s, VALUE=<missing>",
                            index,
                            pKVP->f_KVP[index].pKey);
                }
            }

            fflush(stdout);
        }
    }
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//                         INTERNAL SUPPORT METHODS
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

/*!
** FUNCTION: _KVPARSE_findKey
**
** DESCRIPTION: Search the parse list for a matching KEY name and if
**              found, return the address of the parse element.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
static KVP *_KVPARSE_findKey(const char *key, PARSED_KVP *kvp)
{
    uint16_t             Index = 0;

    if (
          (key) &&
          (strlen(key) > 0) &&
          (kvp)
       )
    {
        while (Index < kvp->f_KVPCount)
        {
            //***
            // Case-sensitive comparison here!!!
            //***
            if (dfuToolStricmp(key, kvp->f_KVP[Index].pKey) == 0)
            {
                //***
                // Return a pointer to the "value"
                // part of the string
                //***
                return (&kvp->f_KVP[Index]);
            }

            ++Index;
        }
    }

    return (NULL);
}




