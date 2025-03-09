//#############################################################################
//#############################################################################
//#############################################################################
///
/// @copyright 2024, 2025, 2026 Glydways, Inc
/// @copyright https://glydways.com
///
/// @file miniYaml.c
/// @brief 
/// @details 
///
//#############################################################################
//#############################################################################
//#############################################################################
#include "miniYaml.h"

/*
** Internal support function prototypes.
**
*/
static bool isSpace(char c);
static bool isNewline(char c);
static bool isDigit(char c);
static bool isAlpha(char c);
static uint16_t yamlStrlen(const char* str);
static bool yamlStrcmp(const char* s1, const char* s2);
static void yamlStrncpy(char* dst, const char* src, uint16_t n);
static yamlNodeStruct* yamlAllocNode(yamlParserStruct* parser);
static bool yamlIsEof(const yamlParserStruct* parser);
static char yamlCurrentChar(const yamlParserStruct* parser);
static void yamlAdvance(yamlParserStruct* parser);
static void yamlSkipWhitespace(yamlParserStruct* parser);
static void yamlSkipLine(yamlParserStruct* parser);
static uint8_t yamlGetIndentLevel(yamlParserStruct* parser);
static yamlErrorEnum yamlParseScalar(yamlParserStruct* parser, yamlNodeStruct* node);
static yamlErrorEnum yamlParseSequence(yamlParserStruct* parser, yamlNodeStruct* node, uint8_t indent);
static yamlErrorEnum yamlParseMapping(yamlParserStruct* parser, yamlNodeStruct* node, uint8_t indent);

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                        PUBLIC API IMPLEMENTATIONS
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
void yamlParserInit(yamlParserStruct* parser, const char* buffer, uint16_t length)
{
    parser->buffer = buffer;
    parser->position = 0;
    parser->length = length;
    parser->line = 1;
    parser->column = 1;
    parser->currentDepth = 0;
    parser->nodeCount = 0;
}

///
/// @fn: yamlParse
/// @details Parses a YAML document and builds a node tree
///
/// @param[in/out] parser Pointer to an initialized parser structure
/// @param[out] root Double pointer to receive the root node of the parsed document
///
/// @returns YAML_SUCCESS on successful parsing or an error code
///
yamlErrorEnum yamlParse(yamlParserStruct* parser, yamlNodeStruct** root)
{
    // Allocate root node
    *root = yamlAllocNode(parser);
    if (*root == NULL)
    {
        return YAML_ERROR_ITEMS_EXCEEDED;
    }
    
    // Skip initial whitespace and comments
    while (!yamlIsEof(parser))
    {
        yamlSkipWhitespace(parser);
        
        if (yamlCurrentChar(parser) == '#')
        {
            yamlSkipLine(parser);
            continue;
        }
        
        if (isNewline(yamlCurrentChar(parser)))
        {
            yamlAdvance(parser);
            continue;
        }
        
        break;
    }
    
    // Determine document type
    if (yamlIsEof(parser))
    {
        // Empty document
        (*root)->type = YAML_TYPE_MAPPING;
        (*root)->data.mapping.count = 0;
        return YAML_SUCCESS;
    }
    
    // Check for document start marker
    if (parser->position + 2 < parser->length &&
        parser->buffer[parser->position] == '-' &&
        parser->buffer[parser->position + 1] == '-' &&
        parser->buffer[parser->position + 2] == '-')
    {
        yamlSkipLine(parser);
        
        // Skip whitespace after document marker
        while (!yamlIsEof(parser))
        {
            yamlSkipWhitespace(parser);
            
            if (yamlCurrentChar(parser) == '#')
            {
                yamlSkipLine(parser);
                continue;
            }
            
            if (isNewline(yamlCurrentChar(parser)))
            {
                yamlAdvance(parser);
                continue;
            }
            
            break;
        }
    }
    
    // Determine root node type
    if (yamlCurrentChar(parser) == '-')
    {
        // Root is a sequence
        yamlAdvance(parser); // Skip the dash
        yamlSkipWhitespace(parser);
        
        (*root)->type = YAML_TYPE_SEQUENCE;
        (*root)->data.sequence.count = 0;
        
        return yamlParseSequence(parser, *root, 0);
    }
    else
    {
        // Root is a mapping
        return yamlParseMapping(parser, *root, 0);
    }
}

