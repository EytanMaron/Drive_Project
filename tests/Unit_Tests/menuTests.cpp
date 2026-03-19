#include <gtest/gtest.h>
#include <string>
#include <utility> // need this for std::pair
#include "CommandMenu.h" 
#include "StreamInput.h" 

// Test Fixture Class
// Setup and Teardown for every test case
class MenuTest : public ::testing::Test {
protected:
    // Simulates user typing into the console
    std::stringstream ss;  
    IInput* input;
    IMenu* menu;

    // Runs BEFORE every TEST_F
    void SetUp() override {
        // Connecting the plumbing: Stream -> Input -> Menu
        input = new StreamInput(ss);
        menu = new CommandMenu(input);
    }

    // Runs AFTER every TEST_F
    // Cleaning up memory because C++ doesn't have Garbage Collection
    void TearDown() override {
        delete menu;
        delete input;
    }
    
    // Helper function
    // Resets the fake user input.
    void setInput(const std::string& text) {
        ss.str(""); // Clear buffer
        ss.clear(); // Reset error flags (eof bit, etc.)
        ss << text; // Push new text
    }
};

// Test 1: The "Happy Path". Standard input.
TEST_F(MenuTest, NextSeparatesCommandAndDataByFirstSpace) {
    setInput("add file.txt hello world\n");

    std::pair<std::string, std::string> result = menu->next();

    EXPECT_EQ(result.first, "add");
    EXPECT_EQ(result.second, "file.txt hello world");
}

// Test 2: Single word command
TEST_F(MenuTest, NextHandlesCommandWithoutData) {
    setInput("exit\n");

    std::pair<std::string, std::string> result = menu->next();

    EXPECT_EQ(result.first, "exit");
    EXPECT_EQ(result.second, ""); // Should be empty
}

// Test 3: Tricky input with multiple spaces.
// We only want to split on the FIRST space.
TEST_F(MenuTest, NextSplitsOnlyAtFirstSpace) {
    setInput("search text with spaces\n");

    std::pair<std::string, std::string> result = menu->next();

    EXPECT_EQ(result.first, "search");
    EXPECT_EQ(result.second, "text with spaces"); // The rest stays together
}

// Test 4: User just pressed Enter
TEST_F(MenuTest, NextHandlesEmptyLine) {
    setInput("\n");

    std::pair<std::string, std::string> result = menu->next();

    EXPECT_EQ(result.first, "");
    EXPECT_EQ(result.second, "");
}