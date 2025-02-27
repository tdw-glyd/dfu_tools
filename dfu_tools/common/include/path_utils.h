//#############################################################################
//#############################################################################
//#############################################################################
//
/// @file path_utils.h
/// @brief Platform-independent path/filename utilities header.
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
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <limits.h>
#endif

#define MAX_PATHUTILS_LEN           (512)

#if defined(__cplusplus)
extern "C" {
#endif

///
/// @fn: getExecutablePath
///
/// @details Cross-platform way to return the path an executable
///          is running from.
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
const char* getExecutablePath(void);

///
/// @fn: getDirectory
///
/// @details Given a full path/executable, this will return the directory
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
char* getDirectory(const char *path, char *dir, size_t dir_size);

///
/// @fn: isAbsolutePath
///
/// @details Determines if a path is absolute (fully-qualified) or relative
///
/// @param[in] path: The path to examine
/// @param[in]
/// @param[in]
/// @param[in]
///
/// @returns TRUE if the path is absolute. FALSE if not.
///
/// @tracereq(@req{xxxxxxx}}
///
bool isAbsolutePath(const char *path);

///
/// @fn: getCWD
///
/// @details Gets the current working directory (cross-platform)
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
char* getCWD(char* buffer, size_t bufferSize) ;

#if defined(__cplusplus)
}
#endif
