/**
 * @file test_keypad_crypto.cpp
 * @brief Unit tests for keypad code encryption / decryption (util/CryptoUtils.h).
 *
 * Run natively (no ESP32 hardware needed):
 *   pio test -e native
 */

#include <unity.h>
#include "util/CryptoUtils.h"

// ── Default parameters (from PreferencesKeys.h defaults) ─────────────────────
static constexpr uint32_t DEFAULT_MULTIPLIER = 73;
static constexpr uint32_t DEFAULT_OFFSET     = 12345;
static constexpr uint32_t DEFAULT_MODULUS    = 1000000;

// ── modInverse ────────────────────────────────────────────────────────────────

void test_modInverse_default_params()
{
    // 73 * x ≡ 1 (mod 1,000,000)
    uint32_t inv = modInverse(DEFAULT_MULTIPLIER, DEFAULT_MODULUS);
    TEST_ASSERT_NOT_EQUAL(0, inv);
    TEST_ASSERT_EQUAL_UINT32(1, (DEFAULT_MULTIPLIER * inv) % DEFAULT_MODULUS);
}

void test_modInverse_simple()
{
    // 3 * 7 = 21 ≡ 1 (mod 10)  — textbook example
    TEST_ASSERT_EQUAL_UINT32(7, modInverse(3, 10));
}

void test_modInverse_no_inverse()
{
    // gcd(4, 8) = 4 != 1 → no inverse
    TEST_ASSERT_EQUAL_UINT32(0, modInverse(4, 8));
}

void test_modInverse_modulus_one()
{
    // m=1: everything is 0 mod 1, no meaningful inverse
    TEST_ASSERT_EQUAL_UINT32(0, modInverse(5, 1));
}

void test_modInverse_modulus_zero()
{
    TEST_ASSERT_EQUAL_UINT32(0, modInverse(5, 0));
}

// ── encryptKeypadCode / decryptKeypadCode ─────────────────────────────────────

void test_encrypt_decrypt_roundtrip_default()
{
    uint32_t inv  = modInverse(DEFAULT_MULTIPLIER, DEFAULT_MODULUS);
    uint32_t code = 123456;

    uint32_t enc = encryptKeypadCode(code, DEFAULT_MULTIPLIER, DEFAULT_OFFSET, DEFAULT_MODULUS);
    uint32_t dec = decryptKeypadCode(enc,  inv,                DEFAULT_OFFSET, DEFAULT_MODULUS);

    TEST_ASSERT_EQUAL_UINT32(code, dec);
}

void test_encrypt_result_exceeds_modulus()
{
    // Encrypted code must be >= modulus (as per the + modulus in the formula)
    uint32_t enc = encryptKeypadCode(123456, DEFAULT_MULTIPLIER, DEFAULT_OFFSET, DEFAULT_MODULUS);
    TEST_ASSERT_GREATER_OR_EQUAL(DEFAULT_MODULUS, enc);
}

void test_encrypt_decrypt_boundary_zero()
{
    uint32_t inv = modInverse(DEFAULT_MULTIPLIER, DEFAULT_MODULUS);
    uint32_t enc = encryptKeypadCode(0, DEFAULT_MULTIPLIER, DEFAULT_OFFSET, DEFAULT_MODULUS);
    uint32_t dec = decryptKeypadCode(enc, inv, DEFAULT_OFFSET, DEFAULT_MODULUS);
    TEST_ASSERT_EQUAL_UINT32(0, dec);
}

void test_encrypt_decrypt_boundary_max()
{
    // Maximum valid 6-digit code
    uint32_t inv  = modInverse(DEFAULT_MULTIPLIER, DEFAULT_MODULUS);
    uint32_t code = DEFAULT_MODULUS - 1;  // 999999

    uint32_t enc = encryptKeypadCode(code, DEFAULT_MULTIPLIER, DEFAULT_OFFSET, DEFAULT_MODULUS);
    uint32_t dec = decryptKeypadCode(enc,  inv,                DEFAULT_OFFSET, DEFAULT_MODULUS);

    TEST_ASSERT_EQUAL_UINT32(code, dec);
}

void test_decrypt_returns_zero_if_no_inverse()
{
    // If inverse == 0 (invalid params), decrypt must return 0 safely
    uint32_t result = decryptKeypadCode(1234567, 0, DEFAULT_OFFSET, DEFAULT_MODULUS);
    TEST_ASSERT_EQUAL_UINT32(0, result);
}

void test_encrypt_different_codes_produce_different_results()
{
    uint32_t enc1 = encryptKeypadCode(100000, DEFAULT_MULTIPLIER, DEFAULT_OFFSET, DEFAULT_MODULUS);
    uint32_t enc2 = encryptKeypadCode(100001, DEFAULT_MULTIPLIER, DEFAULT_OFFSET, DEFAULT_MODULUS);
    TEST_ASSERT_NOT_EQUAL(enc1, enc2);
}

void test_roundtrip_all_sample_codes()
{
    uint32_t inv       = modInverse(DEFAULT_MULTIPLIER, DEFAULT_MODULUS);
    uint32_t samples[] = { 1, 1000, 12345, 99999, 500000, 999998 };

    for (uint32_t code : samples)
    {
        uint32_t enc = encryptKeypadCode(code, DEFAULT_MULTIPLIER, DEFAULT_OFFSET, DEFAULT_MODULUS);
        uint32_t dec = decryptKeypadCode(enc,  inv,                DEFAULT_OFFSET, DEFAULT_MODULUS);
        TEST_ASSERT_EQUAL_UINT32(code, dec);
    }
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char **argv)
{
    UNITY_BEGIN();

    RUN_TEST(test_modInverse_default_params);
    RUN_TEST(test_modInverse_simple);
    RUN_TEST(test_modInverse_no_inverse);
    RUN_TEST(test_modInverse_modulus_one);
    RUN_TEST(test_modInverse_modulus_zero);

    RUN_TEST(test_encrypt_decrypt_roundtrip_default);
    RUN_TEST(test_encrypt_result_exceeds_modulus);
    RUN_TEST(test_encrypt_decrypt_boundary_zero);
    RUN_TEST(test_encrypt_decrypt_boundary_max);
    RUN_TEST(test_decrypt_returns_zero_if_no_inverse);
    RUN_TEST(test_encrypt_different_codes_produce_different_results);
    RUN_TEST(test_roundtrip_all_sample_codes);

    return UNITY_END();
}
