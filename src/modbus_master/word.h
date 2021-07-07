/**
@file
Utility Functions for Manipulating Words

@defgroup util_word "util/word.h": Utility Functions for Manipulating Words
@code#include "util/word.h"@endcode

This header file provides utility functions for manipulating words.

*/
/*

  word.h - Utility Functions for Manipulating Words

  This file is part of ModbusMaster.

  ModbusMaster is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  ModbusMaster is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with ModbusMaster.  If not, see <http://www.gnu.org/licenses/>.

  Written by Doc Walker (Rx)
  Copyright © 2009-2015 Doc Walker <4-20ma at wvfans dot net>

*/

#ifndef _UTIL_WORD_H_
#define _UTIL_WORD_H_

#include <stdint.h>

/** @ingroup util_word
    Return low word of a 32-bit integer.

    @param uint32_t ww (0x00000000..0xFFFFFFFF)
    @return low word of input (0x0000..0xFFFF)
*/
static inline uint16_t word_get_low(uint32_t ww)
{
    return (uint16_t)((ww)&0xFFFF);
}

/** @ingroup util_word
    Return high word of a 32-bit integer.

    @param uint32_t ww (0x00000000..0xFFFFFFFF)
    @return high word of input (0x0000..0xFFFF)
*/
static inline uint16_t word_get_high(uint32_t ww)
{
    return (uint16_t)((ww) >> 16);
}

static inline uint8_t word_get_low_byte(uint16_t ww)
{
    return (uint8_t)((ww)&0x00FF);
}

static inline uint8_t word_get_high_byte(uint16_t ww)
{
    return (uint8_t)((ww) >> 8);
}

static inline uint16_t word_make(uint8_t h_byte, uint8_t l_byte)
{
    uint16_t word;
    word = (uint16_t)(h_byte << 8);
    word = word + l_byte;
    return word;
}

#define WORD_BIT_SET(value, bit) ((value) |= (1UL << (bit)))
#define WORD_BIT_CLR(value, bit) ((value) &= ~(1UL << (bit)))

#define WORD_BIT_WRITE(value, bit, bitvalue) (bitvalue ? WORD_BIT_SET(value, bit) : WORD_BIT_CLR(value, bit))
#define WORD_BIT_READ(value, bit) (((value) >> (bit)) & 0x01)

#endif /* _UTIL_WORD_H_ */