///
/// @fn: yamlMappingGet
/// @details Retrieves a node from a mapping by key
///
/// @param[in] mapping Pointer to the mapping structure to search
/// @param[in] key The key string to look for in the mapping
///
/// @returns Pointer to the found node or NULL if not found
///
yamlNodeStruct* yamlMappingGet(const yamlMappingStruct* mapping, const char* key)
{
    uint8_t i;
    for (i = 0; i < mapping->count; i++)
    {
        if (yamlStrcmp(mapping->items[i].key, key))
        {
            return mapping->items[i].value;
        }
    }
    return NULL;
}

///
/// @fn: yamlSequenceGet
/// @details Retrieves a node from a sequence by index
///
/// @param[in] sequence Pointer to the sequence structure
/// @param[in] index Zero-based index of the item to retrieve
///
/// @returns Pointer to the node at the given index or NULL if index is out of bounds
///
yamlNodeStruct* yamlSequenceGet(const yamlSequenceStruct* sequence, uint8_t index)
{
    if (index < sequence->count)
    {
        return sequence->items[index];
    }
    return NULL;
}

///
/// @fn: yamlIsScalar
/// @details Checks if a node is a scalar value
///
/// @param[in] node Pointer to the node to check
///
/// @returns true if the node is a scalar, false otherwise
///
bool yamlIsScalar(const yamlNodeStruct* node)
{
    return node != NULL && node->type == YAML_TYPE_SCALAR;
}

///
/// @fn: yamlIsMapping
/// @details Checks if a node is a mapping (key-value pairs)
///
/// @param[in] node Pointer to the node to check
///
/// @returns true if the node is a mapping, false otherwise
///
bool yamlIsMapping(const yamlNodeStruct* node)
{
    return node != NULL && node->type == YAML_TYPE_MAPPING;
}

///
/// @fn: yamlIsSequence
/// @details Checks if a node is a sequence (array)
///
/// @param[in] node Pointer to the node to check
///
/// @returns true if the node is a sequence, false otherwise
///
bool yamlIsSequence(const yamlNodeStruct* node)
{
    return node != NULL && node->type == YAML_TYPE_SEQUENCE;
}

///
/// @fn: yamlIsNull
/// @details Checks if a node is null or has the null type
///
/// @param[in] node Pointer to the node to check
///
/// @returns true if the node is null, false otherwise
///
bool yamlIsNull(const yamlNodeStruct* node)
{
    return node == NULL || node->type == YAML_TYPE_NULL;
}

///
/// @fn: yamlGetScalar
/// @details Gets the string value of a scalar node
///
/// @param[in] node Pointer to the node to get the value from
///
/// @returns Pointer to the scalar string or NULL if the node is not a scalar
///
const char* yamlGetScalar(const yamlNodeStruct* node)
{
    if (yamlIsScalar(node))
    {
        return node->data.scalar;
    }
    return NULL;
}

///
/// @fn: yamlScalarToBool
/// @details Converts a scalar node to a boolean value
///
/// @param[in] node Pointer to the scalar node
/// @param[out] value Pointer to store the resulting boolean value
///
/// @returns true if conversion was successful, false otherwise
///
bool yamlScalarToBool(const yamlNodeStruct* node, bool* value)
{
    if (!yamlIsScalar(node))
    {
        return false;
    }
    
    const char* scalar = node->data.scalar;
    
    if (yamlStrcmp(scalar, "true") || 
        yamlStrcmp(scalar, "yes") ||
        yamlStrcmp(scalar, "on") ||
        yamlStrcmp(scalar, "1"))
    {
        *value = true;
        return true;
    }
    
    if (yamlStrcmp(scalar, "false") || 
        yamlStrcmp(scalar, "no") ||
        yamlStrcmp(scalar, "off") ||
        yamlStrcmp(scalar, "0"))
    {
        *value = false;
        return true;
    }
    
    return false;
}

