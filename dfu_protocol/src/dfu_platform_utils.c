//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_platform_utils.c
**
** DESCRIPTION: Utilities to help deal with platform-specific issues.
**
** REVISION HISTORY:
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include "dfu_platform_utils.h"



// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
//                             BYTE-ORDER UTILITIES
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

/*!
** FUNCTION: is_little_endian
**
** DESCRIPTION: Lets us know if the platform is little-endian or not.
**
** PARAMETERS: None
**
** RETURNS: 1 if little-endian, 0 if big-endian
**
** COMMENTS: Used by all conversion functions to determine if conversion is needed
*/
int is_little_endian(void)
{
    /*
    uint16_t x = 1;
    return *((uint8_t*)&x);  // Returns 1 if little-endian, 0 if big-endian
    */
  unsigned int x = 0x76543210;
  char *c = (char*) &x;

  if (*c == 0x10)
  {
      return (1);
  }

  return (0);
}

/*!
** FUNCTION: to_little_endian_16
**
** DESCRIPTION: Ensures that 16-bit integers are in little-endian format.
**
** PARAMETERS: value - The 16-bit value to convert
**
** RETURNS: The value in little-endian format
*/
uint16_t to_little_endian_16(uint16_t value)
{
    if (is_little_endian())
    {
        return value;  // Already in little-endian
    }
    else
    {
        return (value >> 8) | (value << 8);  // Swap byte order
    }
}

/*!
** FUNCTION: to_big_endian_16
**
** DESCRIPTION: Ensures that 16-bit integers are in big-endian format.
**
** PARAMETERS: value - The 16-bit value to convert
**
** RETURNS: The value in big-endian format
*/
uint16_t to_big_endian_16(uint16_t value)
{
    if (!is_little_endian())
    {
        return value;  // Already in big-endian
    }
    else
    {
        return (value >> 8) | (value << 8);  // Swap byte order
    }
}

/*!
** FUNCTION: from_little_endian_16
**
** DESCRIPTION: Converts 16-bit little-endian integers to the platform's endianness.
**
** PARAMETERS: value - The 16-bit value to convert from little-endian
**
** RETURNS: The value in platform's native endianness
*/
uint16_t from_little_endian_16(uint16_t value)
{
    return to_little_endian_16(value);  // Reuse the same logic
}

/*!
** FUNCTION: from_big_endian_16
**
** DESCRIPTION: Converts 16-bit big-endian integers to the platform's endianness.
**
** PARAMETERS: value - The 16-bit value to convert from big-endian
**
** RETURNS: The value in platform's native endianness
*/
uint16_t from_big_endian_16(uint16_t value)
{
    return to_big_endian_16(value);  // Reuse the same logic
}

/*!
** FUNCTION: to_little_endian_32
**
** DESCRIPTION: Ensures that 32-bit integers are in little-endian format.
**
** PARAMETERS: value - The 32-bit value to convert
**
** RETURNS: The value in little-endian format
*/
uint32_t to_little_endian_32(uint32_t value)
{
    if (is_little_endian())
    {
        return value;  // Already in little-endian
    }
    else
    {
        return ((value >> 24) & 0x000000FF) |
               ((value >> 8)  & 0x0000FF00) |
               ((value << 8)  & 0x00FF0000) |
               ((value << 24) & 0xFF000000);  // Swap byte order
    }
}

/*!
** FUNCTION: to_big_endian_32
**
** DESCRIPTION: Ensures that 32-bit integers are in big-endian format.
**
** PARAMETERS: value - The 32-bit value to convert
**
** RETURNS: The value in big-endian format
*/
uint32_t to_big_endian_32(uint32_t value)
{
    if (!is_little_endian())
    {
        return value;  // Already in big-endian
    }
    else
    {
        return ((value >> 24) & 0x000000FF) |
               ((value >> 8)  & 0x0000FF00) |
               ((value << 8)  & 0x00FF0000) |
               ((value << 24) & 0xFF000000);  // Swap byte order
    }
}

/*!
** FUNCTION: from_little_endian_32
**
** DESCRIPTION: Converts 32-bit little-endian integers to the platform's endianness.
**
** PARAMETERS: value - The 32-bit value to convert from little-endian
**
** RETURNS: The value in platform's native endianness
*/
uint32_t from_little_endian_32(uint32_t value)
{
    return to_little_endian_32(value);
}

/*!
** FUNCTION: from_big_endian_32
**
** DESCRIPTION: Converts 32-bit big-endian integers to the platform's endianness.
**
** PARAMETERS: value - The 32-bit value to convert from big-endian
**
** RETURNS: The value in platform's native endianness
*/
uint32_t from_big_endian_32(uint32_t value)
{
    return to_big_endian_32(value);
}

/*!
** FUNCTION: to_little_endian_64
**
** DESCRIPTION: Ensures that 64-bit integers are in little-endian format.
**
** PARAMETERS: value - The 64-bit value to convert
**
** RETURNS: The value in little-endian format
*/
uint64_t to_little_endian_64(uint64_t value)
{
    if (is_little_endian())
    {
        return value;
    }
    else
    {
        return ((value >> 56) & 0x00000000000000FF) |
               ((value >> 40) & 0x000000000000FF00) |
               ((value >> 24) & 0x0000000000FF0000) |
               ((value >> 8)  & 0x00000000FF000000) |
               ((value << 8)  & 0x000000FF00000000) |
               ((value << 24) & 0x0000FF0000000000) |
               ((value << 40) & 0x00FF000000000000) |
               ((value << 56) & 0xFF00000000000000);
    }
}

