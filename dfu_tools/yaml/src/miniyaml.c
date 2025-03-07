/*
** miniYaml.c - Implementation of minimal YAML parser
*/

#include "miniYaml.h"

// Character classification helpers - avoid ctype.h dependency
static bool isSpace(char c)
{
    return c == ' ' || c == '\t';
}

static bool isNewline(char c)
{
    return c == '\n' || c == '\r';
}

static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool isAlpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// String utilities that avoid string.h
static uint16_t yamlStrlen(const char* str)
{
    uint16_t len = 0;
    while (str[len] != '\0')
    {
        len++;
    }
    return len;
}

static bool yamlStrcmp(const char* s1, const char* s2)
{
    while (*s1 && *s2 && *s1 == *s2)
    {
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

static void yamlStrncpy(char* dst, const char* src, uint16_t n)
{
    uint16_t i;
    for (i = 0; i < n - 1 && src[i] != '\0'; i++)
    {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

// Allocate a node from the static pool
static yamlNode_t* yamlAllocNode(yamlParser_t* parser)
{
    if (parser->nodeCount >= YAML_MAX_ITEMS * 2)
    {
        return NULL;
    }
    
    yamlNode_t* node = &parser->nodes[parser->nodeCount++];
    node->type = YAML_TYPE_NONE;
    return node;
}

// Parser helper functions
static bool yamlIsEof(const yamlParser_t* parser)
{
    return parser->position >= parser->length;
}

static char yamlCurrentChar(const yamlParser_t* parser)
{
    if (yamlIsEof(parser))
    {
        return '\0';
    }
    return parser->buffer[parser->position];
}

static void yamlAdvance(yamlParser_t* parser)
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

static void yamlSkipWhitespace(yamlParser_t* parser)
{
    while (!yamlIsEof(parser) && isSpace(yamlCurrentChar(parser)))
    {
        yamlAdvance(parser);
    }
}

static void yamlSkipLine(yamlParser_t* parser)
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

static uint8_t yamlGetIndentLevel(yamlParser_t* parser)
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

static yamlError_t yamlParseScalar(yamlParser_t* parser, yamlNode_t* node)
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

// Forward declaration for mutual recursion
static yamlError_t yamlParseSequence(yamlParser_t* parser, yamlNode_t* node, uint8_t indent);

static yamlError_t yamlParseMapping(yamlParser_t* parser, yamlNode_t* node, uint8_t indent)
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
        yamlNode_t* valueNode = yamlAllocNode(parser);
        if (valueNode == NULL)
        {
            return YAML_ERROR_ITEMS_EXCEEDED;
        }
        
        // Check if the value is on the same line
        if (!isNewline(yamlCurrentChar(parser)) && yamlCurrentChar(parser) != '#')
        {
            // Parse inline value (scalar)
            yamlError_t err = yamlParseScalar(parser, valueNode);
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
                    yamlError_t err = yamlParseSequence(parser, valueNode, nextIndent);
                    parser->currentDepth--;
                    if (err != YAML_SUCCESS)
                    {
                        return err;
                    }
                }
                else
                {
                    // This is a nested mapping
                    yamlError_t err = yamlParseMapping(parser, valueNode, nextIndent);
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

static yamlError_t yamlParseSequence(yamlParser_t* parser, yamlNode_t* node, uint8_t indent)
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
        yamlNode_t* itemNode = yamlAllocNode(parser);
        if (itemNode == NULL)
        {
            return YAML_ERROR_ITEMS_EXCEEDED;
        }
        
        // Parse the item value
        if (!isNewline(yamlCurrentChar(parser)) && yamlCurrentChar(parser) != '#')
        {
            // Inline scalar
            yamlError_t err = yamlParseScalar(parser, itemNode);
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
                    
                    yamlError_t err = yamlParseSequence(parser, itemNode, nextIndent);
                    parser->currentDepth--;
                    if (err != YAML_SUCCESS)
                    {
                        return err;
                    }
                }
                else
                {
                    // Nested mapping
                    yamlError_t err = yamlParseMapping(parser, itemNode, nextIndent);
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

// Public API implementation
void yamlParserInit(yamlParser_t* parser, const char* buffer, uint16_t length)
{
    parser->buffer = buffer;
    parser->position = 0;
    parser->length = length;
    parser->line = 1;
    parser->column = 1;
    parser->currentDepth = 0;
    parser->nodeCount = 0;
}

yamlError_t yamlParse(yamlParser_t* parser, yamlNode_t** root)
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

// Helper functions for accessing parsed data
yamlNode_t* yamlMappingGet(const yamlMapping_t* mapping, const char* key)
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

yamlNode_t* yamlSequenceGet(const yamlSequence_t* sequence, uint8_t index)
{
    if (index < sequence->count)
    {
        return sequence->items[index];
    }
    return NULL;
}

bool yamlIsScalar(const yamlNode_t* node)
{
    return node != NULL && node->type == YAML_TYPE_SCALAR;
}

bool yamlIsMapping(const yamlNode_t* node)
{
    return node != NULL && node->type == YAML_TYPE_MAPPING;
}

bool yamlIsSequence(const yamlNode_t* node)
{
    return node != NULL && node->type == YAML_TYPE_SEQUENCE;
}

bool yamlIsNull(const yamlNode_t* node)
{
    return node == NULL || node->type == YAML_TYPE_NULL;
}

const char* yamlGetScalar(const yamlNode_t* node)
{
    if (yamlIsScalar(node))
    {
        return node->data.scalar;
    }
    return NULL;
}

// Utility functions for specific scalar types
bool yamlScalarToBool(const yamlNode_t* node, bool* value)
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

yamlError_t yamlScalarToInt(const yamlNode_t* node, int32_t* value)
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

yamlError_t yamlScalarToUint(const yamlNode_t* node, uint32_t* value)
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

// Get error description
const char* yamlErrorString(yamlError_t error)
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