#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>

#include "SearchCommand.h"
#include "IStorage.h"
#include "ICompressor.h"
#include "IOutput.h"
#include "IConfig.h"

using ::testing::Return;
using ::testing::_;
using ::testing::Throw;
using ::testing::HasSubstr;

// Mock implementations for dependency injection
class MockStorage : public IStorage {
public:
    MOCK_METHOD(void, write, (const std::string& path, const std::string& content), (override));
    MOCK_METHOD(std::string, read, (const std::string& path), (override));
    MOCK_METHOD(std::vector<std::string>, listFiles, (const std::string& path), (override));
    MOCK_METHOD(void, remove, (const std::string& path), (override));
};

class MockCompressor : public ICompressor {
public:
    MOCK_METHOD(std::string, compress, (std::string data), (override));
    MOCK_METHOD(std::string, decompress, (std::string data), (override));
};

class MockOutput : public IOutput {
public:
    MOCK_METHOD(void, write, (const std::string& message), (const, override));
};

class MockConfig : public IConfig {
public:
    MOCK_METHOD(std::string, getStoragePath, (), (const, override));
};

// Test fixture with shared mock setup
class SearchCommandTest : public ::testing::Test {
protected:
    MockStorage mockStorage;
    MockCompressor mockCompressor;
    MockOutput mockOutput;
    MockConfig mockConfig;
    SearchCommand* cmd;

    void SetUp() override {
        cmd = new SearchCommand(&mockCompressor, &mockStorage, &mockConfig, &mockOutput);
    }

    void TearDown() override {
        delete cmd;
    }
};

// --- Filename Match Tests ---

// Matches file by filename only, content does not contain search term
TEST_F(SearchCommandTest, FilenameMatch_NoContentMatch) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return("storage"));
    EXPECT_CALL(mockStorage, listFiles("storage/"))
        .WillOnce(Return(std::vector<std::string>{"report.txt"}));
    EXPECT_CALL(mockStorage, read("storage/report.txt"))
        .WillOnce(Return("compressed"));
    EXPECT_CALL(mockCompressor, decompress("compressed"))
        .WillOnce(Return("no match here"));

    // File should be found because "report" is in filename
    EXPECT_CALL(mockOutput, write(HasSubstr("report.txt"))).Times(1);

    cmd->execute("report");
}

// --- Content Match Tests ---

// Matches file by content only, filename does not contain search term
TEST_F(SearchCommandTest, ContentMatch_NoFilenameMatch) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return("storage"));
    EXPECT_CALL(mockStorage, listFiles("storage/"))
        .WillOnce(Return(std::vector<std::string>{"data.txt"}));
    EXPECT_CALL(mockStorage, read("storage/data.txt"))
        .WillOnce(Return("compressed"));
    EXPECT_CALL(mockCompressor, decompress("compressed"))
        .WillOnce(Return("This contains SECRET information"));

    // File should be found because "SECRET" is in content
    EXPECT_CALL(mockOutput, write(HasSubstr("data.txt"))).Times(1);

    cmd->execute("SECRET");
}

// --- Deduplication Tests ---

// File matches both filename and content, should appear only once
TEST_F(SearchCommandTest, BothMatch_NoDuplicates) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return("storage"));
    EXPECT_CALL(mockStorage, listFiles("storage/"))
        .WillOnce(Return(std::vector<std::string>{"notes.txt"}));
    EXPECT_CALL(mockStorage, read("storage/notes.txt"))
        .WillOnce(Return("compressed"));
    EXPECT_CALL(mockCompressor, decompress("compressed"))
        .WillOnce(Return("These are notes about the project"));

    // "notes" appears in both filename and content - verify single output
    EXPECT_CALL(mockOutput, write(_)).Times(1);

    cmd->execute("notes");
}

// --- Multiple Files Tests ---

// Multiple files with mixed match types
TEST_F(SearchCommandTest, MultipleFiles_MixedMatches) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return("dir"));
    EXPECT_CALL(mockStorage, listFiles("dir/"))
        .WillOnce(Return(std::vector<std::string>{"log.txt", "data.bin", "other.txt"}));

    // log.txt: filename matches "log", content doesn't
    EXPECT_CALL(mockStorage, read("dir/log.txt")).WillOnce(Return("c1"));
    EXPECT_CALL(mockCompressor, decompress("c1")).WillOnce(Return("no match"));

    // data.bin: filename doesn't match, content has "log"
    EXPECT_CALL(mockStorage, read("dir/data.bin")).WillOnce(Return("c2"));
    EXPECT_CALL(mockCompressor, decompress("c2")).WillOnce(Return("contains log entry"));

    // other.txt: neither matches
    EXPECT_CALL(mockStorage, read("dir/other.txt")).WillOnce(Return("c3"));
    EXPECT_CALL(mockCompressor, decompress("c3")).WillOnce(Return("nothing here"));

    // Should output log.txt and data.bin
    EXPECT_CALL(mockOutput, write(_)).WillOnce([](const std::string& msg) {
        EXPECT_TRUE(msg.find("log.txt") != std::string::npos);
        EXPECT_TRUE(msg.find("data.bin") != std::string::npos);
        EXPECT_TRUE(msg.find("other.txt") == std::string::npos);
    });

    cmd->execute("log");
}

// --- Edge Case Tests ---