///
/// @fn: yamlScalarToInt
/// @details Converts a scalar node to a signed 32-bit integer
///
/// @param[in] node Pointer to the scalar node
/// @param[out] value Pointer to store the resulting integer value
///
/// @returns YAML_SUCCESS on successful conversion or an error code
///
yamlErrorEnum yamlScalarToInt(const yamlNodeStruct* node, int32_t* value)
{
    if (!yamlIsScalar(node))
    {
        return YAML_ERROR_INVALID_TYPE;
    }
    
    const char* scalar = node->data.scalar;
    bool negative = false;
    uint16_t i = 0;
    
    // Check for sign
    if (scalar[0] == '-')
    {
        negative = true;
        i++;
    }
    else if (scalar[0] == '+')
    {
        i++;
    }
    
    // Ensure we have at least one digit
    if (!isDigit(scalar[i]))
    {
        return YAML_ERROR_MALFORMED;
    }
    
    // Parse digits
    int32_t result = 0;
    while (scalar[i] != '\0')
    {
        if (!isDigit(scalar[i]))
        {
            return YAML_ERROR_MALFORMED;
        }
        
        // Check for overflow
        if (result > INT32_MAX / 10)
        {
            return YAML_ERROR_MALFORMED;
        }
        
        result = result * 10 + (scalar[i] - '0');
        i++;
    }
    
    *value = negative ? -result : result;
    return YAML_SUCCESS;
}

///
/// @fn: yamlScalarToUint
/// @details Converts a scalar node to an unsigned 32-bit integer
///
/// @param[in] node Pointer to the scalar node
/// @param[out] value Pointer to store the resulting unsigned integer value
///
/// @returns YAML_SUCCESS on successful conversion or an error code
///
yamlErrorEnum yamlScalarToUint(const yamlNodeStruct* node, uint32_t* value)
{
    if (!yamlIsScalar(node))
    {
        return YAML_ERROR_INVALID_TYPE;
    }
    
    const char* scalar = node->data.scalar;
    uint16_t i = 0;
    
    // Check for sign (must be positive)
    if (scalar[0] == '+')
    {
        i++;
    }
    else if (scalar[0] == '-')
    {
        return YAML_ERROR_MALFORMED;
    }
    
    // Ensure we have at least one digit
    if (!isDigit(scalar[i]))
    {
        return YAML_ERROR_MALFORMED;
    }
    
    // Parse digits
    uint32_t result = 0;
    while (scalar[i] != '\0')
    {
        if (!isDigit(scalar[i]))
        {
            return YAML_ERROR_MALFORMED;
        }
        
        // Check for overflow
        if (result > UINT32_MAX / 10)
        {
            return YAML_ERROR_MALFORMED;
        }
        
        result = result * 10 + (scalar[i] - '0');
        i++;
    }
    
    *value = result;
    return YAML_SUCCESS;
}

///
/// @fn: yamlErrorString
/// @details Converts a YAML error code to a human-readable string
///
/// @param[in] error The error code to convert
///
/// @returns Pointer to a constant string describing the error
///
const char* yamlErrorString(yamlErrorEnum error)
{
    switch (error)
    {
        case YAML_SUCCESS:
            return "Success";
        case YAML_ERROR_MALFORMED:
            return "Malformed YAML";
        case YAML_ERROR_DEPTH_EXCEEDED:
            return "Maximum nesting depth exceeded";
        case YAML_ERROR_BUFFER_OVERFLOW:
            return "Buffer overflow";
        case YAML_ERROR_ITEMS_EXCEEDED:
            return "Maximum items exceeded";
        case YAML_ERROR_INVALID_TYPE:
            return "Invalid type";
        default:
            return "Unknown error";
    }
}

// Static function implementations

///
/// @fn: isSpace
/// @details Checks if a character is a space or tab (whitespace)
///
/// @param[in] c Character to check
///
/// @returns true if the character is a space or tab, false otherwise
///
static bool isSpace(char c)
{
    return c == ' ' || c == '\t';
}

///
/// @fn: isNewline
/// @details Checks if a character is a newline character
///
/// @param[in] c Character to check
///
/// @returns true if the character is a newline character, false otherwise
///
static bool isNewline(char c)
{
    return c == '\n' || c == '\r';
}

///
/// @fn: isDigit
/// @details Checks if a character is a decimal digit
///
/// @param[in] c Character to check
///
/// @returns true if the character is a digit, false otherwise
///
static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

///
/// @fn: isAlpha
/// @details Checks if a character is an alphabetic character
///
/// @param[in] c Character to check
///
/// @returns true if the character is alphabetic, false otherwise
///
static bool isAlpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

