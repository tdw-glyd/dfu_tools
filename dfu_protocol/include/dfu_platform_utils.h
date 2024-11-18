//#############################################################################
//#############################################################################
//#############################################################################
/*! \file
**
** MODULE: dfu_platform_utils.h
**
** DESCRIPTION: DFU platform utilities
**
** REVISION HISTORY: 
**
*/
//#############################################################################
//#############################################################################
//#############################################################################
#pragma once

#include <stdint.h>







#if defined(__cplusplus)
extern "C" {
#endif

/*!
** FUNCTION: is_little_endian
**
** DESCRIPTION: Lets us know if the platform is little-endian or not.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
int is_little_endian(void);

/*!
** FUNCTION: to_little_endian_16
**
** DESCRIPTION: Ensures that 16-bit integers are in little-endian
**              format.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint16_t to_little_endian_16(uint16_t value);

/*!
** FUNCTION: to_big_endian_16
**
** DESCRIPTION: Ensures that 16-bit integers are in big-endian format.
**
** PARAMETERS: value - The 16-bit value to convert
**
** RETURNS: The value in big-endian format
*/
uint16_t to_big_endian_16(uint16_t value);

/*!
** FUNCTION: from_little_endian_16
**
** DESCRIPTION: Converts 16-bit little-endian integers to the platform's 
**              endianness.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint16_t from_little_endian_16(uint16_t value);

/*!
** FUNCTION: from_big_endian_16
**
** DESCRIPTION: Converts 16-bit big-endian integers to the platform's endianness.
**
** PARAMETERS: value - The 16-bit value to convert from big-endian
**
** RETURNS: The value in platform's native endianness
*/
uint16_t from_big_endian_16(uint16_t value);

/*!
** FUNCTION: to_little_endian_32
**
** DESCRIPTION: Ensure that 32-bit integers are in little-endian format 
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint32_t to_little_endian_32(uint32_t value);

/*!
** FUNCTION: to_big_endian_32
**
** DESCRIPTION: Ensures that 32-bit integers are in big-endian format.
**
** PARAMETERS: value - The 32-bit value to convert
**
** RETURNS: The value in big-endian format
*/
uint32_t to_big_endian_32(uint32_t value);

/*!
** FUNCTION: from_little_endian_32
**
** DESCRIPTION: Converts 32-bit little-endian integers to the platform's 
**              endianness.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint32_t from_little_endian_32(uint32_t value);

/*!
** FUNCTION: from_big_endian_32
**
** DESCRIPTION: Converts 32-bit big-endian integers to the platform's endianness.
**
** PARAMETERS: value - The 32-bit value to convert from big-endian
**
** RETURNS: The value in platform's native endianness
*/
uint32_t from_big_endian_32(uint32_t value);

/*!
** FUNCTION: to_little_endian_64 
**
** DESCRIPTION: Ensures that 64-bit integers are in little-endian format.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint64_t to_little_endian_64(uint64_t value);

/*!
** FUNCTION: to_big_endian_64
**
** DESCRIPTION: Ensures that 64-bit integers are in big-endian format.
**
** PARAMETERS: value - The 64-bit value to convert
**
** RETURNS: The value in big-endian format
*/
uint64_t to_big_endian_64(uint64_t value);

/*!
** FUNCTION: from_little_endian_64
**
** DESCRIPTION: Converts 64-bit little-endian integers to the platform's 
**              endianness.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
uint64_t from_little_endian_64(uint64_t value);

/*!
** FUNCTION: from_big_endian_64
**
** DESCRIPTION: Converts 64-bit big-endian integers to the platform's endianness.
**
** PARAMETERS: value - The 64-bit value to convert from big-endian
**
** RETURNS: The value in platform's native endianness
*/
uint64_t from_big_endian_64(uint64_t value);

/*!
** FUNCTION: to_little_endian_float
**
** DESCRIPTION: Ensures that single-precisions floats are in little-endian format.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
float to_little_endian_float(float value);

/*!
** FUNCTION: to_big_endian_float
**
** DESCRIPTION: Ensures that single-precision floats are in big-endian format.
**
** PARAMETERS: value - The float value to convert
**
** RETURNS: The value in big-endian format
*/
float to_big_endian_float(float value);

/*!
** FUNCTION: from_little_endian_float
**
** DESCRIPTION: Converts single-precision floats from little-endian to platform endianness.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
float from_little_endian_float(float value);

/*!
** FUNCTION: from_big_endian_float
**
** DESCRIPTION: Converts single-precision floats from big-endian to platform endianness.
**
** PARAMETERS: value - The float value to convert from big-endian
**
** RETURNS: The value in platform's native endianness
*/
float from_big_endian_float(float value);

/*!
** FUNCTION: to_little_endian_double 
**
** DESCRIPTION: Ensures that double-precision floats are in little-endian format.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
double to_little_endian_double(double value);

/*!
** FUNCTION: to_big_endian_double
**
** DESCRIPTION: Ensures that double-precision floats are in big-endian format.
**
** PARAMETERS: value - The double value to convert
**
** RETURNS: The value in big-endian format
*/
double to_big_endian_double(double value);

/*!
** FUNCTION: from_little_endian_double
**
** DESCRIPTION: Converts double-precision floats from little-endian to platform endianness.
**
** PARAMETERS: 
**
** RETURNS: 
**
** COMMENTS: 
**
*/
double from_little_endian_double(double value);

/*!
** FUNCTION: from_big_endian_double
**
** DESCRIPTION: Converts double-precision floats from big-endian to platform endianness.
**
** PARAMETERS: value - The double value to convert from big-endian
**
** RETURNS: The value in platform's native endianness
*/
double from_big_endian_double(double value);

#if defined(__cplusplus)
}
#endif