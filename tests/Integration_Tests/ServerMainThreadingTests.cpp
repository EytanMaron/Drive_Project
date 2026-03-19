/**
 * @file ServerMainThreadingTests.cpp
 * @brief Integration tests for ServerMain thread pool functionality.
 * 
 * These tests verify that ServerMain properly handles multiple concurrent clients
 * using a thread pool for efficient request processing.
 * 
 * Testing Strategy:
 * - Tests use real TCP connections (not mocks)
 * - Tests will FAIL until ServerMain implements the accept loop with threading
 * - Tests will PASS once ServerMain implements threading
 * - The test itself does NOT change between RED and GREEN phases
 * 
 * How it works:
 * - The test starts a server and connects multiple clients
 * - Clients send commands concurrently
 * - Test verifies commands are handled (responses received)
 * - If ServerMain doesn't implement threading, commands won't be handled and test FAILS
 * - If ServerMain implements threading, commands are handled and test PASSES
 */

#include <gtest/gtest.h>
#include "Server/TcpServer.h"
#include "Server/ServerAcceptLoop.h"
#include "Server/ThreadPool.h"
#include "Commands/CommandExecutor.h"
#include "Core/CommandBuilder.h"
#include "Services/RleCompressor.h"
#include "Services/FileSystemStorage.h"
#include "Services/EnvVarConfig.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <string>
#include <cstring>
#include <filesystem>
#include <cstdlib>

// Helper function to find an available port for testing
static int findAvailablePort() {
    int testSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (testSocket == -1) {
        return 0;
    }
    
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;  // Let OS choose port
    
    if (bind(testSocket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(testSocket);
        return 0;
    }
    
    socklen_t len = sizeof(addr);
    if (getsockname(testSocket, (struct sockaddr*)&addr, &len) == -1) {
        close(testSocket);
        return 0;
    }
    
    int port = ntohs(addr.sin_port);
    close(testSocket);
    return port;
}

// Helper function to create a test client connection
static int connectToServer(const std::string& host, int port) {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        return -1;
    }
    
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        close(clientSocket);
        return -1;
    }
    
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        close(clientSocket);
        return -1;
    }
    
    return clientSocket;
}

// Helper function to send data over socket
static bool sendData(int socketFd, const std::string& data) {
    std::string dataWithNewline = data + "\n";
    ssize_t sent = send(socketFd, dataWithNewline.c_str(), dataWithNewline.length(), 0);
    return sent == static_cast<ssize_t>(dataWithNewline.length());
}

// Helper function to receive data from socket (with timeout)
static bool receiveData(int socketFd, std::string& buffer, int timeoutMs = 2000) {
    fd_set readfds;
    struct timeval timeout;
    
    FD_ZERO(&readfds);
    FD_SET(socketFd, &readfds);
    
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    
    int selectResult = select(socketFd + 1, &readfds, nullptr, nullptr, &timeout);
    if (selectResult <= 0) {
        return false;  // Timeout or error
    }
    
    char buf[1024];
    ssize_t received = recv(socketFd, buf, sizeof(buf) - 1, 0);
    if (received <= 0) {
        return false;
    }
    
    buf[received] = '\0';
    buffer = std::string(buf);
    return true;
}

/**
 * @brief Test fixture for ServerMain threading tests.
 * Sets up a testable server environment similar to what ServerMain creates.
 */
class ServerMainThreadingTest : public ::testing::Test {
protected:
    int m_port;
    std::string m_testStorageDir;
    std::string m_originalStorageDir;
    
    // Shared services (like ServerMain will create)
    std::shared_ptr<RleCompressor> m_compressor;
    std::shared_ptr<FileSystemStorage> m_storage;
    std::shared_ptr<EnvVarConfig> m_config;
    std::shared_ptr<CommandBuilder> m_builder;
    std::shared_ptr<CommandExecutor> m_executor;
    
