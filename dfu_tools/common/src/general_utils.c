//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: general_utils.c
**
** DESCRIPTION: Utility functions for the tool.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include "general_utils.h"

#if defined(_WIN32) || defined(_WIN64)
    #include <conio.h>
#else
    #include <termios.h>
#endif // defined

/*!
** FUNCTION: dfuToolStripQuotes
**
** DESCRIPTION: Remove any double-quotes from the string passed.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
char * dfuToolStripQuotes(char *str)
{
    char *src = str, *dst = str;

    // Iterate through the string
    while (*src) {
        // Copy character if it's not a quote
        if (*src != '"') {
            *dst = *src;
            dst++;
        }
        src++;
    }

    // Null-terminate the result string
    *dst = '\0';

    return (str);
}

/*!
** FUNCTION: dfuToolStricmp
**
** DESCRIPTION: Compares two strings, ignores case and length.
**
** ARGUMENTS:
**
** NOTES:
*/
int dfuToolStricmp (const char *s1, const char *s2)
{
    char    c1, c2;
    int     result = 0;

    if ( (s1) && (s2) )
    {
        while (result == 0)
        {
            c1 = *s1++;
            c2 = *s2++;
            if ((c1 >= 'a') && (c1 <= 'z'))
                c1 = (char)(c1 - ' ');
            if ((c2 >= 'a') && (c2 <= 'z'))
                c2 = (char)(c2 - ' ');
            if ((result = (c1 - c2)) != 0)
                break;
            if ((c1 == 0) || (c2 == 0))
                break;
        }
    }

    return result;
}

/*!
** FUNCTION: dfuToolStrnicmp
**
** DESCRIPTION: Case-insensitive string comparison
**
** PARAMETERS:
**
** RETURNS: 0 if a match.  Non-zero otherwise
**
** COMMENTS:
**
*/
int dfuToolStrnicmp(const char *s1, const char *s2, size_t n)
{
    uint8_t                 c1;
    uint8_t                 c2;

    if ( (s1) && (s2) )
    {
        while (n-- != 0 && (*s1 || *s2))
        {
            c1 = *(const unsigned char *)s1++;
            if ('a' <= c1 && c1 <= 'z')
                c1 += ('A' - 'a');
            c2 = *(const unsigned char *)s2++;
            if ('a' <= c2 && c2 <= 'z')
                c2 += ('A' - 'a');
            if (c1 != c2)
                return c1 - c2;
        }
    }

    return 0;
}

/*!
** FUNCTION: dfuToolStristr
**
** DESCRIPTION: Does a case-insensitve search for "needle" in
**              the "haystack".  If found, returns the address
**              where it was found. If not found, returns NULL
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
char * dfuToolStristr(const char *haystack, const char *needle)
{
    int c = tolower((unsigned char)*needle);
    if (c == '\0')
        return (char *)haystack;

    for (; *haystack; haystack++)
    {
        if (tolower((unsigned char)*haystack) == c)
        {
            size_t              i;

            for (i = 0;;)
            {
                if (needle[++i] == '\0')
                    return (char *)haystack;

                if (tolower((unsigned char)haystack[i]) != tolower((unsigned char)needle[i]))
                    break;
            }
        }
    }

    return NULL;
}

/*!
** FUNCTION: dfuToolGetFileSize
**
** DESCRIPTION: Returns the length of the named file.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
uint32_t dfuToolGetFileSize(char *filename)
{
    uint32_t             ret = 0;

    if ((filename) && (strlen(filename)) )
    {
        FILE *                  handle = fopen(filename, "r+b");

        if (handle)
        {
            fseek(handle, 0, SEEK_END);
            ret = (uint32_t)ftell(handle);
            fclose(handle);
        }
    }

    return (ret);
}

/*!
** FUNCTION: dfuToolPadStr
**
** DESCRIPTION: Pad a string to the desired length using the
**              specified character as the padding.
**
** PARAMETERS:
**
** RETURNS:
**
** COMMENTS:
**
*/
char *dfuToolPadStr(char *pStr, char padChar, int padLength)
{
    int             i;
    char           *pLocal;
    int             localLen;

    if ( (pStr) && (strlen(pStr) < (size_t)padLength) )
    {
        pLocal = pStr + strlen(pStr);
        localLen = (padLength - strlen(pStr));
        for (i = 0; i < localLen; i++)
        {
            *pLocal = padChar;
            pLocal++;
            *pLocal = 0x00;
        }

        return (pStr);
    }

    return (NULL);
}

/*!
** FUNCTION: dfuToolLtrim
**
** DESCRIPTION: Trim non-printables from LEADING side of string
**
** ARGUMENTS:
**
** NOTES:
*/
char *dfuToolLtrim(char *s)
{
    char* newstart = s;

    if (s)
    {
        while (isspace( (int)*newstart))
        {
            ++newstart;
        }

        // newstart points to first non-whitespace char (which might be '\0')
        memmove( s, newstart, strlen( newstart) + 1); // don't forget to move the '\0' terminator
    }

    return (s);
}

/*!
** FUNCTION: dfuToolRtrim
**
** DESCRIPTION: Trim non-printables from the TRAILING edge of the string.
**
** ARGUMENTS:
**
** NOTES:
*/
char *dfuToolRtrim(char *s)
{
    char *end = s + strlen(s);

    // find the last non-whitespace character
    while ((end != s) && isspace( (int)*(end-1)))
    {
        --end;
    }

    // at this point either (end == s) and s is either empty or all whitespace
    //      so it needs to be made empty, or
    //      end points just past the last non-whitespace character (it might point
    //      at the '\0' terminator, in which case there's no problem writing
    //      another there).
    *end = '\0';

    return (s);
}

/*!
** FUNCTION: UTIL_trim
**
** DESCRIPTION: Trims both leading and trailing sides of string.
**
** ARGUMENTS:
**
** NOTES:
*/
char *dfuToolTrim(char *s)
{
    return ( dfuToolRtrim(dfuToolRtrim(s)) );
}
