/**
 * @file test_auth.cpp
 * @brief Unit tests for authentication utilities (util/AuthUtils.h).
 *
 * Run natively (no ESP32 hardware needed):
 *   pio test -e native
 */

#include <unity.h>
#include "util/AuthUtils.h"

// ── constTimeStrEqual ─────────────────────────────────────────────────────────

void test_equal_strings_return_true()
{
    TEST_ASSERT_TRUE(constTimeStrEqual("abc123", "abc123"));
}

void test_different_strings_return_false()
{
    TEST_ASSERT_FALSE(constTimeStrEqual("abc123", "abc124"));
}

void test_empty_strings_are_equal()
{
    TEST_ASSERT_TRUE(constTimeStrEqual("", ""));
}

void test_empty_vs_nonempty_returns_false()
{
    TEST_ASSERT_FALSE(constTimeStrEqual("", "a"));
    TEST_ASSERT_FALSE(constTimeStrEqual("a", ""));
}

void test_different_lengths_return_false()
{
    TEST_ASSERT_FALSE(constTimeStrEqual("abc", "abcd"));
    TEST_ASSERT_FALSE(constTimeStrEqual("abcd", "abc"));
}

void test_prefix_match_is_not_equal()
{
    // "token" must not match "token_extra"
    TEST_ASSERT_FALSE(constTimeStrEqual("token", "token_extra"));
}

void test_20_char_token_match()
{
    // Typical API token length
    const char* tok = "abcdefghij0123456789";
    TEST_ASSERT_TRUE(constTimeStrEqual(tok, tok));
    TEST_ASSERT_TRUE(constTimeStrEqual("abcdefghij0123456789",
                                       "abcdefghij0123456789"));
}

void test_20_char_token_last_char_differs()
{
    TEST_ASSERT_FALSE(constTimeStrEqual("abcdefghij0123456789",
                                        "abcdefghij012345678x"));
}

void test_20_char_token_first_char_differs()
{
    TEST_ASSERT_FALSE(constTimeStrEqual("abcdefghij0123456789",
                                        "xbcdefghij0123456789"));
}

void test_single_char_equal()
{
    TEST_ASSERT_TRUE(constTimeStrEqual("a", "a"));
}

void test_single_char_different()
{
    TEST_ASSERT_FALSE(constTimeStrEqual("a", "b"));
}

void test_case_sensitive()
{
    TEST_ASSERT_FALSE(constTimeStrEqual("Token", "token"));
    TEST_ASSERT_FALSE(constTimeStrEqual("TOKEN", "token"));
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char **argv)
{
    UNITY_BEGIN();

    RUN_TEST(test_equal_strings_return_true);
    RUN_TEST(test_different_strings_return_false);
    RUN_TEST(test_empty_strings_are_equal);
    RUN_TEST(test_empty_vs_nonempty_returns_false);
    RUN_TEST(test_different_lengths_return_false);
    RUN_TEST(test_prefix_match_is_not_equal);
    RUN_TEST(test_20_char_token_match);
    RUN_TEST(test_20_char_token_last_char_differs);
    RUN_TEST(test_20_char_token_first_char_differs);
    RUN_TEST(test_single_char_equal);
    RUN_TEST(test_single_char_different);
    RUN_TEST(test_case_sensitive);

    return UNITY_END();
}
