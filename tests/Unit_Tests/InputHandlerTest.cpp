#include <gtest/gtest.h> 
#include <sstream>
#include "StreamInput.h" 

TEST(InputHandlerTest, ReadsSingleValidLine) {
    // Create a mock input stream
    std::istringstream mock_input("add file.txt hello\n");
    StreamInput input(mock_input);  // Inject the mock stream directly
    std::string line = input.readLine();
    // Checks the result
    EXPECT_EQ(line, "add file.txt hello");
}

TEST(InputHandlerTest, ThrowsOnReadFailure) {
    std::istringstream mock_input;  // Empty stream simulates end-of-input failure

    StreamInput input(mock_input);  // Inject the failing stream

    EXPECT_THROW({
        input.readLine();
    }, std::runtime_error);
}