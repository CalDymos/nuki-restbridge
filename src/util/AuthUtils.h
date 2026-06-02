#pragma once
#include <cstring>
#include <cstdint>

/**
 * @file AuthUtils.h
 * @brief Authentication utility functions.
 *
 * All functions are stateless and hardware-independent, making them
 * unit-testable in a native (non-ESP32) environment.
 */

/**
 * @brief Constant-time string comparison.
 *
 * Unlike strcmp(), this function always iterates the full length of the longer
 * string. This prevents timing-based token reconstruction attacks where an
 * attacker measures response time to guess the token character by character.
 *
 * Note: strlen() calls at the start do leak length information, but since the
 * token length is fixed at 20 characters this is acceptable.
 *
 * @param a  First string (e.g. the submitted token).
 * @param b  Second string (e.g. the stored token).
 * @return   true only if both strings are identical in length and content.
 */
inline bool constTimeStrEqual(const char* a, const char* b)
{
    const size_t lenA = strlen(a);
    const size_t lenB = strlen(b);
    const size_t maxLen = (lenA > lenB) ? lenA : lenB;

    uint8_t diff = (uint8_t)(lenA ^ lenB); // non-zero if lengths differ

    for (size_t i = 0; i < maxLen; i++)
    {
        uint8_t ca = (i < lenA) ? (uint8_t)a[i] : 0;
        uint8_t cb = (i < lenB) ? (uint8_t)b[i] : 0;
        diff |= ca ^ cb;
    }
    return diff == 0;
}