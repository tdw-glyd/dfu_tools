//#############################################################################
//#############################################################################
//#############################################################################
///
/// @copyright 2024, 2025, 2026 Glydways, Inc
/// @copyright https://glydways.com
///
/// @file miniYaml.h
/// @brief Lightweight YAML parser for embedded systems
/// @details A small, memory-efficient YAML parser with no external dependencies,
///          designed for resource-constrained environments.
///
//#############################################################################
//#############################################################################
//#############################################################################

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configuration constants - adjust these as needed
#define YAML_MAX_KEY_LENGTH    64      // Max length of mapping keys
#define YAML_MAX_VALUE_LENGTH  256     // Max length of scalar values
#define YAML_MAX_ITEMS         32      // Max items in a mapping or sequence
#define YAML_MAX_DEPTH         8       // Max nesting depth

// Error codes
typedef enum
{
    YAML_SUCCESS = 0,
    YAML_ERROR_MALFORMED,
    YAML_ERROR_DEPTH_EXCEEDED,
    YAML_ERROR_BUFFER_OVERFLOW,
    YAML_ERROR_ITEMS_EXCEEDED,
    YAML_ERROR_INVALID_TYPE
} yamlErrorEnum;

// Node types
typedef enum
{
    YAML_TYPE_NONE = 0,
    YAML_TYPE_SCALAR,
    YAML_TYPE_MAPPING,
    YAML_TYPE_SEQUENCE,
    YAML_TYPE_NULL
} yamlNodeTypeEnum;

// Forward declarations for recursive structures
typedef struct yamlNode_s yamlNodeStruct;

// Sequence type
typedef struct
{
    uint8_t count;
    yamlNodeStruct* items[YAML_MAX_ITEMS];
} yamlSequenceStruct;

// Mapping key-value pair
typedef struct
{
    char key[YAML_MAX_KEY_LENGTH];
    yamlNodeStruct* value;
} yamlKeyValueStruct;

// Mapping type
typedef struct
{
    uint8_t count;
    yamlKeyValueStruct items[YAML_MAX_ITEMS];
} yamlMappingStruct;

// Node structure
struct yamlNode_s
{
    yamlNodeTypeEnum type;
    union
    {
        char scalar[YAML_MAX_VALUE_LENGTH];
        yamlMappingStruct mapping;
        yamlSequenceStruct sequence;
    } data;
};

// Parser structure
typedef struct
{
    const char* buffer;
    uint16_t position;
    uint16_t length;
    uint16_t line;
    uint16_t column;
    uint8_t currentDepth;
    uint16_t nodeCount;
    yamlNodeStruct nodes[YAML_MAX_ITEMS * 2]; // Node pool
} yamlParserStruct;

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                         PUBLIC API DECLARATIONS
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

///
/// @fn: yamlParserInit
/// @details Initializes a YAML parser with the given buffer
///
/// @param[out] parser Pointer to the parser structure to initialize
/// @param[in] buffer Pointer to the YAML text buffer to parse
/// @param[in] length Length of the buffer in bytes
///
/// @returns None
///
void yamlParserInit(yamlParserStruct* parser, const char* buffer, uint16_t length);

///
/// @fn: yamlParse
/// @details Parses a YAML document and builds a node tree
///
/// @param[in/out] parser Pointer to an initialized parser structure
/// @param[out] root Double pointer to receive the root node of the parsed document
///
/// @returns YAML_SUCCESS on successful parsing or an error code
///
yamlErrorEnum yamlParse(yamlParserStruct* parser, yamlNodeStruct** root);

///
/// @fn: yamlMappingGet
/// @details Retrieves a node from a mapping by key
///
/// @param[in] mapping Pointer to the mapping structure to search
/// @param[in] key The key string to look for in the mapping
///
/// @returns Pointer to the found node or NULL if not found
///
yamlNodeStruct* yamlMappingGet(const yamlMappingStruct* mapping, const char* key);

///
/// @fn: yamlSequenceGet
/// @details Retrieves a node from a sequence by index
///
/// @param[in] sequence Pointer to the sequence structure
/// @param[in] index Zero-based index of the item to retrieve
///
/// @returns Pointer to the node at the given index or NULL if index is out of bounds
///
yamlNodeStruct* yamlSequenceGet(const yamlSequenceStruct* sequence, uint8_t index);

///
/// @fn: yamlIsScalar
/// @details Checks if a node is a scalar value
///
/// @param[in] node Pointer to the node to check
///
/// @returns true if the node is a scalar, false otherwise
///
bool yamlIsScalar(const yamlNodeStruct* node);

///
/// @fn: yamlIsMapping
/// @details Checks if a node is a mapping (key-value pairs)
///
/// @param[in] node Pointer to the node to check
///
/// @returns true if the node is a mapping, false otherwise
///
bool yamlIsMapping(const yamlNodeStruct* node);

///
/// @fn: yamlIsSequence
/// @details Checks if a node is a sequence (array)
///
/// @param[in] node Pointer to the node to check
///
/// @returns true if the node is a sequence, false otherwise
///
bool yamlIsSequence(const yamlNodeStruct* node);

///
/// @fn: yamlIsNull
/// @details Checks if a node is null or has the null type
///
/// @param[in] node Pointer to the node to check
///
/// @returns true if the node is null, false otherwise
///
bool yamlIsNull(const yamlNodeStruct* node);

///
/// @fn: yamlGetScalar
/// @details Gets the string value of a scalar node
///
/// @param[in] node Pointer to the node to get the value from
///
/// @returns Pointer to the scalar string or NULL if the node is not a scalar
///
const char* yamlGetScalar(const yamlNodeStruct* node);

///
/// @fn: yamlScalarToBool
/// @details Converts a scalar node to a boolean value
///
/// @param[in] node Pointer to the scalar node
/// @param[out] value Pointer to store the resulting boolean value
///
/// @returns true if conversion was successful, false otherwise
///
bool yamlScalarToBool(const yamlNodeStruct* node, bool* value);

///
/// @fn: yamlScalarToInt
/// @details Converts a scalar node to a signed 32-bit integer
///
/// @param[in] node Pointer to the scalar node
/// @param[out] value Pointer to store the resulting integer value
///
/// @returns YAML_SUCCESS on successful conversion or an error code
///
yamlErrorEnum yamlScalarToInt(const yamlNodeStruct* node, int32_t* value);

///
/// @fn: yamlScalarToUint
/// @details Converts a scalar node to an unsigned 32-bit integer
///
/// @param[in] node Pointer to the scalar node
/// @param[out] value Pointer to store the resulting unsigned integer value
///
/// @returns YAML_SUCCESS on successful conversion or an error code
///
yamlErrorEnum yamlScalarToUint(const yamlNodeStruct* node, uint32_t* value);

///
/// @fn: yamlErrorString
/// @details Converts a YAML error code to a human-readable string
///
/// @param[in] error The error code to convert
///
/// @returns Pointer to a constant string describing the error
///
const char* yamlErrorString(yamlErrorEnum error);

#ifdef __cplusplus
}
#endif