#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "IStorage.h"
#include "IConfig.h"
#include "DeleteCommand.h"

using ::testing::_;
using ::testing::Return;
using ::testing::Throw;
using ::testing::StrictMock;

// Mock for storage with remove() method
class MockStorage : public IStorage {
public:
    MOCK_METHOD(void, write, (const std::string& path, const std::string& content), (override));
    MOCK_METHOD(std::string, read, (const std::string& path), (override));
    MOCK_METHOD(std::vector<std::string>, listFiles, (const std::string& path), (override));
    MOCK_METHOD(void, remove, (const std::string& path), (override));
};

// Mock for config
class MockConfig : public IConfig {
public:
    MOCK_METHOD(std::string, getStoragePath, (), (const, override));
};

// Test fixture for DeleteCommand
class DeleteCommandTest : public ::testing::Test {
protected:
    StrictMock<MockStorage> storage;
    StrictMock<MockConfig> config;
    
    std::unique_ptr<DeleteCommand> createCommand() {
        return std::make_unique<DeleteCommand>(&storage, &config);
    }
};

// Test valid filename triggers storage remove with correct path
TEST_F(DeleteCommandTest, Execute_ValidFilename_CallsStorageRemove) {
    std::string fileName = "file.txt";
    std::string rootPath = "/my/storage";
    std::string expectedFullPath = "/my/storage/file.txt";

    EXPECT_CALL(config, getStoragePath())
        .WillOnce(Return(rootPath));

    EXPECT_CALL(storage, remove(expectedFullPath))
        .Times(1);

    auto cmd = createCommand();
    cmd->execute(fileName);
}

// Test empty args throws invalid_argument exception
TEST_F(DeleteCommandTest, Execute_EmptyArgs_ThrowsInvalidArgument) {
    std::string args = "";

    auto cmd = createCommand();
    EXPECT_THROW(cmd->execute(args), std::invalid_argument);
}

// Test whitespace-only args throws invalid_argument exception
TEST_F(DeleteCommandTest, Execute_WhitespaceOnlyArgs_ThrowsInvalidArgument) {
    std::string args = "   ";

    auto cmd = createCommand();
    EXPECT_THROW(cmd->execute(args), std::invalid_argument);
}

// Test filename with internal whitespace throws invalid_argument exception
TEST_F(DeleteCommandTest, Execute_FilenameWithSpaces_ThrowsInvalidArgument) {
    std::string args = "file name.txt";

    auto cmd = createCommand();
    EXPECT_THROW(cmd->execute(args), std::invalid_argument);
}

// Test file not found propagates runtime_error from storage
TEST_F(DeleteCommandTest, Execute_FileNotFound_PropagatesRuntimeError) {
    std::string fileName = "missing.txt";
    std::string rootPath = "/storage";
    std::string fullPath = "/storage/missing.txt";

    EXPECT_CALL(config, getStoragePath())
        .WillOnce(Return(rootPath));

    EXPECT_CALL(storage, remove(fullPath))
        .WillOnce(Throw(std::runtime_error("File not found: " + fullPath)));

    auto cmd = createCommand();

    EXPECT_THROW(cmd->execute(fileName), std::runtime_error);
}

// Test path construction without trailing slash
TEST_F(DeleteCommandTest, Execute_RootPathNoTrailingSlash_AddsSlash) {
    std::string fileName = "test.txt";
    std::string rootPath = "/root";
    std::string expectedFullPath = "/root/test.txt";

    EXPECT_CALL(config, getStoragePath())
        .WillOnce(Return(rootPath));

    EXPECT_CALL(storage, remove(expectedFullPath))
        .Times(1);

    auto cmd = createCommand();
    cmd->execute(fileName);
}

// Test path construction with trailing slash
TEST_F(DeleteCommandTest, Execute_RootPathWithTrailingSlash_NoDoubleSlash) {
    std::string fileName = "test.txt";
    std::string rootPath = "/root/";
    std::string expectedFullPath = "/root/test.txt";

    EXPECT_CALL(config, getStoragePath())
        .WillOnce(Return(rootPath));

    EXPECT_CALL(storage, remove(expectedFullPath))
        .Times(1);

    auto cmd = createCommand();
    cmd->execute(fileName);
}

// Test filename with tabs is rejected
TEST_F(DeleteCommandTest, Execute_FilenameWithTab_ThrowsInvalidArgument) {
    std::string args = "file\tname.txt";

    auto cmd = createCommand();
    EXPECT_THROW(cmd->execute(args), std::invalid_argument);
}

// Test filename with newline is rejected
TEST_F(DeleteCommandTest, Execute_FilenameWithNewline_ThrowsInvalidArgument) {
    std::string args = "file\nname.txt";

    auto cmd = createCommand();
    EXPECT_THROW(cmd->execute(args), std::invalid_argument);
}
