#define _CRT_SECURE_NO_WARNINGS
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <string>

#include "SearchCommand.h"
#include "RleCompressor.h"
#include "FileSystemStorage.h"
#include "EnvVarConfig.h"
#include "ConsoleOutput.h"

class SearchCommandIntegrationTest : public ::testing::Test {
protected:
    // Pointers to Real Objects
    RleCompressor* compressor;
    FileSystemStorage* storage;
    EnvVarConfig* config;
    ConsoleOutput* output;
    SearchCommand* command;

    // Variables to manage the sandbox environment
    std::string testStorageDir;
    std::string originalStorageDir;
    std::stringstream* outputStream; // To capture what is printed to screen

    // Runs before the test
    void SetUp() override {
        // Create a unique directory name based on random seed
        testStorageDir = std::filesystem::absolute("test_search_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed())).string();
        std::filesystem::create_directory(testStorageDir);

        // Save the original environment variable (if exists)
        const char* original = std::getenv("STORAGE_DIR");
        if (original != nullptr) {
            originalStorageDir = std::string(original);
        }

        // Hijack the environment variable to point to our test folder
        #ifdef _WIN32
            _putenv_s("STORAGE_DIR", testStorageDir.c_str());
        #else
            setenv("STORAGE_DIR", testStorageDir.c_str(), 1);
        #endif

        // Instantiate REAL objects
        compressor = new RleCompressor();
        storage = new FileSystemStorage();
        config = new EnvVarConfig();

        // Capture output: We redirect output to a stringstream instead of cout
        outputStream = new std::stringstream();
        output = new ConsoleOutput(*outputStream);

        // Create the Command with all real dependencies
        command = new SearchCommand(compressor, storage, config, output);
    }

    // Runs for after each test
    void TearDown() override {
        // Clean up memory
        delete command;
        delete output;
        delete outputStream;
        delete config;
        delete storage;
        delete compressor;

        // Restore the original environment variable
        if (!originalStorageDir.empty()) {
            #ifdef _WIN32
                _putenv_s("STORAGE_DIR", originalStorageDir.c_str());
            #else
                setenv("STORAGE_DIR", originalStorageDir.c_str(), 1);
            #endif
        } else {
            // If it didn't exist before, remove it
            #ifdef _WIN32
                _putenv_s("STORAGE_DIR", "");
            #else
                unsetenv("STORAGE_DIR");
            #endif
        }

        // Physically delete the test directory and all files inside
        if (std::filesystem::exists(testStorageDir)) {
            std::filesystem::remove_all(testStorageDir);
        }
    }

    // Helper to create a compressed file on the real disk
    void createCompressedFile(const std::string& filename, const std::string& content) {
        // Use the real compressor to convert raw text to compressed format
        std::string compressed = compressor->compress(content);
        
        // Construct the full path inside our sandbox directory
        std::string fullPath = testStorageDir + "/" + filename; 
        
        // Write physically to disk using standard C++ file output
        std::ofstream file(fullPath);
        if (!file.is_open()) {
            throw std::runtime_error("Integration Test Error: Could not create file: " + fullPath);
        }
        file << compressed;
        file.close();
    }

    // Helper to retrieve what was printed to the "screen"
    std::string getCapturedOutput() {
        return outputStream->str();
    }
};


// --- SANITY TESTS ---

// SANITY: Search finds a string in decompressed file content
TEST_F(SearchCommandIntegrationTest, Sanity_FindsStringInRealFile) {
    // Arrange
    std::string filename = "real.txt";
    createCompressedFile(filename, "AAABBBCCC");

    // Act - search for "BBB" which exists in decompressed content
    command->execute("BBB");

    // Assert
    std::string output = getCapturedOutput();
    EXPECT_NE(output.find(filename), std::string::npos) 
        << "Output should contain the filename where string was found";
}

// SANITY: Search finds a string in filename (new Exercise 2 feature)
TEST_F(SearchCommandIntegrationTest, Sanity_FindsStringInFilename) {
    // Arrange
    std::string filename = "report_data.txt";
    createCompressedFile(filename, "HELLO WORLD");

    // Act - search for "report" which is in the filename
    command->execute("report");

    // Assert
    std::string output = getCapturedOutput();
    EXPECT_NE(output.find(filename), std::string::npos) 
        << "Output should contain the filename that matches search term";
}

// --- NEGATIVE TESTS ---

// NEGATIVE: Search does not find missing string
TEST_F(SearchCommandIntegrationTest, Negative_DoesNotFindMissingString) {
    // Arrange
    createCompressedFile("file1.txt", "HELLO WORLD");
    
    // Act
    command->execute("GOODBYE");

    // Assert - output should be empty because nothing matched
    EXPECT_EQ(getCapturedOutput(), "") << "Should output nothing if not found";
}

// NEGATIVE: Empty args throws exception
TEST_F(SearchCommandIntegrationTest, Negative_EmptyArgsThrows) {
    EXPECT_THROW(command->execute(""), std::invalid_argument);
}

// NEGATIVE: Whitespace-only args throws exception
TEST_F(SearchCommandIntegrationTest, Negative_WhitespaceOnlyArgsThrows) {
    EXPECT_THROW(command->execute("   "), std::invalid_argument);
}

// --- INTEGRATION TESTS ---

// INTEGRATION: Search works correctly with multiple files
TEST_F(SearchCommandIntegrationTest, Integration_SearchMultipleFiles) {
    // Arrange
    createCompressedFile("found_1.txt", "TARGET HERE");
    createCompressedFile("miss_2.txt", "NOTHING HERE");
    createCompressedFile("found_3.txt", "ALSO TARGET");

    // Act
    command->execute("TARGET");

    // Assert
    std::string output = getCapturedOutput();
    
    // Verify correct files are found
    EXPECT_NE(output.find("found_1.txt"), std::string::npos) << "Should find first file";
    EXPECT_NE(output.find("found_3.txt"), std::string::npos) << "Should find third file";
    
    // Verify incorrect file is NOT found
    EXPECT_EQ(output.find("miss_2.txt"), std::string::npos) << "Should NOT find second file";
}

// INTEGRATION: Search finds files by both filename and content (no duplicates)
TEST_F(SearchCommandIntegrationTest, Integration_FilenameAndContentMatch_NoDuplicates) {
    // Arrange - filename contains "log" AND content contains "log"
    createCompressedFile("log_file.txt", "This is a log entry");

    // Act
    command->execute("log");

    // Assert - file should appear exactly once
    std::string output = getCapturedOutput();
    EXPECT_NE(output.find("log_file.txt"), std::string::npos) << "Should find the file";
    
    // Count occurrences - should be exactly 1
    size_t first = output.find("log_file.txt");
    size_t second = output.find("log_file.txt", first + 1);
    EXPECT_EQ(second, std::string::npos) << "File should appear only once (deduplication)";
}

// INTEGRATION: Case sensitivity test
TEST_F(SearchCommandIntegrationTest, Integration_CaseSensitive) {
    // Arrange
    createCompressedFile("DATA.txt", "hello world");

    // Act - search for lowercase "data"
    command->execute("data");

    // Assert - should NOT find "DATA.txt" (case sensitive)
    std::string output = getCapturedOutput();
    EXPECT_EQ(output.find("DATA.txt"), std::string::npos) 
        << "Search should be case-sensitive";
}

// INTEGRATION: Empty directory returns no output
TEST_F(SearchCommandIntegrationTest, Integration_EmptyDirectory) {
    // Arrange - no files created

    // Act
    command->execute("anything");

    // Assert
    EXPECT_EQ(getCapturedOutput(), "") << "Empty directory should produce no output";
}
