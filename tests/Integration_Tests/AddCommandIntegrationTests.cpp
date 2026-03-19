#include <gtest/gtest.h>
#include "Commands/AddCommand.h"
#include "Services/RleCompressor.h"
#include "Services/FileSystemStorage.h"
#include "Services/EnvVarConfig.h"
#include "Services/ConsoleOutput.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <string>

/**
 * @brief Integration test fixture for AddCommand
 * This test class use real objects (and not mocks) to test AddCommand
 * in integration with all dependencies
 * Each test create temporary directory for storage
 */
class AddCommandIntegrationTest : public ::testing::Test {
protected:
    // Real implementations
    RleCompressor* compressor;
    FileSystemStorage* storage;
    EnvVarConfig* config;
    ConsoleOutput* output;
    AddCommand* command;
    
    // Test directory for file operations
    std::string testStorageDir;
    std::string originalStorageDir; // To restore after test
    
    void SetUp() override {
        // Create temporary directory for this test
        testStorageDir = "test_storage_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed());
        std::filesystem::create_directory(testStorageDir);
        
        // Save original STORAGE_DIR if exists
        const char* original = getenv("STORAGE_DIR");
        if (original != nullptr) {
            originalStorageDir = std::string(original);
        }
        
        // Set STORAGE_DIR to our test directory
        #ifdef _WIN32
            _putenv_s("STORAGE_DIR", testStorageDir.c_str());
        #else
            setenv("STORAGE_DIR", testStorageDir.c_str(), 1);
        #endif
        
        // Create real objects
        compressor = new RleCompressor();
        storage = new FileSystemStorage();
        config = new EnvVarConfig();
        
        // Use stringstream to capture output (even though add should be silent)
        outputStream = new std::stringstream();
        output = new ConsoleOutput(*outputStream);
        
        // Create command with real dependencies
        command = new AddCommand(compressor, storage, config, output);
    }
    
    void TearDown() override {
        // Clean up objects
        delete command;
        delete output;
        delete outputStream;
        delete config;
        delete storage;
        delete compressor;
        
        // Restore original STORAGE_DIR
        if (!originalStorageDir.empty()) {
            #ifdef _WIN32
                _putenv_s("STORAGE_DIR", originalStorageDir.c_str());
            #else
                setenv("STORAGE_DIR", originalStorageDir.c_str(), 1);
            #endif
        } else {
            #ifdef _WIN32
                _putenv_s("STORAGE_DIR", "");
            #else
                unsetenv("STORAGE_DIR");
            #endif
        }
        
        // Remove test directory and all files
        if (std::filesystem::exists(testStorageDir)) {
            std::filesystem::remove_all(testStorageDir);
        }
    }
    
    // Helper to read file content from storage
    std::string readFileContent(const std::string& filename) {
        std::string fullPath = testStorageDir + std::filesystem::path::preferred_separator + filename;
        std::ifstream file(fullPath);
        if (!file.is_open()) {
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    // Helper to check if file exists
    bool fileExists(const std::string& filename) {
        std::string fullPath = testStorageDir + std::filesystem::path::preferred_separator + filename;
        return std::filesystem::exists(fullPath);
    }
    
    // Helper to get captured output
    std::string getCapturedOutput() {
        return outputStream->str();
    }
    
private:
    std::stringstream* outputStream;
};


// SANITY TESTS
/**
 * @brief SANITY: Test basic add command with real objects
 * 
 * This test verify that AddCommand work correct with all real dependencies:
 * - Parse filename and content correct
 * - Compress content using real RLE compressor
 * - Write compressed data to real file system
 * - File is created with correct compressed content
 */
 
TEST_F(AddCommandIntegrationTest, Sanity_AddFileWithRealObjects) {
    // Arrange
    std::string filename = "testfile.txt";
    std::string content = "AAABBBCC";
    std::string expectedCompressed = "A3B3C2"; // RLE compression result
    
    // Act
    command->execute(filename + " " + content);
    
    // Assert
    ASSERT_TRUE(fileExists(filename)) << "File should be created";
    
    std::string fileContent = readFileContent(filename);
    EXPECT_EQ(fileContent, expectedCompressed) << "File should contain compressed content";
    
    // Verify no output was produced (add command is silent)
    EXPECT_EQ(getCapturedOutput(), "") << "Add command should produce no output";
}

/**
 * @brief SANITY: Test that existing file is not overwritten
 * Throws out_of_range when file already exists (logical error → 404).
 */
TEST_F(AddCommandIntegrationTest, Sanity_DoNotOverwriteExistingFile) {
    // Arrange
    std::string filename = "existing.txt";
    std::string originalContent = "original";
    std::string newContent = "new content";
    
    // Create file manually first
    std::string fullPath = testStorageDir + std::filesystem::path::preferred_separator + filename;
    std::ofstream file(fullPath);
    file << originalContent;
    file.close();
    
    // Act & Assert - should throw when file already exists
    EXPECT_THROW(command->execute(filename + " " + newContent), std::out_of_range);
    
    // Verify file still has original content
    ASSERT_TRUE(fileExists(filename)) << "File should still exist";
    std::string fileContent = readFileContent(filename);
    EXPECT_EQ(fileContent, originalContent) << "File should not be overwritten";
}


// BOUNDARY TESTS
/**
 * @brief BOUNDARY: Test add command with empty content
 * 
 * This test verify that AddCommand handle empty content correct:
 * - Empty string should be compressed to empty string
 * - Empty file should be created
 */
TEST_F(AddCommandIntegrationTest, Boundary_AddFileWithEmptyContent) {
    // Arrange
    std::string filename = "empty.txt";
    std::string content = "";
    std::string expectedCompressed = ""; // RLE compresses empty to empty
    
    // Act
    command->execute(filename + " " + content);
    
    // Assert
    ASSERT_TRUE(fileExists(filename)) << "File should be created even with empty content";
    
    std::string fileContent = readFileContent(filename);
    EXPECT_EQ(fileContent, expectedCompressed) << "Empty content should result in empty file";
}





