//#############################################################################
//#############################################################################
//#############################################################################
//
/// @file logger.c
/// @brief Logger for the DFU interface tools.
///
/// @details
///
/// @copyright 2024 Glydways, Inc
/// @copyright https://glydways.com
//
//#############################################################################
//#############################################################################
//#############################################################################
#include "logger.h"

// log_system.c
#include "log_system.h"
#include <string.h>



/*
** Internal prototypes
**
*/
static uint32_t calculateCRC(const uint8_t* data, uint32_t length);
static bool hasSpace(logBufferStruct* buffer, uint32_t needed);



void log_init(logSystemStruct* log) 
{
    log->maxLevel = LOG_LEVEL_INFO;
    
    for (int i = 0; i < LOG_LEVEL_COUNT; i++) 
    {
        log->buffers[i].writeIndex = 0;
        log->buffers[i].readIndex = 0;
        log->buffers[i].entryCount = 0;
    }
}


bool log_write(logSystemStruct* log, logLevelEnum level, const uint8_t* msg, uint32_t len) 
{
    if (level > log->maxLevel || len > LOG_MAX_ENTRY_SIZE) 
    {
        return false;
    }
    
    logBufferStruct* buffer = &log->buffers[level];
    uint32_t total_size = sizeof(logEntryHeaderStruct) + len;
    
    if (!has_space(buffer, total_size)) 
    {
        // Buffer full
        return false;
    }
    
    LogEntryHeader header;
    header.timestamp = /* get_system_timestamp() */ 0; // Implement based on your system
    header.msg_len = len;
    header.crc = calculate_crc(msg, len);
    
    // Write header
    uint32_t space_to_end = LOG_BUFFER_SIZE - buffer->write_index;
    if (space_to_end < sizeof(header)) {
        // Split header across buffer boundary
        uint32_t first_part = space_to_end;
        memcpy(&buffer->buffer[buffer->write_index], &header, first_part);
        memcpy(&buffer->buffer[0], (uint8_t*)&header + first_part, 
               sizeof(header) - first_part);
        buffer->write_index = sizeof(header) - first_part;
    } else {
        memcpy(&buffer->buffer[buffer->write_index], &header, sizeof(header));
        buffer->write_index += sizeof(header);
        if (buffer->write_index == LOG_BUFFER_SIZE) {
            buffer->write_index = 0;
        }
    }
    
    // Write message with potential wrap
    space_to_end = LOG_BUFFER_SIZE - buffer->write_index;
    if (space_to_end < len) {
        memcpy(&buffer->buffer[buffer->write_index], msg, space_to_end);
        memcpy(&buffer->buffer[0], msg + space_to_end, len - space_to_end);
        buffer->write_index = len - space_to_end;
    } else {
        memcpy(&buffer->buffer[buffer->write_index], msg, len);
        buffer->write_index += len;
        if (buffer->write_index == LOG_BUFFER_SIZE) {
            buffer->write_index = 0;
        }
    }
    
    buffer->entry_count++;
    return true;
}

bool log_read_next(LogSystem* log, LogLevel level, LogEntry* entry) {
    if (level >= LOG_LEVEL_COUNT) return false;
    
    LogBuffer* buffer = &log->buffers[level];
    if (buffer->read_index == buffer->write_index) {
        return false;  // No more entries
    }
    
    LogEntryHeader header;
    uint32_t space_to_end = LOG_BUFFER_SIZE - buffer->read_index;
    
    // Read header
    if (space_to_end < sizeof(header)) {
        memcpy(&header, &buffer->buffer[buffer->read_index], space_to_end);
        memcpy((uint8_t*)&header + space_to_end, &buffer->buffer[0], 
               sizeof(header) - space_to_end);
        buffer->read_index = sizeof(header) - space_to_end;
    } else {
        memcpy(&header, &buffer->buffer[buffer->read_index], sizeof(header));
        buffer->read_index += sizeof(header);
        if (buffer->read_index == LOG_BUFFER_SIZE) {
            buffer->read_index = 0;
        }
    }
    
    // Validate message length
    if (header.msg_len > LOG_MAX_ENTRY_SIZE) {
        return false;  // Corrupted entry
    }
    
    // Setup entry response
    entry->timestamp = header.timestamp;
    entry->level = level;
    entry->msg_len = header.msg_len;
    
    // For simplicity, we're returning a pointer into the buffer
    // Be aware this means the client needs to copy data before the next read
    entry->msg = &buffer->buffer[buffer->read_index];
    
    // Verify CRC
    uint32_t calculated_crc = calculate_crc(entry->msg, entry->msg_len);
    if (calculated_crc != header.crc) {
        return false;  // CRC mismatch
    }
    
    // Update read pointer
    buffer->read_index += header.msg_len;
    if (buffer->read_index >= LOG_BUFFER_SIZE) {
        buffer->read_index -= LOG_BUFFER_SIZE;
    }
    
    return true;
}

void log_read_reset(LogSystem* log, LogLevel level) {
    if (level < LOG_LEVEL_COUNT) {
        log->buffers[level].read_index = 0;
    }
}

uint32_t log_get_entry_count(LogSystem* log, LogLevel level) {
    if (level >= LOG_LEVEL_COUNT) return 0;
    return log->buffers[level].entry_count;
}

// Helper function implementations
bool log_write_uint32(LogSystem* log, LogLevel level, const uint8_t* prefix, uint32_t value) {
    uint8_t temp[LOG_MAX_ENTRY_SIZE];
    uint32_t prefix_len = strlen((const char*)prefix);
    
    if (prefix_len >= LOG_MAX_ENTRY_SIZE) return false;
    
    memcpy(temp, prefix, prefix_len);
    
    uint32_t pos = prefix_len;
    uint32_t val = value;
    uint32_t digits[10];
    uint32_t digit_count = 0;
    
    do {
        digits[digit_count++] = val % 10;
        val /= 10;
    } while (val > 0);
    
    if ((pos + digit_count) >= LOG_MAX_ENTRY_SIZE) return false;
    
    while (digit_count > 0) {
        temp[pos++] = '0' + digits[--digit_count];
    }
    
    return log_write(log, level, temp, pos);
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                         INTERNAL SUPPORT FUNCTIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

///
/// @fn: calculateCRC
///
/// @details Rudimentary CRC-32
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
static uint32_t calculateCRC(const uint8_t* data, uint32_t length) 
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < length; i++) 
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) 
        {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }

    return ~crc;
}

///
/// @fn: hasSpace
///
/// @details Is there space in the specified buffer for the message?
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
static bool hasSpace(logBufferStruct* buffer, uint32_t needed) 
{
    uint32_t            available;

    if (buffer->writeIndex >= buffer->readIndex) 
    {
        available = LOG_BUFFER_SIZE - buffer->write_index;

        if (buffer->readIndex > 0) 
        {
            available += buffer->readIndex - 1;
        }
    } 
    else 
    {
        available = buffer->readIndex - buffer->writeIndex - 1;
    }
    
    return available >= needed;
}