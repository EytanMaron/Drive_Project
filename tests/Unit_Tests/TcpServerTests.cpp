/**
 * TcpServer Unit Tests
 * 
 * These tests are written before implementing TcpServer class,
 * and are designed to validate its basic listening functionality once implemented.
 * We make sure tests fail before implementation of TcpServer (red phase).
 * 
 * Testing Strategy:
 * - Tests socket operations: socket(), bind(), listen(), accept()
 * - Tests error handling: invalid ports, port already in use, etc.
 * - Tests connection acceptance with real client connections
 */

#include <gtest/gtest.h>
#include "TcpServer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>

// Helper function to find an available port for testing
int findAvailablePort() {
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
int connectToServer(const std::string& host, int port) {
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

//  TcpServer can start listening on a valid port
TEST(TcpServerTest, CanStartListening) {
    int port = findAvailablePort();
    ASSERT_GT(port, 0) << "Could not find available port for testing";
    
    TcpServer server(port);
    EXPECT_NO_THROW({
        server.start();
    });
    
    EXPECT_TRUE(server.isListening());
    EXPECT_EQ(server.getPort(), port);
    
    server.stop();
}

//  TcpServer throws exception when trying to start listening twice
TEST(TcpServerTest, ThrowsOnDoubleStart) {
    int port = findAvailablePort();
    ASSERT_GT(port, 0) << "Could not find available port for testing";
    
    TcpServer server(port);
    server.start();
    
    EXPECT_THROW({
        server.start();  // Try to start again
    }, std::runtime_error);
    
    server.stop();
}

//  TcpServer can stop listening and close server socket
TEST(TcpServerTest, CanStopListening) {
    int port = findAvailablePort();
    ASSERT_GT(port, 0) << "Could not find available port for testing";
    
    TcpServer server(port);
    server.start();
    EXPECT_TRUE(server.isListening());
    
    server.stop();
    EXPECT_FALSE(server.isListening());
}

//  TcpServer can accept a single client connection
TEST(TcpServerTest, AcceptsSingleClientConnection) {
    int port = findAvailablePort();
    ASSERT_GT(port, 0) << "Could not find available port for testing";
    
    TcpServer server(port);
    server.start();
    
    // Connect a client in a separate thread (accept blocks)
    int clientSocket = -1;
    std::thread clientThread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        clientSocket = connectToServer("127.0.0.1", port);
    });
    
    // Accept the connection
    int acceptedSocket = server.acceptConnection();
    EXPECT_GE(acceptedSocket, 0) << "Failed to accept client connection";
    
    clientThread.join();
    
    // Clean up
    if (acceptedSocket >= 0) {
        close(acceptedSocket);
    }
    if (clientSocket >= 0) {
        close(clientSocket);
    }
    
    server.stop();
}

//  TcpServer handles port already in use error 
TEST(TcpServerTest, HandlesPortAlreadyInUse) {
    int port = findAvailablePort();
    ASSERT_GT(port, 0) << "Could not find available port for testing";
    
    // Create first server and start it
    TcpServer server1(port);
    server1.start();
    
    // Try to create second server on same port
    TcpServer server2(port);
    EXPECT_THROW({
        server2.start();
    }, std::runtime_error);
    
    server1.stop();
}

//  TcpServer can reuse port after stopping (SO_REUSEADDR)
TEST(TcpServerTest, CanReusePortAfterStopping) {
    int port = findAvailablePort();
    ASSERT_GT(port, 0) << "Could not find available port for testing";
    
    // Start and stop first server
    {
        TcpServer server1(port);
        server1.start();
        EXPECT_TRUE(server1.isListening());
        server1.stop();
    }
    
    // Small delay to ensure port is released
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Should be able to start new server on same port
    TcpServer server2(port);
    EXPECT_NO_THROW({
        server2.start();
    });
    EXPECT_TRUE(server2.isListening());
    
    server2.stop();
}

//  TcpServer properly closes socket in destructor
TEST(TcpServerTest, ClosesSocketInDestructor) {
    int port = findAvailablePort();
    ASSERT_GT(port, 0) << "Could not find available port for testing";
    
    {
        TcpServer server(port);
        server.start();
        EXPECT_TRUE(server.isListening());
        // Server goes out of scope, destructor should close socket
    }
    
    // Small delay to ensure socket is closed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Should be able to create new server on same port
    TcpServer server2(port);
    EXPECT_NO_THROW({
        server2.start();
    });
    server2.stop();
}
