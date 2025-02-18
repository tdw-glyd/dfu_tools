//#############################################################################
//#############################################################################
//#############################################################################
//
/// @file logger.h
/// @brief Logging header
/// @copyright 2024,2025 Glydways, Inc
/// @copyright https://glydways.com
//
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define LOG_BUFFER_SIZE 4096
#define LOG_MAX_ENTRY_SIZE 128

/*
** Supported logging levels
**
*/
typedef enum 
{
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN  = 1,
    LOG_LEVEL_INFO  = 2,
    LOG_LEVEL_DEBUG = 3,
    LOG_LEVEL_COUNT = 4  // Used for array sizing
} logLevelEnum;


/*
** Each log entry has one of these at the front.
**
*/
typedef struct 
{
    uint32_t        timestamp;
    uint32_t        msgLen;
    uint32_t        crc;
} logEntryHeaderStruct;


/*
** Each log level buffer has this structure
**
*/
typedef struct 
{
    uint8_t         buffer[LOG_BUFFER_SIZE];
    uint32_t        writeIndex;
    uint32_t        readIndex;
    uint32_t        entryCount;
} logBufferStruct;

/*
** This is the top-level logging structure
**
*/
typedef struct 
{
    logBufferStruct     buffers[LOG_LEVEL_COUNT];
    logLevelEnum        maxLevel;
} logSystemStruct;

/*
** Allows clients to read from the logs
**
*/
typedef struct 
{
    uint32_t            timestamp;
    logLevelEnum        level;
    uint32_t            msg_len;
    const uint8_t*      msg;
} logEntryStruct;

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                          EXPORTED API PROTOTYPES
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

// Initialize the logging system
void logerInit(LogSystem* log);

// Core logging function that takes pre-formatted message
bool loggerWrite(LogSystem* log, logLevelEnum level, const uint8_t* msg, uint32_t len);

// Helper functions for common number types
bool loggerWrite_uint32(LogSystem* log, logLevelEnum level, const uint8_t* prefix, uint32_t value);
bool loggerWrite_hex32(LogSystem* log, logLevelEnum level, const uint8_t* prefix, uint32_t value);

// Read functions
bool loggerReadNext(LogSystem* log, logLevelEnum level, logEntryStruct* entry);
void loggerReadReset(LogSystem* log, logLevelEnum level);
uint32_t log_get_entry_count(LogSystem* log, logLevelEnum level);


#if defined(__cplusplus)
extern "C" {
#endif




#if defined(__cplusplus)
}
#endif