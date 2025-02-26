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
** FUNCTION: dfuToolStricmp
**
** DESCRIPTION: Compares two strings, ignores case and length.
**
** ARGUMENTS:
**
** NOTES:
*/
int dfuToolStricmp (const char *s1, const char *s2);

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

///
/// @fn: dfuToolExtractPath
///
/// @details Does an in-place extraction of the path to a file.
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
char* dfuToolExtractPath(char* buffer);

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

/*!
** FUNCTION: dfuToolLtrim
**
** DESCRIPTION: Trim non-printables from LEADING side of string
**
** ARGUMENTS:
**
** NOTES:
*/
char *dfuToolLtrim(char *s);

/*!
** FUNCTION: dfuToolRtrim
**
** DESCRIPTION: Trim non-printables from the TRAILING edge of the string.
**
** ARGUMENTS:
**
** NOTES:
*/
char *dfuToolRtrim(char *s);

/*!
** FUNCTION: dfuToolTrim
**
** DESCRIPTION: Trims both leading and trailing sides of string.
**
** ARGUMENTS:
**
** NOTES:
*/
char *dfuToolTrim(char *s);



//
// Helpful macros
//
#define LTRIM(str)          dfuToolLtrim(str)
#define RTRIM(str)          dfuToolRtrim(str)
#define TRIM(str)           dfuToolTrim(str)

#if defined(__cplusplus)
}
#endif


