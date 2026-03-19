#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "ICompressor.h"
#include "IStorage.h"
#include "IConfig.h"
#include "IOutput.h"
#include "GetCommand.h" 

using ::testing::_;
using ::testing::Return;
using ::testing::Throw;
using ::testing::StrictMock; 

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

class MockOutput : public IOutput {
public:
    MOCK_METHOD(void, write, (const std::string& message), (const, override));
};

class MockConfig : public IConfig {
public:
    MOCK_METHOD(std::string, getStoragePath, (), (const, override));
};

class GetCommandTest : public ::testing::Test {
protected:
    StrictMock<MockCompressor> compressor;
    StrictMock<MockStorage> storage;
    StrictMock<MockConfig> config;
    StrictMock<MockOutput> output;
    
    std::unique_ptr<GetCommand> createCommand() {
        return std::make_unique<GetCommand>(&compressor, &storage, &config, &output);
    }
};

// Valid args - reads, decompresses, outputs
TEST_F(GetCommandTest, Execute_ValidArgs_UsesConfigAndDecompresses) {
    std::string fileName = "file.txt";
    std::string rootPath = "/my/root";
    std::string fullPath = "/my/root/file.txt";
    std::string fileContent = "compressed_data";
    std::string decompressedContent = "readable_data";

    EXPECT_CALL(config, getStoragePath()).WillOnce(Return(rootPath));
    EXPECT_CALL(storage, read(fullPath)).WillOnce(Return(fileContent));
    EXPECT_CALL(compressor, decompress(fileContent)).WillOnce(Return(decompressedContent));
    EXPECT_CALL(output, write(decompressedContent + "\n")).Times(1);

    auto cmd = createCommand();
    cmd->execute(fileName);
}

// Filename with spaces throws invalid_argument
TEST_F(GetCommandTest, Execute_InvalidArgs_SpacesInName_Throws) {
    auto cmd = createCommand();
    EXPECT_THROW(cmd->execute("file name.txt"), std::invalid_argument);
}

// Empty args throws invalid_argument
TEST_F(GetCommandTest, Execute_EmptyArgs_Throws) {
    auto cmd = createCommand();
    EXPECT_THROW(cmd->execute(""), std::invalid_argument);
}

// Whitespace-only args throws invalid_argument
TEST_F(GetCommandTest, Execute_WhitespaceOnlyArgs_Throws) {
    auto cmd = createCommand();
    EXPECT_THROW(cmd->execute("   "), std::invalid_argument);
}

// File not found propagates runtime_error
TEST_F(GetCommandTest, Execute_FileNotFound_PropagatesException) {
    std::string fileName = "missing.txt";
    std::string rootPath = "/storage";
    std::string fullPath = "/storage/missing.txt";

    EXPECT_CALL(config, getStoragePath()).WillOnce(Return(rootPath));
    EXPECT_CALL(storage, read(fullPath)).WillOnce(Throw(std::runtime_error("File not found")));

    auto cmd = createCommand();
    EXPECT_THROW(cmd->execute(fileName), std::runtime_error);
}

// Decompression error propagates exception
TEST_F(GetCommandTest, Execute_DecompressionError_PropagatesException) {
    std::string fileName = "corrupt.txt";
    std::string rootPath = "/storage";
    std::string fullPath = "/storage/corrupt.txt";

    EXPECT_CALL(config, getStoragePath()).WillOnce(Return(rootPath));
    EXPECT_CALL(storage, read(fullPath)).WillOnce(Return("corrupted_data"));
    EXPECT_CALL(compressor, decompress("corrupted_data"))
        .WillOnce(Throw(std::runtime_error("Decompression failed")));

    auto cmd = createCommand();
    EXPECT_THROW(cmd->execute(fileName), std::runtime_error);
}

// Tab in filename throws invalid_argument
TEST_F(GetCommandTest, Execute_TabInFilename_Throws) {
    auto cmd = createCommand();
    EXPECT_THROW(cmd->execute("file\tname.txt"), std::invalid_argument);
}

// Newline in filename throws invalid_argument
TEST_F(GetCommandTest, Execute_NewlineInFilename_Throws) {
    auto cmd = createCommand();
    EXPECT_THROW(cmd->execute("file\nname.txt"), std::invalid_argument);
}