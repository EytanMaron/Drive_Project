#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Commands/CommandExecutor.h"
#include "Commands/ICommand.h"
#include "Core/CommandBuilder.h"
#include "Services/IOutput.h"
#include "Services/ICompressor.h"
#include "Services/IStorage.h"
#include "Services/IConfig.h"
#include <stdexcept>
#include <memory>

using ::testing::Return;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Throw;
using ::testing::NiceMock;

class UnknownException {};

class MockCommand : public ICommand {
public:
    MOCK_METHOD(void, execute, (const std::string&), (override));
};

// Stub implementations for CommandBuilder dependencies
class StubCompressor : public ICompressor {
public:
    std::string compress(std::string data) override { return data; }
    std::string decompress(std::string data) override { return data; }
};

class StubStorage : public IStorage {
public:
    void write(const std::string&, const std::string&) override {}
    std::string read(const std::string&) override { return ""; }
    std::vector<std::string> listFiles(const std::string&) override { return {}; }
    void remove(const std::string&) override {}
};

class StubConfig : public IConfig {
public:
    std::string getStoragePath() const override { return "/tmp/"; }
};

// MockCommandBuilder inherits CommandBuilder with injected stubs
class MockCommandBuilder : public CommandBuilder {
public:
    MockCommandBuilder()
        : CommandBuilder(
            std::make_shared<StubCompressor>(),
            std::make_shared<StubStorage>(),
            std::make_shared<StubConfig>()
        ) {}
    
    MOCK_METHOD(CommandResult, createCommand, 
                (const std::string& key, IOutput* output), (override));
};

class CommandExecutorTest : public ::testing::Test {
protected:
    std::shared_ptr<MockCommandBuilder> mockBuilder;
    std::shared_ptr<CommandExecutor> executor;

    void SetUp() override {
        mockBuilder = std::make_shared<MockCommandBuilder>();
        executor = std::make_shared<CommandExecutor>(mockBuilder);
    }
};

// POST returns 201 Created
TEST_F(CommandExecutorTest, Execute_Post_Returns201Created) {
    auto cmd = std::make_unique<MockCommand>();
    MockCommand* cmdPtr = cmd.get();

    CommandResult result{std::move(cmd), "201 Created"};

    EXPECT_CALL(*mockBuilder, createCommand("POST", _))
        .WillOnce(Return(ByMove(std::move(result))));
    EXPECT_CALL(*cmdPtr, execute("args")).Times(1);
    
    std::string response = executor->execute("POST", "args");
    EXPECT_THAT(response, ::testing::HasSubstr("201 Created"));
}

// GET returns 200 Ok
TEST_F(CommandExecutorTest, Execute_Get_Returns200Ok) {
    auto cmd = std::make_unique<MockCommand>();
    MockCommand* cmdPtr = cmd.get();

    CommandResult result{std::move(cmd), "200 Ok"};

    EXPECT_CALL(*mockBuilder, createCommand("GET", _))
        .WillOnce(Return(ByMove(std::move(result))));
    EXPECT_CALL(*cmdPtr, execute("args")).Times(1);

    std::string response = executor->execute("GET", "args");
    EXPECT_THAT(response, ::testing::HasSubstr("200 Ok"));
}

// DELETE returns 204 No Content
TEST_F(CommandExecutorTest, Execute_Delete_Returns204NoContent) {
    auto cmd = std::make_unique<MockCommand>();
    MockCommand* cmdPtr = cmd.get();

    CommandResult result{std::move(cmd), "204 No Content"};

    EXPECT_CALL(*mockBuilder, createCommand("DELETE", _))
        .WillOnce(Return(ByMove(std::move(result))));
    EXPECT_CALL(*cmdPtr, execute("args")).Times(1);

    std::string response = executor->execute("DELETE", "args");
    EXPECT_THAT(response, ::testing::HasSubstr("204 No Content"));
}

