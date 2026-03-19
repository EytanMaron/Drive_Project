/**
 * ClientHandler Unit Tests
 * 
 * These tests are written before implementing ClientHandler class,
 * and are designed to validate its functionality once implemented.
 * We make sure tests fail before implementation of ClientHandler (red phase).
 * 
 * Testing Strategy:
 * - Uses socketpair() to create connected socket pairs for testing
 * - ClientHandler reads commands from one socket and writes responses to the same socket
 * - Tests cover: single command, multiple commands, error handling, socket closure
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Server/ClientHandler.h"
#include "Commands/CommandExecutor.h"
#include "Core/CommandBuilder.h"
#include "Services/IStorage.h"
#include "Services/ICompressor.h"
#include "Services/IConfig.h"
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>
#include <string>
#include <memory>
#include <thread>
#include <chrono>

using ::testing::NiceMock;
using ::testing::Return;

// Mock classes for CommandBuilder dependencies
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

class MockConfig : public IConfig {
public:
    MOCK_METHOD(std::string, getStoragePath, (), (const, override));
};

// Helper function to create CommandExecutor with mocks
static std::shared_ptr<CommandExecutor> createTestExecutor() {
    auto compressor = std::make_shared<NiceMock<MockCompressor>>();
    auto storage = std::make_shared<NiceMock<MockStorage>>();
    auto config = std::make_shared<NiceMock<MockConfig>>();
    auto builder = std::make_shared<CommandBuilder>(compressor, storage, config);
    return std::make_shared<CommandExecutor>(builder);
}

// Helper class to manage socket pairs for testing
class SocketPairHelper {
public:
    SocketPairHelper() {
        int sockets[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == -1) {
            throw std::runtime_error("Failed to create socket pair");
        }
        m_writeFd = sockets[0];
        m_readFd = sockets[1];
    }

    ~SocketPairHelper() {
        if (m_writeFd != -1) close(m_writeFd);
        if (m_readFd != -1) close(m_readFd);
    }

    // Prevent copying (manages file descriptors)
    SocketPairHelper(const SocketPairHelper&) = delete;
    SocketPairHelper& operator=(const SocketPairHelper&) = delete;

    // Get the file descriptor for reading (used by ClientHandler)
    int getReadFd() const { return m_readFd; }

    // Get the file descriptor for writing (used to send test data)
    int getWriteFd() const { return m_writeFd; }

    // Write data to the socket
    void writeToSocket(const std::string& data) {
        ssize_t written = write(m_writeFd, data.c_str(), data.length());
        if (written == -1) {
            throw std::runtime_error("Failed to write to test socket");
        }
    }

    // Read data from the socket (with timeout)
    // Note: Handler writes to m_readFd, so we read from m_writeFd
    std::string readFromSocket(size_t maxBytes = 4096, int timeoutMs = 1000) {
        std::string result;
        char buffer[4096];
        
        // Wait for data with timeout
        // Handler writes to m_readFd, so we read from m_writeFd (other end of pair)
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(m_writeFd, &readfds);  // Read from writeFd to get handler's responses
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        
        int selectResult = select(m_writeFd + 1, &readfds, nullptr, nullptr, &timeout);
        if (selectResult > 0 && FD_ISSET(m_writeFd, &readfds)) {
            ssize_t bytesRead = read(m_writeFd, buffer, std::min(maxBytes, sizeof(buffer)));
            if (bytesRead > 0) {
                result.assign(buffer, bytesRead);
            }
        }
        return result;
    }

    // Close the write socket to simulate connection closure
    void closeWriteSocket() {
        if (m_writeFd != -1) {
            close(m_writeFd);
            m_writeFd = -1;
        }
    }

private:
    int m_writeFd = -1;
    int m_readFd = -1;
};

// Test: ClientHandler processes a single command successfully
TEST(ClientHandlerTest, HandlesSingleCommand) {
    SocketPairHelper socketPair;
    
    // Create CommandExecutor with CommandBuilder
    auto executor = createTestExecutor();
    
    // Create ClientHandler
    ClientHandler handler(socketPair.getReadFd(), executor);
    
    // Run handler in a separate thread (it should process and exit)
    std::thread handlerThread([&handler]() {
        handler.handle();
    });
    
    // Send command
    socketPair.writeToSocket("GET file.txt\n");
    
    // Wait a bit for handler to process
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Read response (with timeout)
    std::string response = socketPair.readFromSocket(4096, 500);
    
    // Close write socket to signal handler to exit
    socketPair.closeWriteSocket();
    
    // Wait for handler to finish
    handlerThread.join();
    
    // Verify response contains status code
    EXPECT_TRUE(response.find("200") != std::string::npos || 
                response.find("404") != std::string::npos ||
                response.find("400") != std::string::npos);
}

// Test: ClientHandler processes multiple commands sequentially
TEST(ClientHandlerTest, HandlesMultipleCommands) {
    SocketPairHelper socketPair;
    
    auto executor = createTestExecutor();
    
    ClientHandler handler(socketPair.getReadFd(), executor);
    
    // Send multiple commands
    socketPair.writeToSocket("POST file.txt hello\nGET file.txt\n");
    
    std::thread handlerThread([&handler]() {
        handler.handle();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::string response = socketPair.readFromSocket();
    
    socketPair.closeWriteSocket();
    handlerThread.join();
    
    // Should have processed both commands - check for status codes
    EXPECT_TRUE(response.find("201") != std::string::npos || 
                response.find("200") != std::string::npos ||
                response.find("404") != std::string::npos ||
                response.find("400") != std::string::npos);
}

// Test: ClientHandler handles invalid commands gracefully
TEST(ClientHandlerTest, HandlesInvalidCommand) {
    SocketPairHelper socketPair;
    
    auto executor = createTestExecutor();
    
    ClientHandler handler(socketPair.getReadFd(), executor);
    
    // Run handler in a separate thread
    std::thread handlerThread([&handler]() {
        handler.handle();
    });
    
    // Send invalid command
    socketPair.writeToSocket("INVALID command\n");
    
    // Wait a bit for handler to process
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Read response (with timeout)
    std::string response = socketPair.readFromSocket(4096, 500);
    
    // Close write socket to signal handler to exit
    socketPair.closeWriteSocket();
    
    handlerThread.join();
    
    // Should return error status
    EXPECT_TRUE(response.find("400") != std::string::npos);
}

// Test: ClientHandler exits when socket is closed
TEST(ClientHandlerTest, ExitsOnSocketClosure) {
    SocketPairHelper socketPair;
    
    auto executor = createTestExecutor();
    
    ClientHandler handler(socketPair.getReadFd(), executor);
    
    // Close socket immediately
    socketPair.closeWriteSocket();
    
    // Handler should exit without hanging
    std::thread handlerThread([&handler]() {
        handler.handle();
    });
    
    // Wait for handler to complete (with timeout)
    auto start = std::chrono::steady_clock::now();
    handlerThread.join();
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete quickly (within 1 second)
    EXPECT_LT(duration.count(), 1000);
}

// Test: ClientHandler handles empty line
TEST(ClientHandlerTest, HandlesEmptyLine) {
    SocketPairHelper socketPair;
    
    auto executor = createTestExecutor();
    
    ClientHandler handler(socketPair.getReadFd(), executor);
    
    // Send empty line
    socketPair.writeToSocket("\n");
    
    std::thread handlerThread([&handler]() {
        handler.handle();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Read response - should get error status for empty command
    std::string response = socketPair.readFromSocket();
    
    socketPair.closeWriteSocket();
    handlerThread.join();
    
    // Should return error status (400) for empty/invalid command
    EXPECT_TRUE(response.find("400") != std::string::npos || response.empty());
}

// Test: ClientHandler closes socket when done
TEST(ClientHandlerTest, ClosesSocketOnCompletion) {
    SocketPairHelper socketPair;
    
    auto executor = createTestExecutor();
    
    int clientFd = socketPair.getReadFd();
    ClientHandler handler(clientFd, executor);
    
    // Send command and close write side
    socketPair.writeToSocket("GET file.txt\n");
    socketPair.closeWriteSocket();
    
    std::thread handlerThread([&handler]() {
        handler.handle();
    });
    
    handlerThread.join();
    
    // Socket should be closed (read should return error, not EOF)
    // When socket is closed, read() returns -1 with errno = EBADF
    char buffer[1];
    ssize_t result = read(clientFd, buffer, 1);
    EXPECT_EQ(result, -1); // Error because socket is closed
}

