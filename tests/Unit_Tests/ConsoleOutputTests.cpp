#include "Services/ConsoleOutput.h"
#include <gtest/gtest.h>
#include <string>
#include <sstream> // for std::ostringstream

// Sanity test
// Test 1: Verify that writing a simple string works as expected.
TEST(ConsoleOutputTests, Sanity_WriteSimpleMessage) {
    std::stringstream fake_console;
    ConsoleOutput console_output(fake_console);

    console_output.write("Hello, TDD!");

    EXPECT_EQ(fake_console.str(), "Hello, TDD!");
}

// Boundary test
// Test 2: Verify that writing an empty string does nothing (and does not crash).
TEST(ConsoleOutputTests, Boundry_WriteEmptyMessage) {
    std::stringstream fake_console;
    ConsoleOutput console_output(fake_console);

    console_output.write("");

    EXPECT_EQ(fake_console.str(), "");
}

// Negative test
// Test 3: Verify that writing to a stream that is in a bad state throws an exception.
TEST(ConsoleOutputTests, Negative_WriteToBadStream) {
    std::stringstream broken_stream;

    broken_stream.setstate(std::ios::failbit); // makes broken_stream bad
    ConsoleOutput console_output(broken_stream);

    EXPECT_THROW(console_output.write("This should fail"), std::runtime_error);
}