/*!
** FUNCTION: to_big_endian_64
**
** DESCRIPTION: Ensures that 64-bit integers are in big-endian format.
**
** PARAMETERS: value - The 64-bit value to convert
**
** RETURNS: The value in big-endian format
*/
uint64_t to_big_endian_64(uint64_t value)
{
    if (!is_little_endian())
    {
        return value;
    }
    else
    {
        return ((value >> 56) & 0x00000000000000FF) |
               ((value >> 40) & 0x000000000000FF00) |
               ((value >> 24) & 0x0000000000FF0000) |
               ((value >> 8)  & 0x00000000FF000000) |
               ((value << 8)  & 0x000000FF00000000) |
               ((value << 24) & 0x0000FF0000000000) |
               ((value << 40) & 0x00FF000000000000) |
               ((value << 56) & 0xFF00000000000000);
    }
}

/*!
** FUNCTION: from_little_endian_64
**
** DESCRIPTION: Converts 64-bit little-endian integers to the platform's endianness.
**
** PARAMETERS: value - The 64-bit value to convert from little-endian
**
** RETURNS: The value in platform's native endianness
*/
uint64_t from_little_endian_64(uint64_t value)
{
    return to_little_endian_64(value);
}

/*!
** FUNCTION: from_big_endian_64
**
** DESCRIPTION: Converts 64-bit big-endian integers to the platform's endianness.
**
** PARAMETERS: value - The 64-bit value to convert from big-endian
**
** RETURNS: The value in platform's native endianness
*/
uint64_t from_big_endian_64(uint64_t value)
{
    return to_big_endian_64(value);
}

/*!
** FUNCTION: to_little_endian_float
**
** DESCRIPTION: Ensures that single-precision floats are in little-endian format.
**
** PARAMETERS: value - The float value to convert
**
** RETURNS: The value in little-endian format
*/
float to_little_endian_float(float value)
{
    uint32_t int_repr;
    float result;

    memcpy(&int_repr, &value, sizeof(float));
    int_repr = to_little_endian_32(int_repr);
    memcpy(&result, &int_repr, sizeof(float));

    return result;
}

/*!
** FUNCTION: to_big_endian_float
**
** DESCRIPTION: Ensures that single-precision floats are in big-endian format.
**
** PARAMETERS: value - The float value to convert
**
** RETURNS: The value in big-endian format
*/
float to_big_endian_float(float value)
{
    uint32_t int_repr;
    float result;

    memcpy(&int_repr, &value, sizeof(float));
    int_repr = to_big_endian_32(int_repr);
    memcpy(&result, &int_repr, sizeof(float));

    return result;
}

/*!
** FUNCTION: from_little_endian_float
**
** DESCRIPTION: Converts single-precision floats from little-endian to platform endianness.
**
** PARAMETERS: value - The float value to convert from little-endian
**
** RETURNS: The value in platform's native endianness
*/
float from_little_endian_float(float value)
{
    return to_little_endian_float(value);
}

/*!
** FUNCTION: from_big_endian_float
**
** DESCRIPTION: Converts single-precision floats from big-endian to platform endianness.
**
** PARAMETERS: value - The float value to convert from big-endian
**
** RETURNS: The value in platform's native endianness
*/
float from_big_endian_float(float value)
{
    return to_big_endian_float(value);
}

/*!
** FUNCTION: to_little_endian_double
**
** DESCRIPTION: Ensures that double-precision floats are in little-endian format.
**
** PARAMETERS: value - The double value to convert
**
** RETURNS: The value in little-endian format
*/
double to_little_endian_double(double value)
{
    uint64_t int_repr;
    double result;

    memcpy(&int_repr, &value, sizeof(double));
    int_repr = to_little_endian_64(int_repr);
    memcpy(&result, &int_repr, sizeof(double));

    return result;
}

/*!
** FUNCTION: to_big_endian_double
**
** DESCRIPTION: Ensures that double-precision floats are in big-endian format.
**
** PARAMETERS: value - The double value to convert
**
** RETURNS: The value in big-endian format
*/
double to_big_endian_double(double value)
{
    uint64_t int_repr;
    double result;

    memcpy(&int_repr, &value, sizeof(double));
    int_repr = to_big_endian_64(int_repr);
    memcpy(&result, &int_repr, sizeof(double));

    return result;
}

/*!
** FUNCTION: from_little_endian_double
**
** DESCRIPTION: Converts double-precision floats from little-endian to platform endianness.
**
** PARAMETERS: value - The double value to convert from little-endian
**
** RETURNS: The value in platform's native endianness
*/
double from_little_endian_double(double value)
{
    return to_little_endian_double(value);
}

/*!
** FUNCTION: from_big_endian_double
**
** DESCRIPTION: Converts double-precision floats from big-endian to platform endianness.
**
** PARAMETERS: value - The double value to convert from big-endian
**
** RETURNS: The value in platform's native endianness
*/
double from_big_endian_double(double value)
{
    return to_big_endian_double(value);
}
