#pragma once
#include <cstdint>

/**
 * @file CryptoUtils.h
 * @brief Pure-math utility functions for keypad code encryption.
 *
 * All functions are stateless and hardware-independent, which makes them
 * unit-testable in a native (non-ESP32) environment.
 */

/**
 * @brief Extended Euclidean Algorithm — O(log n) modular multiplicative inverse.
 *
 * Returns x such that (a * x) % m == 1, or 0 if no inverse exists (gcd(a,m) != 1).
 * Replaces the original O(n) brute-force loop.
 *
 * Typical iteration count for the default parameters
 *   (multiplier=73, modulus=1,000,000): ~9 iterations.
 */
inline uint32_t modInverse(uint32_t a, uint32_t m)
{
    if (m <= 1) return 0;

    int64_t old_r = (int64_t)a,  r = (int64_t)m;
    int64_t old_s = 1,           s = 0;

    while (r != 0)
    {
        int64_t q   = old_r / r;
        int64_t tmp = r;   r   = old_r - q * r;   old_r = tmp;
                    tmp = s;   s   = old_s - q * s;   old_s = tmp;
    }

    if (old_r != 1) return 0;           // gcd(a, m) != 1 — no inverse exists
    if (old_s < 0)  old_s += (int64_t)m;
    return (uint32_t)old_s;
}

/**
 * @brief Encrypt a keypad code using an affine cipher.
 *
 * Formula: ((code * multiplier + offset) % modulus) + modulus
 *
 * The + modulus ensures the result is always distinguishable from a raw code.
 */
inline uint32_t encryptKeypadCode(uint32_t code,
                                   uint32_t multiplier,
                                   uint32_t offset,
                                   uint32_t modulus)
{
    return ((code * multiplier + offset) % modulus) + modulus;
}

/**
 * @brief Decrypt a keypad code that was encrypted with encryptKeypadCode().
 *
 * @param encryptedCode  The encrypted code (>= modulus).
 * @param inverse        Modular inverse of multiplier (from modInverse()).
 * @param offset         The same offset used during encryption.
 * @param modulus        The same modulus used during encryption.
 * @return               Original code, or 0 if inverse is 0 (invalid parameters).
 */
inline uint32_t decryptKeypadCode(uint32_t encryptedCode,
                                   uint32_t inverse,
                                   uint32_t offset,
                                   uint32_t modulus)
{
    if (inverse == 0) return 0;

    // BUG FIX: the original code computed (encryptedCode - offset) * inverse,
    // which overflows uint32_t for any realistic code value (encryptedCode is
    // always >= modulus, so encryptedCode - offset ≈ modulus ≈ 1,000,000 and
    // inverse ≈ 630,137 → product ≈ 6.3 × 10^11, far beyond 2^32).
    //
    // Correct approach:
    //   1. Reduce (encryptedCode - offset) mod modulus first — safe, fits uint32_t.
    //   2. Widen to uint64_t for the multiplication to prevent overflow.
    //
    // Note: (encryptedCode - offset) is always >= 0 because encryptKeypadCode()
    // guarantees encryptedCode >= modulus and modulus > offset.
    const uint32_t shifted = (encryptedCode - offset) % modulus;
    return static_cast<uint32_t>(
        (static_cast<uint64_t>(shifted) * static_cast<uint64_t>(inverse)) % modulus
    );
}