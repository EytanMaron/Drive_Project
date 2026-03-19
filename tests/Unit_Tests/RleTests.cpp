#include "RleCompressor.h"
#include <string>
#include <gtest/gtest.h>

// --- Tests for RleCompressor (Task DP-14) ---

// Test 1: compressing empty string
TEST(RleCompressorTests, CompressEmptyString) {
    RleCompressor compressor;
    std::string result = compressor.compress("");
    EXPECT_EQ(result, "");
}


// Test 2: compressing string with single character
TEST(RleCompressorTests, CompressSingleCharacter) {
    RleCompressor compressor;
    std::string result = compressor.compress("a");
    EXPECT_EQ(result, "a1");
}

// Test 3: compressing string with multiple characters
TEST(RleCompressorTests, CompressMultipleCharacters) {
    RleCompressor compressor;
    std::string result = compressor.compress("aaabbc");
    EXPECT_EQ(result, "a3b2c1");
}

// Test 4: compressing string with digits
// NOTE: When handling digits will use @ for not confusing counts with digits in input
TEST(RleCompressorTests, CompressWithDigit) {
    RleCompressor Compressor;
    std::string result = Compressor.compress("aaa33b");
    EXPECT_EQ(result, "a3@32b1");

}

// --- Tests for Decompress ---

// Test 1: Decompressing empty string
TEST(RleCompressorTests, DecompressEmptyString) {
    RleCompressor compressor;
    std::string result = compressor.decompress("");
    EXPECT_EQ(result, "");
}

// Test 2: Decompressing string with single character
TEST(RleCompressorTests, DecompressSingleCharacter) {
    RleCompressor compressor;
    std::string result = compressor.decompress("a1");
    EXPECT_EQ(result, "a");
}

// Test 3: Decompressing string with multi-digit count
TEST(RleCompressorTests, DecompressMultiDigitCount) {
    RleCompressor compressor;
    std::string result = compressor.decompress("a10");
    EXPECT_EQ(result, "aaaaaaaaaa");
}

// Test 4: Decompressing string with digits
// NOTE: When handling digits will use @ for not confusing counts with digits in input
TEST(RleCompressorTests, DecompressWithDigit) {
    RleCompressor compressor;
    std::string result = compressor.decompress("a3@32b1");
    EXPECT_EQ(result, "aaa33b");
}