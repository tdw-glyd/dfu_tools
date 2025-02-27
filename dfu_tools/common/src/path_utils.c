//#############################################################################
//#############################################################################
//#############################################################################
//
/// @file path_utils.c
/// @brief Platform-independent tools to manage file paths, filenames, etc.
///
/// @details
///
/// @copyright 2024 Glydways, Inc
/// @copyright https://glydways.com
//
//#############################################################################
//#############################################################################
//#############################################################################
#include "path_utils.h"

#ifdef _WIN32
    #include <direct.h>
    #define GetCurrentDir _getcwd
#else
    #include <unistd.h>
    #define GetCurrentDir getcwd
#endif


char exe_path[MAX_PATHUTILS_LEN]; // Buffer to store the executable path

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                          PUBLIC API FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

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
const char* getExecutablePath(void) 
{
    #ifdef _WIN32
        // Windows implementation
        DWORD   result = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));

        if ( (result == 0) || (result == sizeof(exe_path)) )
        {
            // Error or buffer too small
            return NULL;
        }
    #else
        // Linux implementation
        ssize_t     count = readlink("/proc/self/exe", exe_path, sizeof(exe_path));
        if ( (count <= 0) || (count >= sizeof(exe_path)) )
        {
            // Error or buffer too small
            return NULL;
        }

        exe_path[count] = '\0'; // Null terminate the path
    #endif

    return exe_path;
}

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
char* getDirectory(const char *path, char *dir, size_t dir_size) 
{
    // Make a copy so we don't modify the original
    char            temp_path[MAX_PATHUTILS_LEN];

    strncpy(temp_path, path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';
    
    // Find the last path separator
    char *last_sep = NULL;
    
    #ifdef _WIN32
        // Check for both / and \ on Windows
        char*       last_fwd = strrchr(temp_path, '/');
        char*       last_back = strrchr(temp_path, '\\');
        
        if (last_back && (!last_fwd || last_back > last_fwd))
            last_sep = last_back;
        else
            last_sep = last_fwd;
    #else
        // Only need to check for / on Linux
        last_sep = strrchr(temp_path, '/');
    #endif
    
    if (last_sep) 
    {
        // Terminate the string at the last separator
        *(last_sep + 1) = '\0';
        strncpy(dir, temp_path, dir_size - 1);
        dir[dir_size - 1] = '\0';
    } 
    else 
    {
        // No directory component, use current directory
        strncpy(dir, "./", dir_size - 1);
        dir[dir_size - 1] = '\0';
    }

    return dir;
}

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
bool isAbsolutePath(const char *path) 
{
    if ( (!path) || (!*path) ) 
    {
        return false;  // Empty path is considered relative
    }

#ifdef _WIN32
    // Windows absolute paths:
    // 1. Drive letter followed by colon and slash (C:/, C:\)
    // 2. UNC paths (\\server\share)
    // 3. Extended-length paths (\\?\C:\)
    
    // Check for drive letter (e.g., "C:/" or "C:\")
    if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) 
    {
        if (path[1] == ':' && (path[2] == '/' || path[2] == '\\')) 
        {
            return true;
        }
    }
    
    // Check for UNC paths and extended paths (start with \\ or //)
    if ((path[0] == '\\' && path[1] == '\\') || (path[0] == '/' && path[1] == '/')) 
    {
        return true;
    }
    
    return false;
#else
    // Unix/Linux absolute paths start with /
    return path[0] == '/';
#endif
}

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
char* getCWD(char* buffer, size_t bufferSize) 
{
    if ( (!buffer) || (bufferSize == 0) )
    {
        return NULL;
    }
    
    if (GetCurrentDir(buffer, bufferSize) == NULL) 
    {
        // Error handling
        buffer[0] = '\0';
        return NULL;
    }
    
    // Ensure null termination (though API should do this already)
    buffer[bufferSize - 1] = '\0';
    
    return buffer;
}

