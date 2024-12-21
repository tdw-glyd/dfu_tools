//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: general_utils.h
**
** DESCRIPTION: Utility module header.
**
** REVISION HISTORY: 
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once

#include <stdint.h>
#include <stdbool.h>


#if defined(__cplusplus)
extern "C" {
#endif

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
char * dfuToolStripQuotes(char *str);

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
int dfuToolStrnicmp(const char *s1, const char *s2, size_t n);

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
char * dfuToolStristr(const char *haystack, const char *needle);

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
uint32_t dfuToolGetFileSize(char *filename);

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
char *dfuToolPadStr(char *pStr, char padChar, int padLength);



#if defined(__cplusplus)
}
#endif