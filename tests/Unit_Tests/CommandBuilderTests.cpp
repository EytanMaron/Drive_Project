#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Core/CommandBuilder.h"
#include "Commands/ICommand.h"
#include "Services/IOutput.h"
#include "Services/ICompressor.h"
#include "Services/IStorage.h"
#include "Services/IConfig.h"
#include <stdexcept>
#include <memory>

using ::testing::_;
using ::testing::Return;

// Mock implementations for injected dependencies
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

// Test Fixture
class CommandBuilderTest : public ::testing::Test {
protected:
    std::shared_ptr<MockCompressor> mockCompressor;
    std::shared_ptr<MockStorage> mockStorage;
    std::shared_ptr<MockConfig> mockConfig;
    MockOutput mockOutput;
    std::unique_ptr<CommandBuilder> builder;

    void SetUp() override {
        mockCompressor = std::make_shared<MockCompressor>();
        mockStorage = std::make_shared<MockStorage>();
        mockConfig = std::make_shared<MockConfig>();
        
        // Inject mock dependencies into CommandBuilder
        builder = std::make_unique<CommandBuilder>(mockCompressor, mockStorage, mockConfig);
    }
};

// POST returns 201 Created
TEST_F(CommandBuilderTest, CreateCommand_POST_Returns201Created) {
    auto result = builder->createCommand("POST", &mockOutput);
    EXPECT_NE(result.command, nullptr);
    EXPECT_EQ(result.successStatus, "201 Created");
}

// GET returns 200 Ok
TEST_F(CommandBuilderTest, CreateCommand_GET_Returns200Ok) {
    auto result = builder->createCommand("GET", &mockOutput);
    EXPECT_NE(result.command, nullptr);
    EXPECT_EQ(result.successStatus, "200 Ok");
}

// SEARCH returns 200 Ok
TEST_F(CommandBuilderTest, CreateCommand_SEARCH_Returns200Ok) {
    auto result = builder->createCommand("SEARCH", &mockOutput);
    EXPECT_NE(result.command, nullptr);
    EXPECT_EQ(result.successStatus, "200 Ok");
}

// DELETE returns 204 No Content
TEST_F(CommandBuilderTest, CreateCommand_DELETE_Returns204NoContent) {
    auto result = builder->createCommand("DELETE", &mockOutput);
    EXPECT_NE(result.command, nullptr);
    EXPECT_EQ(result.successStatus, "204 No Content");
}

// Case insensitivity
TEST_F(CommandBuilderTest, CreateCommand_LowercasePost_Works) {
    auto result = builder->createCommand("post", &mockOutput);
    EXPECT_NE(result.command, nullptr);
    EXPECT_EQ(result.successStatus, "201 Created");
}

TEST_F(CommandBuilderTest, CreateCommand_MixedCaseGet_Works) {
    auto result = builder->createCommand("Get", &mockOutput);
    EXPECT_NE(result.command, nullptr);
}

// Unknown key throws
TEST_F(CommandBuilderTest, CreateCommand_UnknownKey_ThrowsException) {
    EXPECT_THROW(builder->createCommand("UNKNOWN", &mockOutput), std::invalid_argument);
}

// Empty key throws
TEST_F(CommandBuilderTest, CreateCommand_EmptyKey_ThrowsException) {
    EXPECT_THROW(builder->createCommand("", &mockOutput), std::invalid_argument);
}

// Each call creates new instance
TEST_F(CommandBuilderTest, CreateCommand_CreatesNewInstanceEachTime) {
    auto result1 = builder->createCommand("POST", &mockOutput);
    auto result2 = builder->createCommand("POST", &mockOutput);
    EXPECT_NE(result1.command.get(), result2.command.get());
}