// Empty search term throws invalid_argument
TEST_F(SearchCommandTest, EmptyArgs_ThrowsInvalidArgument) {
    EXPECT_CALL(mockConfig, getStoragePath()).Times(0);
    EXPECT_CALL(mockStorage, listFiles(_)).Times(0);
    EXPECT_CALL(mockOutput, write(_)).Times(0);

    EXPECT_THROW(cmd->execute(""), std::invalid_argument);
}

// Whitespace-only search term throws invalid_argument
TEST_F(SearchCommandTest, WhitespaceArgs_ThrowsInvalidArgument) {
    EXPECT_CALL(mockConfig, getStoragePath()).Times(0);
    EXPECT_CALL(mockStorage, listFiles(_)).Times(0);
    EXPECT_CALL(mockOutput, write(_)).Times(0);

    EXPECT_THROW(cmd->execute("   \t\n"), std::invalid_argument);
}

// No matches found should not produce output
TEST_F(SearchCommandTest, NoMatches_NoOutput) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return("dir"));
    EXPECT_CALL(mockStorage, listFiles("dir/"))
        .WillOnce(Return(std::vector<std::string>{"file.txt"}));
    EXPECT_CALL(mockStorage, read("dir/file.txt")).WillOnce(Return("c"));
    EXPECT_CALL(mockCompressor, decompress("c")).WillOnce(Return("content"));

    EXPECT_CALL(mockOutput, write(_)).Times(0);

    cmd->execute("nonexistent");
}

// --- Error Handling Tests ---

// Config error propagates to caller for proper error response
TEST_F(SearchCommandTest, ConfigError_PropagatesException) {
    EXPECT_CALL(mockConfig, getStoragePath())
        .WillOnce(Throw(std::runtime_error("Config error")));
    EXPECT_CALL(mockStorage, listFiles(_)).Times(0);
    EXPECT_CALL(mockOutput, write(_)).Times(0);

    EXPECT_THROW(cmd->execute("test"), std::runtime_error);
}

// Storage listFiles error propagates to caller for proper error response
TEST_F(SearchCommandTest, ListFilesError_PropagatesException) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return("dir"));
    EXPECT_CALL(mockStorage, listFiles("dir/"))
        .WillOnce(Throw(std::runtime_error("Storage error")));
    EXPECT_CALL(mockOutput, write(_)).Times(0);

    EXPECT_THROW(cmd->execute("test"), std::runtime_error);
}

// Single file read error should skip that file, process others
TEST_F(SearchCommandTest, ReadError_SkipsFile) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return("dir"));
    EXPECT_CALL(mockStorage, listFiles("dir/"))
        .WillOnce(Return(std::vector<std::string>{"bad.txt", "good.txt"}));

    // bad.txt throws on read
    EXPECT_CALL(mockStorage, read("dir/bad.txt"))
        .WillOnce(Throw(std::runtime_error("Read error")));

    // good.txt works normally
    EXPECT_CALL(mockStorage, read("dir/good.txt")).WillOnce(Return("c"));
    EXPECT_CALL(mockCompressor, decompress("c")).WillOnce(Return("has test data"));

    // Should still find good.txt by content
    EXPECT_CALL(mockOutput, write(HasSubstr("good.txt"))).Times(1);

    cmd->execute("test");
}

// Decompression error should skip that file, process others
TEST_F(SearchCommandTest, DecompressError_SkipsFile) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return("dir"));
    EXPECT_CALL(mockStorage, listFiles("dir/"))
        .WillOnce(Return(std::vector<std::string>{"corrupt.txt", "valid.txt"}));

    // corrupt.txt: read works but decompress fails
    EXPECT_CALL(mockStorage, read("dir/corrupt.txt")).WillOnce(Return("bad"));
    EXPECT_CALL(mockCompressor, decompress("bad"))
        .WillOnce(Throw(std::runtime_error("Decompress error")));

    // valid.txt works normally
    EXPECT_CALL(mockStorage, read("dir/valid.txt")).WillOnce(Return("good"));
    EXPECT_CALL(mockCompressor, decompress("good")).WillOnce(Return("has query here"));

    // Should still find valid.txt
    EXPECT_CALL(mockOutput, write(HasSubstr("valid.txt"))).Times(1);

    cmd->execute("query");
}

// --- Case Sensitivity Tests ---

// Case-sensitive filename search
TEST_F(SearchCommandTest, CaseSensitive_FilenameNoMatch) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return("dir"));
    EXPECT_CALL(mockStorage, listFiles("dir/"))
        .WillOnce(Return(std::vector<std::string>{"Report.txt"}));
    EXPECT_CALL(mockStorage, read("dir/Report.txt")).WillOnce(Return("c"));
    EXPECT_CALL(mockCompressor, decompress("c")).WillOnce(Return("no match"));

    // "report" should NOT match "Report.txt" (case sensitive)
    EXPECT_CALL(mockOutput, write(_)).Times(0);

    cmd->execute("report");
}

// Case-sensitive content search
TEST_F(SearchCommandTest, CaseSensitive_ContentNoMatch) {
    EXPECT_CALL(mockConfig, getStoragePath()).WillOnce(Return("dir"));
    EXPECT_CALL(mockStorage, listFiles("dir/"))
        .WillOnce(Return(std::vector<std::string>{"file.txt"}));
    EXPECT_CALL(mockStorage, read("dir/file.txt")).WillOnce(Return("c"));
    EXPECT_CALL(mockCompressor, decompress("c")).WillOnce(Return("Hello World"));

    // "hello" should NOT match "Hello" (case sensitive)
    EXPECT_CALL(mockOutput, write(_)).Times(0);

    cmd->execute("hello");
}