///
/// @fn: yamlStrlen
/// @details Calculates the length of a null-terminated string
///
/// @param[in] str Pointer to the null-terminated string
///
/// @returns Length of the string in characters
///
static uint16_t yamlStrlen(const char* str)
{
    uint16_t len = 0;
    while (str[len] != '\0')
    {
        len++;
    }
    return len;
}

///
/// @fn: yamlStrcmp
/// @details Compares two null-terminated strings
///
/// @param[in] s1 Pointer to the first string
/// @param[in] s2 Pointer to the second string
///
/// @returns true if strings are equal, false otherwise
///
static bool yamlStrcmp(const char* s1, const char* s2)
{
    while (*s1 && *s2 && *s1 == *s2)
    {
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

///
/// @fn: yamlStrncpy
/// @details Copies at most n-1 characters from source to destination
///
/// @param[out] dst Pointer to the destination buffer
/// @param[in] src Pointer to the source string
/// @param[in] n Maximum number of characters to copy including null terminator
///
/// @returns None
///
static void yamlStrncpy(char* dst, const char* src, uint16_t n)
{
    uint16_t i;
    for (i = 0; i < n - 1 && src[i] != '\0'; i++)
    {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

///
/// @fn: yamlAllocNode
/// @details Allocates a node from the static node pool
///
/// @param[in/out] parser Pointer to the parser with the node pool
///
/// @returns Pointer to the allocated node or NULL if pool is exhausted
///
static yamlNodeStruct* yamlAllocNode(yamlParserStruct* parser)
{
    if (parser->nodeCount >= YAML_MAX_ITEMS * 2)
    {
        return NULL;
    }
    
    yamlNodeStruct* node = &parser->nodes[parser->nodeCount++];
    node->type = YAML_TYPE_NONE;
    return node;
}

///
/// @fn: yamlIsEof
/// @details Checks if the parser has reached the end of the buffer
///
/// @param[in] parser Pointer to the parser
///
/// @returns true if the parser has reached the end of the buffer, false otherwise
///
static bool yamlIsEof(const yamlParserStruct* parser)
{
    return parser->position >= parser->length;
}

///
/// @fn: yamlCurrentChar
/// @details Gets the current character at the parser's position
///
/// @param[in] parser Pointer to the parser
///
/// @returns Current character or '\0' if at the end of the buffer
///
static char yamlCurrentChar(const yamlParserStruct* parser)
{
    if (yamlIsEof(parser))
    {
        return '\0';
    }
    return parser->buffer[parser->position];
}

///
/// @fn: yamlAdvance
/// @details Advances the parser position by one character and updates line/column tracking
///
/// @param[in/out] parser Pointer to the parser
///
/// @returns None
///
static void yamlAdvance(yamlParserStruct* parser)
{
    if (yamlIsEof(parser))
    {
        return;
    }
    
    char c = parser->buffer[parser->position++];
    
    if (c == '\n')
    {
        parser->line++;
        parser->column = 0;
    }
    else
    {
        parser->column++;
    }
}

///
/// @fn: yamlSkipWhitespace
/// @details Advances the parser position past any spaces and tabs
///
/// @param[in/out] parser Pointer to the parser
///
/// @returns None
///
static void yamlSkipWhitespace(yamlParserStruct* parser)
{
    while (!yamlIsEof(parser) && isSpace(yamlCurrentChar(parser)))
    {
        yamlAdvance(parser);
    }
}

///
/// @fn: yamlSkipLine
/// @details Advances the parser position to the beginning of the next line
///
/// @param[in/out] parser Pointer to the parser
///
/// @returns None
///
static void yamlSkipLine(yamlParserStruct* parser)
{
    while (!yamlIsEof(parser) && !isNewline(yamlCurrentChar(parser)))
    {
        yamlAdvance(parser);
    }
    
    if (!yamlIsEof(parser))
    {
        yamlAdvance(parser); // Skip the newline character
    }
}

///
/// @fn: yamlGetIndentLevel
/// @details Calculates the indentation level (in spaces) at the current position
///
/// @param[in/out] parser Pointer to the parser
///
/// @returns Number of spaces of indentation or 0 for empty/comment lines
///
static uint8_t yamlGetIndentLevel(yamlParserStruct* parser)
{
    uint8_t indent = 0;
    uint16_t startPos = parser->position;
    
    while (!yamlIsEof(parser) && isSpace(yamlCurrentChar(parser)))
    {
        if (yamlCurrentChar(parser) == ' ')
        {
            indent++;
        }
        else if (yamlCurrentChar(parser) == '\t')
        {
            indent += 4; // Count tab as 4 spaces
        }
        yamlAdvance(parser);
    }
    
    // If we reached end of line or a comment, reset position and return 0
    if (yamlIsEof(parser) || isNewline(yamlCurrentChar(parser)) || 
        yamlCurrentChar(parser) == '#')
    {
        parser->position = startPos;
        return 0;
    }
    
    return indent;
}

///
/// @fn: yamlParseScalar
/// @details Parses a scalar value (string, number, boolean, null)
///
/// @param[in/out] parser Pointer to the parser
/// @param[out] node Pointer to the node to store the scalar value
///
/// @returns YAML_SUCCESS or an error code
///
static yamlErrorEnum yamlParseScalar(yamlParserStruct* parser, yamlNodeStruct* node)
{
    uint16_t start = parser->position;
    uint16_t length = 0;
    char quoteChar = '\0';
    
    // Check if this is a quoted string
    if (yamlCurrentChar(parser) == '"' || yamlCurrentChar(parser) == '\'')
    {
        quoteChar = yamlCurrentChar(parser);
        yamlAdvance(parser);
        start = parser->position;
        
        // Read until closing quote
        while (!yamlIsEof(parser) && yamlCurrentChar(parser) != quoteChar)
        {
            // Handle escape sequences in double quotes
            if (quoteChar == '"' && yamlCurrentChar(parser) == '\\' && 
                parser->position + 1 < parser->length)
            {
                yamlAdvance(parser);
            }
            yamlAdvance(parser);
            length++;
            
            if (length >= YAML_MAX_VALUE_LENGTH - 1)
            {
                return YAML_ERROR_BUFFER_OVERFLOW;
            }
        }
        
        // Consume closing quote
        if (!yamlIsEof(parser) && yamlCurrentChar(parser) == quoteChar)
        {
            yamlAdvance(parser);
        }
        else
        {
            return YAML_ERROR_MALFORMED;
        }
    }
    else
    {
        // Unquoted scalar - read until end of line, comment, or illegal character
        while (!yamlIsEof(parser) && 
               !isNewline(yamlCurrentChar(parser)) && 
               yamlCurrentChar(parser) != '#' &&
               yamlCurrentChar(parser) != ':' &&
               yamlCurrentChar(parser) != ',')
        {
            yamlAdvance(parser);
            length++;
            
            if (length >= YAML_MAX_VALUE_LENGTH - 1)
            {
                return YAML_ERROR_BUFFER_OVERFLOW;
            }
        }
        
        // Trim trailing spaces
        while (length > 0 && isSpace(parser->buffer[start + length - 1]))
        {
            length--;
        }
    }
    
    // Check for null value
    if (length == 4 && 
        parser->buffer[start] == 'n' && 
        parser->buffer[start+1] == 'u' && 
        parser->buffer[start+2] == 'l' && 
        parser->buffer[start+3] == 'l')
    {
        node->type = YAML_TYPE_NULL;
    }
    else
    {
        node->type = YAML_TYPE_SCALAR;
        yamlStrncpy(node->data.scalar, &parser->buffer[start], length + 1);
        node->data.scalar[length] = '\0';
    }
    
    return YAML_SUCCESS;
}

///
/// @fn: yamlParseMapping
/// @details Parses a YAML mapping (key-value pairs) node
///
/// @param[in/out] parser Pointer to the parser
/// @param[out] node Pointer to the node to store the mapping
/// @param[in] indent Indentation level of the mapping
///
/// @returns YAML_SUCCESS or an error code
///
static yamlErrorEnum yamlParseMapping(yamlParserStruct* parser, yamlNodeStruct* node, uint8_t indent)
{
    node->type = YAML_TYPE_MAPPING;
    node->data.mapping.count = 0;
    
    while (!yamlIsEof(parser))
    {
        uint8_t currentIndent = yamlGetIndentLevel(parser);
        
        // If we encounter a line with less indent, we're done with this mapping
        if (currentIndent < indent)
        {
            return YAML_SUCCESS;
        }
        
        // Skip empty lines and comments
        if (isNewline(yamlCurrentChar(parser)) || yamlCurrentChar(parser) == '#')
        {
            yamlSkipLine(parser);
            continue;
        }
        
        // Parse the key
        uint16_t keyStart = parser->position;
        uint16_t keyLength = 0;
        
        while (!yamlIsEof(parser) && yamlCurrentChar(parser) != ':' && 
               !isNewline(yamlCurrentChar(parser)))
        {
            yamlAdvance(parser);
            keyLength++;
            
            if (keyLength >= YAML_MAX_KEY_LENGTH - 1)
            {
                return YAML_ERROR_BUFFER_OVERFLOW;
            }
        }
        
        // Trim trailing spaces from key
        while (keyLength > 0 && isSpace(parser->buffer[keyStart + keyLength - 1]))
        {
            keyLength--;
        }
        
        if (keyLength == 0 || yamlCurrentChar(parser) != ':')
        {
            return YAML_ERROR_MALFORMED;
        }
        
        // Check if we've exceeded maximum items
        if (node->data.mapping.count >= YAML_MAX_ITEMS)
        {
            return YAML_ERROR_ITEMS_EXCEEDED;
        }
        
        // Store the key
        yamlStrncpy(node->data.mapping.items[node->data.mapping.count].key, 
                   &parser->buffer[keyStart], keyLength + 1);
        node->data.mapping.items[node->data.mapping.count].key[keyLength] = '\0';
        
        // Consume the colon
        yamlAdvance(parser);
        yamlSkipWhitespace(parser);
        
        // Allocate a node for the value
        yamlNodeStruct* valueNode = yamlAllocNode(parser);
        if (valueNode == NULL)
        {
            return YAML_ERROR_ITEMS_EXCEEDED;
        }
        
        // Check if the value is on the same line
        if (!isNewline(yamlCurrentChar(parser)) && yamlCurrentChar(parser) != '#')
        {
            // Parse inline value (scalar)
            yamlErrorEnum err = yamlParseScalar(parser, valueNode);
            if (err != YAML_SUCCESS)
            {
                return err;
            }
        }
        else
        {
            // Skip to the next line
            yamlSkipLine(parser);
            
            // Check for nested block
            uint8_t nextIndent = yamlGetIndentLevel(parser);
            
            if (nextIndent > indent)
            {
                // Increment depth counter
                parser->currentDepth++;
                if (parser->currentDepth >= YAML_MAX_DEPTH)
                {
                    return YAML_ERROR_DEPTH_EXCEEDED;
                }
                
                // Determine if this is a nested mapping or sequence
                if (!yamlIsEof(parser) && yamlCurrentChar(parser) == '-')
                {
                    // This is a sequence
                    yamlAdvance(parser); // Skip the dash
                    yamlSkipWhitespace(parser);
                    
                    valueNode->type = YAML_TYPE_SEQUENCE;
                    valueNode->data.sequence.count = 0;
                    
                    // Parse sequence items
                    yamlErrorEnum err = yamlParseSequence(parser, valueNode, nextIndent);
                    parser->currentDepth--;
                    if (err != YAML_SUCCESS)
                    {
                        return err;
                    }
                }
                else
                {
                    // This is a nested mapping
                    yamlErrorEnum err = yamlParseMapping(parser, valueNode, nextIndent);
                    parser->currentDepth--;
                    if (err != YAML_SUCCESS)
                    {
                        return err;
                    }
                }
            }
            else
            {
                // Empty value or value is on next line with same/less indentation
                valueNode->type = YAML_TYPE_NULL;
            }
        }
        
        // Store the value and increment count
        node->data.mapping.items[node->data.mapping.count].value = valueNode;
        node->data.mapping.count++;
        
        // Skip to next line if needed
        if (!isNewline(yamlCurrentChar(parser)) && !yamlIsEof(parser))
        {
            yamlSkipLine(parser);
        }
    }
    
    return YAML_SUCCESS;
}

///
/// @fn: yamlParseSequence
/// @details Parses a YAML sequence (array) node
///
/// @param[in/out] parser Pointer to the parser
/// @param[out] node Pointer to the node to store the sequence
/// @param[in] indent Indentation level of the sequence
///
/// @returns YAML_SUCCESS or an error code
///
static yamlErrorEnum yamlParseSequence(yamlParserStruct* parser, yamlNodeStruct* node, uint8_t indent)
{
    node->type = YAML_TYPE_SEQUENCE;
    node->data.sequence.count = 0;
    
    bool firstItem = true;
    uint8_t itemIndent = indent;
    
    while (!yamlIsEof(parser))
    {
        yamlSkipWhitespace(parser);
        
        uint8_t currentIndent = yamlGetIndentLevel(parser);
        
        // If we encounter a line with less indent, we're done with this sequence
        if (currentIndent < indent)
        {
            return YAML_SUCCESS;
        }
        
        // Skip empty lines and comments
        if (isNewline(yamlCurrentChar(parser)) || yamlCurrentChar(parser) == '#')
        {
            yamlSkipLine(parser);
            continue;
        }
        
        // Check for sequence item marker
        if (yamlCurrentChar(parser) != '-')
        {
            // If this isn't a sequence item and we're not at the beginning,
            // we're done with this sequence
            if (!firstItem)
            {
                return YAML_SUCCESS;
            }
            return YAML_ERROR_MALFORMED;
        }
        
        // Consume the dash
        yamlAdvance(parser);
        
        // Must have at least one space after dash
        if (!isSpace(yamlCurrentChar(parser)))
        {
            return YAML_ERROR_MALFORMED;
        }
        
        // Skip whitespace after dash
        yamlSkipWhitespace(parser);
        
        // Remember indent level of first item for consistency check
        if (firstItem)
        {
            itemIndent = currentIndent;
            firstItem = false;
        }
        else if (currentIndent != itemIndent)
        {
            // All items must have same indentation
            return YAML_ERROR_MALFORMED;
        }
        
        // Check if we've exceeded maximum items
        if (node->data.sequence.count >= YAML_MAX_ITEMS)
        {
            return YAML_ERROR_ITEMS_EXCEEDED;
        }
        
        // Allocate a node for this item
        yamlNodeStruct* itemNode = yamlAllocNode(parser);
        if (itemNode == NULL)
        {
            return YAML_ERROR_ITEMS_EXCEEDED;
        }
        
        // Parse the item value
        if (!isNewline(yamlCurrentChar(parser)) && yamlCurrentChar(parser) != '#')
        {
            // Inline scalar
            yamlErrorEnum err = yamlParseScalar(parser, itemNode);
            if (err != YAML_SUCCESS)
            {
                return err;
            }
        }
        else
        {
            // Skip to next line for block content
            yamlSkipLine(parser);
            
            uint8_t nextIndent = yamlGetIndentLevel(parser);
            
            if (nextIndent > itemIndent)
            {
                // Increment depth counter
                parser->currentDepth++;
                if (parser->currentDepth >= YAML_MAX_DEPTH)
                {
                    return YAML_ERROR_DEPTH_EXCEEDED;
                }
                
                // Check for nested sequence or mapping
                if (!yamlIsEof(parser) && yamlCurrentChar(parser) == '-')
                {
                    // Nested sequence
                    yamlAdvance(parser); // Skip the dash
                    yamlSkipWhitespace(parser);
                    
                    itemNode->type = YAML_TYPE_SEQUENCE;
                    itemNode->data.sequence.count = 0;
                    
                    yamlErrorEnum err = yamlParseSequence(parser, itemNode, nextIndent);
                    parser->currentDepth--;
                    if (err != YAML_SUCCESS)
                    {
                        return err;
                    }
                }
                else
                {
                    // Nested mapping
                    yamlErrorEnum err = yamlParseMapping(parser, itemNode, nextIndent);
                    parser->currentDepth--;
                    if (err != YAML_SUCCESS)
                    {
                        return err;
                    }
                }
            }
            else
            {
                // Empty value
                itemNode->type = YAML_TYPE_NULL;
            }
        }
        
        // Store the item and increment count
        node->data.sequence.items[node->data.sequence.count] = itemNode;
        node->data.sequence.count++;
        
        // Skip to next line if needed
        if (!isNewline(yamlCurrentChar(parser)) && !yamlIsEof(parser))
        {
            yamlSkipLine(parser);
        }
    }
    
    return YAML_SUCCESS;
}
