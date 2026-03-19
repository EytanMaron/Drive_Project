/**
 * @file ServerPortParserTests.cpp
 * @brief Unit tests for ServerPortParser::parsePort() function.
 * 
 * Tests various invalid and valid port scenarios to ensure proper error handling
 * and correct parsing behavior.
 */

#include <gtest/gtest.h>
#include "Server/ServerPortParser.h"
#include <stdexcept>

class ServerPortParserTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test: Empty string should throw std::invalid_argument
TEST_F(ServerPortParserTest, EmptyStringThrowsException) {
    EXPECT_THROW(ServerPortParser::parsePort(""), std::invalid_argument);
}

// Test: Non-numeric string should throw std::invalid_argument
TEST_F(ServerPortParserTest, NonNumericStringThrowsException) {
    EXPECT_THROW(ServerPortParser::parsePort("abc"), std::invalid_argument);
    EXPECT_THROW(ServerPortParser::parsePort("port"), std::invalid_argument);
    EXPECT_THROW(ServerPortParser::parsePort("123abc"), std::invalid_argument);
    EXPECT_THROW(ServerPortParser::parsePort("abc123"), std::invalid_argument);
}

// Test: Port number zero should throw std::invalid_argument
TEST_F(ServerPortParserTest, ZeroPortThrowsException) {
    EXPECT_THROW(ServerPortParser::parsePort("0"), std::invalid_argument);
}

// Test: Negative port number should throw std::invalid_argument
TEST_F(ServerPortParserTest, NegativePortThrowsException) {
    EXPECT_THROW(ServerPortParser::parsePort("-1"), std::invalid_argument);
    EXPECT_THROW(ServerPortParser::parsePort("-100"), std::invalid_argument);
}

// Test: Port number greater than 65535 should throw std::invalid_argument
TEST_F(ServerPortParserTest, PortTooLargeThrowsException) {
    EXPECT_THROW(ServerPortParser::parsePort("65536"), std::invalid_argument);
    EXPECT_THROW(ServerPortParser::parsePort("99999"), std::invalid_argument);
    EXPECT_THROW(ServerPortParser::parsePort("100000"), std::invalid_argument);
}

// Test: Valid port numbers should parse correctly
TEST_F(ServerPortParserTest, ValidPortsParseCorrectly) {
    EXPECT_EQ(ServerPortParser::parsePort("1"), 1);
    EXPECT_EQ(ServerPortParser::parsePort("8080"), 8080);
    EXPECT_EQ(ServerPortParser::parsePort("65535"), 65535);
    EXPECT_EQ(ServerPortParser::parsePort("1024"), 1024);
    EXPECT_EQ(ServerPortParser::parsePort("3000"), 3000);
}

// Test: Port with leading/trailing whitespace should throw (invalid format)
TEST_F(ServerPortParserTest, PortWithWhitespaceThrowsException) {
    EXPECT_THROW(ServerPortParser::parsePort(" 8080"), std::invalid_argument);
    EXPECT_THROW(ServerPortParser::parsePort("8080 "), std::invalid_argument);
    EXPECT_THROW(ServerPortParser::parsePort(" 8080 "), std::invalid_argument);
}

// Test: Port with special characters should throw
TEST_F(ServerPortParserTest, PortWithSpecialCharactersThrowsException) {
    EXPECT_THROW(ServerPortParser::parsePort("8080.0"), std::invalid_argument);
    EXPECT_THROW(ServerPortParser::parsePort("80-80"), std::invalid_argument);
    EXPECT_THROW(ServerPortParser::parsePort("80:80"), std::invalid_argument);
}

// Test: Boundary values - minimum valid port (1)
TEST_F(ServerPortParserTest, MinimumValidPort) {
    EXPECT_EQ(ServerPortParser::parsePort("1"), 1);
}

// Test: Boundary values - maximum valid port (65535)
TEST_F(ServerPortParserTest, MaximumValidPort) {
    EXPECT_EQ(ServerPortParser::parsePort("65535"), 65535);
}

// Test: Boundary values - just below minimum (0)
TEST_F(ServerPortParserTest, JustBelowMinimumThrowsException) {
    EXPECT_THROW(ServerPortParser::parsePort("0"), std::invalid_argument);
}

// Test: Boundary values - just above maximum (65536)
TEST_F(ServerPortParserTest, JustAboveMaximumThrowsException) {
    EXPECT_THROW(ServerPortParser::parsePort("65536"), std::invalid_argument);
}

