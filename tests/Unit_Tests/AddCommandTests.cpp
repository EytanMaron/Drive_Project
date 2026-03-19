#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "AddCommand.h"
#include <stdexcept>
#include <filesystem>
#include <fstream>

//less problematic usings than 'using namespace std' because:
//I only use a few symbols from testing
//its only used in test container scope
using ::testing::_;
using ::testing::Return;
using ::testing::Eq;
using ::testing::Throw;

// Mocks - Interfaces for dependencies 
// ICompressor, IStorage, IConfig, IOutput
class MockCompressor : public ICompressor {
public:
    MOCK_METHOD(std::string, compress, (std::string data), (override));
    MOCK_METHOD(std::string, decompress, (std::string data), (override));
};

class MockStorage : public IStorage {
public:
    MOCK_METHOD(void, write, (const std::string& path, const std::string& content), (override));
    MOCK_METHOD(std::string, read, (const std::string& path), (override));
    MOCK_METHOD(std::vector<std::string>, listFiles, (const std::string& path), (override));
    MOCK_METHOD(void, remove, (const std::string& path), (override));
};

class MockConfig : public IConfig {
public:
    MOCK_METHOD(std::string, getStoragePath, (), (const, override));
};

class MockOutput : public IOutput {
public:
    MOCK_METHOD(void, write, (const std::string& message), (const, override));
};

// Test Fixture - common setup for AddCommand tests
class AddCommandTest : public ::testing::Test {
protected:
    MockCompressor mockCompressor;
    MockStorage mockStorage;
    MockConfig mockConfig;
    MockOutput mockOutput;

    const std::string TEST_FILENAME = "testfile.txt";
    const std::string RAW_CONTENT = "AAABBBCC";
    const std::string COMPRESSED_CONTENT = "A3B3C2"; 
    std::string STORAGE_PATH;
    std::string FULL_PATH;
    
    void SetUp() override {
        // Create a temporary directory for each test
        std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "AddCommandTest";
        std::filesystem::create_directories(tempDir);
        STORAGE_PATH = tempDir.string() + std::filesystem::path::preferred_separator;
        FULL_PATH = STORAGE_PATH + TEST_FILENAME;
    }
    
    void TearDown() override {
        // Clean up temporary directory
        std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "AddCommandTest";
        if (std::filesystem::exists(tempDir)) {
            std::filesystem::remove_all(tempDir);
        }
    }
    

    // Helper to create a file for tests that need file to exist
    void createTestFile(const std::string& filename, const std::string& content = "") {
        std::filesystem::path filePath = std::filesystem::path(STORAGE_PATH) / filename;
        std::ofstream file(filePath);
        file << content;
        file.close();
    }
};


//SANITY TESTS

//Basic test: Checks that it compresses, gets the path, and writes exactly once.
TEST_F(AddCommandTest, Sanity_CompressAndWriteFile) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return(STORAGE_PATH));
    EXPECT_CALL(mockCompressor, compress(RAW_CONTENT)).WillOnce(Return(COMPRESSED_CONTENT));
    EXPECT_CALL(mockStorage, write(Eq(FULL_PATH), Eq(COMPRESSED_CONTENT))).Times(1);
    EXPECT_CALL(mockOutput, write(_)).Times(0);

    AddCommand command(&mockCompressor, &mockStorage, &mockConfig, &mockOutput);
    command.execute(TEST_FILENAME + " " + RAW_CONTENT);
}

// File already exists throws out_of_range (logical error → 404)
TEST_F(AddCommandTest, Negative_ThrowsIfFileAlreadyExists) {
    createTestFile(TEST_FILENAME, "AlreadyThere");

    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return(STORAGE_PATH));
    EXPECT_CALL(mockCompressor, compress(_)).Times(0);
    EXPECT_CALL(mockStorage, write(_, _)).Times(0);

    AddCommand command(&mockCompressor, &mockStorage, &mockConfig, &mockOutput);
    EXPECT_THROW(command.execute(TEST_FILENAME + " " + RAW_CONTENT), std::out_of_range);
}

// Compression failure propagates exception
TEST_F(AddCommandTest, Negative_PropagatesCompressionException) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return(STORAGE_PATH));
    EXPECT_CALL(mockCompressor, compress(_)).WillOnce(Throw(std::runtime_error("RLE Error")));
    EXPECT_CALL(mockStorage, write(_, _)).Times(0);

    AddCommand command(&mockCompressor, &mockStorage, &mockConfig, &mockOutput);
    EXPECT_THROW(command.execute(TEST_FILENAME + " " + RAW_CONTENT), std::runtime_error);
}

// Write failure propagates exception
TEST_F(AddCommandTest, Negative_PropagatesWriteException) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return(STORAGE_PATH));
    EXPECT_CALL(mockCompressor, compress(RAW_CONTENT)).WillOnce(Return(COMPRESSED_CONTENT));
    EXPECT_CALL(mockStorage, write(Eq(FULL_PATH), Eq(COMPRESSED_CONTENT)))
        .WillOnce(Throw(std::runtime_error("I/O Error")));

    AddCommand command(&mockCompressor, &mockStorage, &mockConfig, &mockOutput);
    EXPECT_THROW(command.execute(TEST_FILENAME + " " + RAW_CONTENT), std::runtime_error);
}

// Empty args throws invalid_argument
TEST_F(AddCommandTest, Negative_ThrowsOnEmptyArgs) {
    AddCommand command(&mockCompressor, &mockStorage, &mockConfig, &mockOutput);
    EXPECT_THROW(command.execute(""), std::invalid_argument);
}

// Leading space throws invalid_argument
TEST_F(AddCommandTest, Negative_ThrowsOnLeadingSpace) {
    AddCommand command(&mockCompressor, &mockStorage, &mockConfig, &mockOutput);
    EXPECT_THROW(command.execute(" filename"), std::invalid_argument);
}

// Filename only (no content) - should succeed with empty content
TEST_F(AddCommandTest, Sanity_FilenameOnly_CreatesEmptyFile) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return(STORAGE_PATH));
    EXPECT_CALL(mockCompressor, compress("")).WillOnce(Return(""));
    EXPECT_CALL(mockStorage, write(Eq(FULL_PATH), Eq(""))).Times(1);

    AddCommand command(&mockCompressor, &mockStorage, &mockConfig, &mockOutput);
    command.execute(TEST_FILENAME);
}

// Content with spaces is preserved
TEST_F(AddCommandTest, Sanity_ContentWithSpaces_Preserved) {
    std::string content = "hello world test";
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return(STORAGE_PATH));
    EXPECT_CALL(mockCompressor, compress(content)).WillOnce(Return("compressed"));
    EXPECT_CALL(mockStorage, write(Eq(FULL_PATH), Eq("compressed"))).Times(1);

    AddCommand command(&mockCompressor, &mockStorage, &mockConfig, &mockOutput);
    command.execute(TEST_FILENAME + " " + content);
}
