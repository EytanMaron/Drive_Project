/**
 * Unit tests for the App class.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <utility>
#include <stdexcept>
#include <memory>

#include "Core/App.h"
#include "Core/CommandBuilder.h"
#include "Commands/CommandExecutor.h"
#include "Menu/IMenu.h"
#include "Services/IOutput.h"
#include "Services/ICompressor.h"
#include "Services/IStorage.h"
#include "Services/IConfig.h"

using ::testing::Return;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Throw;

class TestExitException : public std::exception {};

class MockMenu : public IMenu {
public:
    MOCK_METHOD((std::pair<std::string, std::string>), next, (), (override));
};

class MockOutput : public IOutput {
public:
    MOCK_METHOD(void, write, (const std::string& message), (const, override));
};

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

// Helper to create a fully-wired CommandExecutor for testing
static std::shared_ptr<CommandExecutor> createTestExecutor() {
    auto compressor = std::make_shared<NiceMock<MockCompressor>>();
    auto storage = std::make_shared<NiceMock<MockStorage>>();
    auto config = std::make_shared<NiceMock<MockConfig>>();
    auto builder = std::make_shared<CommandBuilder>(compressor, storage, config);
    return std::make_shared<CommandExecutor>(builder);
}

// Construction with mocks
TEST(AppTest, Construction_WithMocks_NoThrow) {
    MockMenu menu;
    MockOutput output;
    EXPECT_NO_THROW(App app(&menu, &output, createTestExecutor()));
}

// Run loop processes requests and outputs responses
TEST(AppTest, Run_ProcessesRequestAndOutputsResponse) {
    NiceMock<MockMenu> menu;
    NiceMock<MockOutput> output;
    
    EXPECT_CALL(menu, next())
        .WillOnce(Return(std::make_pair("POST", "file.txt content")))
        .WillOnce(Throw(TestExitException()));
    
    EXPECT_CALL(output, write(_)).Times(1);
    
    App app(&menu, &output, createTestExecutor());
    EXPECT_THROW(app.run(), TestExitException);
}

// Null menu returns early
TEST(AppTest, Run_NullMenu_ReturnsEarly) {
    MockOutput output;
    App app(nullptr, &output, createTestExecutor());
    EXPECT_NO_THROW(app.run());
}

// Null output returns early
TEST(AppTest, Run_NullOutput_ReturnsEarly) {
    MockMenu menu;
    App app(&menu, nullptr, createTestExecutor());
    EXPECT_NO_THROW(app.run());
}