    void SetUp() override {
        // Find available port
        m_port = findAvailablePort();
        ASSERT_GT(m_port, 0) << "Could not find available port for testing";
        
        // Create temporary storage directory
        m_testStorageDir = "/tmp/server_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(m_testStorageDir);
        
        // Save and set STORAGE_DIR environment variable
        const char* originalDir = std::getenv("STORAGE_DIR");
        if (originalDir) {
            m_originalStorageDir = originalDir;
        }
        setenv("STORAGE_DIR", m_testStorageDir.c_str(), 1);
        
        // Create shared services (like ServerMain will do)
        m_compressor = std::make_shared<RleCompressor>();
        m_storage = std::make_shared<FileSystemStorage>();
        m_config = std::make_shared<EnvVarConfig>();
        m_builder = std::make_shared<CommandBuilder>(m_compressor, m_storage, m_config);
        m_executor = std::make_shared<CommandExecutor>(m_builder);
    }
    
    void TearDown() override {
        // Restore original STORAGE_DIR
        if (!m_originalStorageDir.empty()) {
            setenv("STORAGE_DIR", m_originalStorageDir.c_str(), 1);
        } else {
            unsetenv("STORAGE_DIR");
        }
        
        // Clean up test directory
        if (std::filesystem::exists(m_testStorageDir)) {
            std::filesystem::remove_all(m_testStorageDir);
        }
    }
};

// Test: Multiple clients can send commands concurrently and receive responses
//
// How it works:
// 1. Starts a TcpServer (like ServerMain does)
// 2. Starts an accept loop that simulates what ServerMain SHOULD do (but doesn't yet)
// 3. Connects multiple clients and sends commands concurrently
// 4. Verifies commands are handled (responses received)
//
// RED phase: ServerMain's accept loop is empty → commands not handled → test FAILS
// GREEN phase: ServerMain implements threading → commands handled → test PASSES
TEST_F(ServerMainThreadingTest, HandlesConcurrentCommandsFromMultipleClients) {
    TcpServer server(m_port);
    server.start();
    
    // Start accept loop using the SAME function that ServerMain uses
    // This test verifies that runServerAcceptLoop() handles concurrent clients correctly
    std::atomic<bool> serverRunning(true);
    ThreadPool threadPool(4);  // Use smaller pool for tests
    std::thread serverThread([&]() {
        // Call the same function that ServerMain calls
        runServerAcceptLoop(server, m_executor, threadPool);
    });
    
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Connect multiple clients
    const int numClients = 3;
    std::vector<int> clientSockets;
    
    for (int i = 0; i < numClients; ++i) {
        int clientSocket = connectToServer("127.0.0.1", m_port);
        ASSERT_GE(clientSocket, 0) << "Failed to connect client " << i;
        clientSockets.push_back(clientSocket);
    }
    
    // Send commands from all clients simultaneously
    std::vector<std::thread> clientThreads;
    std::atomic<int> successfulCommands(0);
    
    for (size_t i = 0; i < clientSockets.size(); ++i) {
        clientThreads.emplace_back([&, i]() {
            std::string filename = "test_file_" + std::to_string(i) + ".txt";
            std::string command = "POST " + filename + " content" + std::to_string(i);
            
            if (sendData(clientSockets[i], command)) {
                std::string response;
                if (receiveData(clientSockets[i], response, 3000)) {
                    // Verify response contains expected status code
                    if (response.find("201") != std::string::npos) {
                        successfulCommands++;
                    }
                }
            }
        });
    }
    
    // Wait for all commands to complete
    for (auto& thread : clientThreads) {
        thread.join();
    }
    
    // VERIFY: All commands should succeed concurrently
    EXPECT_EQ(successfulCommands.load(), numClients) 
        << "ServerMain should handle concurrent commands from multiple clients using threads";
    
    // Clean up
    for (int socketFd : clientSockets) {
        close(socketFd);
    }
    
    serverRunning = false;
    threadPool.shutdown();
    server.stop();
    
    // Wait for server thread with timeout
    auto start = std::chrono::steady_clock::now();
    while (serverThread.joinable()) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(2)) {
            serverThread.detach();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (serverThread.joinable()) {
        serverThread.join();
    }
}

