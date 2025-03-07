/*
** miniYaml.h - Minimal YAML parser for embedded systems
** 
** Features:
** - No dynamic memory allocation
** - No standard C string formatting functions
** - Fixed buffer sizes configurable at compile time
** - Supports basic YAML subset (scalars, mappings, sequences)
** - Minimal dependencies
*/

#ifndef MINI_YAML_H
#define MINI_YAML_H

#include <stdint.h>
#include <stdbool.h>

// Configurable parameters - adjust based on your system constraints
#ifndef YAML_MAX_DEPTH
#define YAML_MAX_DEPTH 8            // Maximum nesting level
#endif

#ifndef YAML_MAX_KEY_LENGTH
#define YAML_MAX_KEY_LENGTH 32      // Maximum key length
#endif

#ifndef YAML_MAX_VALUE_LENGTH
#define YAML_MAX_VALUE_LENGTH 64    // Maximum value length
#endif

#ifndef YAML_MAX_ITEMS
#define YAML_MAX_ITEMS 32           // Maximum items in a mapping or sequence
#endif

// Error codes
typedef enum 
{
    YAML_SUCCESS = 0,
    YAML_ERROR_MALFORMED,
    YAML_ERROR_DEPTH_EXCEEDED,
    YAML_ERROR_BUFFER_OVERFLOW,
    YAML_ERROR_ITEMS_EXCEEDED,
    YAML_ERROR_INVALID_TYPE
} yamlError_t;

// Value types
typedef enum 
{
    YAML_TYPE_NONE = 0,
    YAML_TYPE_NULL,
    YAML_TYPE_SCALAR,
    YAML_TYPE_MAPPING,
    YAML_TYPE_SEQUENCE
} yamlType_t;

// Forward declarations for recursive structures
typedef struct yamlNode_s yamlNode_t;
typedef struct yamlMapping_s yamlMapping_t;
typedef struct yamlSequence_s yamlSequence_t;

// Key-value pair for mappings
typedef struct 
{
    char key[YAML_MAX_KEY_LENGTH];
    yamlNode_t* value;
} yamlPair_t;

// Mapping structure (key-value pairs)
struct yamlMapping_s 
{
    yamlPair_t items[YAML_MAX_ITEMS];
    uint8_t count;
};

// Sequence structure (ordered list)
struct yamlSequence_s 
{
    yamlNode_t* items[YAML_MAX_ITEMS];
    uint8_t count;
};

// Node structure (represents any YAML value)
struct yamlNode_s 
{
    yamlType_t type;
    union 
    {
        char scalar[YAML_MAX_VALUE_LENGTH];
        yamlMapping_t mapping;
        yamlSequence_t sequence;
    } data;
};

// Parser context
typedef struct 
{
    const char* buffer;
    uint16_t position;
    uint16_t length;
    uint8_t line;
    uint8_t column;
    uint8_t currentDepth;
    
    // Static memory pool for nodes
    yamlNode_t nodes[YAML_MAX_ITEMS * 2];
    uint8_t nodeCount;
} yamlParser_t;

// Initialize parser with input buffer
void yamlParserInit(yamlParser_t* parser, const char* buffer, uint16_t length);

// Parse YAML document into root node
yamlError_t yamlParse(yamlParser_t* parser, yamlNode_t** root);

// Helper functions for accessing parsed data
yamlNode_t* yamlMappingGet(const yamlMapping_t* mapping, const char* key);
yamlNode_t* yamlSequenceGet(const yamlSequence_t* sequence, uint8_t index);
bool yamlIsScalar(const yamlNode_t* node);
bool yamlIsMapping(const yamlNode_t* node);
bool yamlIsSequence(const yamlNode_t* node);
bool yamlIsNull(const yamlNode_t* node);
const char* yamlGetScalar(const yamlNode_t* node);

// Utility functions for specific scalar types
bool yamlScalarToBool(const yamlNode_t* node, bool* value);
yamlError_t yamlScalarToInt(const yamlNode_t* node, int32_t* value);
yamlError_t yamlScalarToUint(const yamlNode_t* node, uint32_t* value);

// Get error description
const char* yamlErrorString(yamlError_t error);

#endif // MINI_YAML_H