// Invalid command returns 400
TEST_F(CommandExecutorTest, Execute_InvalidCommand_Returns400) {
    EXPECT_CALL(*mockBuilder, createCommand("INVALID", _))
        .WillOnce(Throw(std::invalid_argument("Unknown")));

    std::string response = executor->execute("INVALID", "args");
    EXPECT_THAT(response, ::testing::HasSubstr("400 Bad Request"));
}

// Command runtime error returns 500 (system/I/O errors are server errors)
TEST_F(CommandExecutorTest, Execute_CommandThrowsRuntimeError_Returns500) {
    auto cmd = std::make_unique<MockCommand>();
    MockCommand* cmdPtr = cmd.get();

    CommandResult result{std::move(cmd), "200 Ok"};

    EXPECT_CALL(*mockBuilder, createCommand("GET", _))
        .WillOnce(Return(ByMove(std::move(result))));
    EXPECT_CALL(*cmdPtr, execute("args"))
        .WillOnce(Throw(std::runtime_error("Error")));

    std::string response = executor->execute("GET", "args");
    EXPECT_THAT(response, ::testing::HasSubstr("500 Internal Server Error"));
}

// Unknown exception returns 500
TEST_F(CommandExecutorTest, Execute_UnknownException_Returns500) {
    auto cmd = std::make_unique<MockCommand>();
    MockCommand* cmdPtr = cmd.get();

    CommandResult result{std::move(cmd), "200 Ok"};

    EXPECT_CALL(*mockBuilder, createCommand("GET", _))
        .WillOnce(Return(ByMove(std::move(result))));
    EXPECT_CALL(*cmdPtr, execute("args"))
        .WillOnce(Throw(UnknownException()));

    std::string response = executor->execute("GET", "args");
    EXPECT_THAT(response, ::testing::HasSubstr("500 Internal Server Error"));
}

// SEARCH returns 200 Ok
TEST_F(CommandExecutorTest, Execute_Search_Returns200Ok) {
    auto cmd = std::make_unique<MockCommand>();
    MockCommand* cmdPtr = cmd.get();

    CommandResult result{std::move(cmd), "200 Ok"};

    EXPECT_CALL(*mockBuilder, createCommand("SEARCH", _))
        .WillOnce(Return(ByMove(std::move(result))));
    EXPECT_CALL(*cmdPtr, execute("searchterm")).Times(1);

    std::string response = executor->execute("SEARCH", "searchterm");
    EXPECT_THAT(response, ::testing::HasSubstr("200 Ok"));
}

// Command throws invalid_argument returns 400
TEST_F(CommandExecutorTest, Execute_CommandThrowsInvalidArgument_Returns400) {
    auto cmd = std::make_unique<MockCommand>();
    MockCommand* cmdPtr = cmd.get();

    CommandResult result{std::move(cmd), "200 Ok"};

    EXPECT_CALL(*mockBuilder, createCommand("GET", _))
        .WillOnce(Return(ByMove(std::move(result))));
    EXPECT_CALL(*cmdPtr, execute(""))
        .WillOnce(Throw(std::invalid_argument("Empty args")));

    std::string response = executor->execute("GET", "");
    EXPECT_THAT(response, ::testing::HasSubstr("400 Bad Request"));
}

// File not found returns 404 (from 404 support commit)
TEST_F(CommandExecutorTest, Execute_FileNotFound_Returns404) {
    auto cmd = std::make_unique<MockCommand>();
    MockCommand* cmdPtr = cmd.get();

    CommandResult result{std::move(cmd), "200 Ok"};

    EXPECT_CALL(*mockBuilder, createCommand("GET", _))
        .WillOnce(Return(ByMove(std::move(result))));
    EXPECT_CALL(*cmdPtr, execute("nonexistent.txt"))
        .WillOnce(Throw(std::out_of_range("File not found")));

    std::string response = executor->execute("GET", "nonexistent.txt");
    EXPECT_THAT(response, ::testing::HasSubstr("404 Not Found"));
}
