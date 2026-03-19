/**
 * SocketOutput Unit Tests 
 * 
 * These tests are written before implementing SocketOutput class,
 * and are designed to validate its functionality once implemented.
 * We make sure tests fail before implementation of SocketOutput (red phase).
 * 
 * Testing Strategy:
 * - Uses socketpair() to create connected socket pairs for testing
 * - SocketOutput writes to one socket, we read from the other to verify
 * - Tests cover various scenarios: single writes, multiple writes, errors, edge cases
 */

#include <gtest/gtest.h>
#include "SocketOutput.h"
#include "IOutput.h"
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include <string>
#include <cstring>
#include <algorithm>  

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
        
        // Make read socket non-blocking to avoid hanging on empty reads
        int flags = fcntl(m_readFd, F_GETFL, 0);
        if (flags == -1 || fcntl(m_readFd, F_SETFL, flags | O_NONBLOCK) == -1) {
            close(m_writeFd);
            close(m_readFd);
            throw std::runtime_error("Failed to set socket to non-blocking mode");
        }
    }

    ~SocketPairHelper() {
        if (m_writeFd != -1) close(m_writeFd);
        if (m_readFd != -1) close(m_readFd);
    }

    // Prevent copying (manages file descriptors)
    SocketPairHelper(const SocketPairHelper&) = delete;
    SocketPairHelper& operator=(const SocketPairHelper&) = delete;

    // Get the file descriptor for writing (used by SocketOutput)
    int getWriteFd() const { return m_writeFd; }

    // Get the file descriptor for reading (used to verify written data)
    int getReadFd() const { return m_readFd; }

    // Check if data is available on the socket
    bool hasDataAvailable() {
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(m_readFd, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;  // Immediate check
        
        int result = select(m_readFd + 1, &readfds, nullptr, nullptr, &timeout);
        return result > 0 && FD_ISSET(m_readFd, &readfds);
    }

    // Read all available data from the socket (to verify what SocketOutput wrote)
    std::string readFromSocket(size_t maxBytes = 4096) {
        std::string result;
        char buffer[4096];
        
        // Read in a loop until no more data is available or we've read maxBytes
        while (result.length() < maxBytes) {
            // For non-blocking socket, check if data is available first
            if (!hasDataAvailable()) {
                // No data available - if we've read something, return it
                // Otherwise, return empty (no data was written)
                break;
            }
            
            size_t toRead = std::min(maxBytes - result.length(), sizeof(buffer));
            ssize_t bytesRead = read(m_readFd, buffer, toRead);
            
            if (bytesRead < 0) {
                // If non-blocking and no data available, return what we have
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;  // No more data available
                }
                throw std::runtime_error("Failed to read from test socket");
            }
            
            if (bytesRead == 0) {
                break;  
            }
            
            result.append(buffer, bytesRead);
        }
        
        return result;
    }

    // Close the read socket to simulate connection closure
    void closeReadSocket() {
        if (m_readFd != -1) {
            close(m_readFd);
            m_readFd = -1;
        }
    }

private:
    int m_writeFd = -1;
    int m_readFd = -1;
};

// Test: SocketOutput writes a single message correctly
TEST(SocketOutputTest, WritesSingleMessage) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("201 Created\n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "201 Created\n");
}

// Test: SocketOutput writes multiple messages sequentially
TEST(SocketOutputTest, WritesMultipleMessagesSequentially) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("200 Ok\n");
    output.write("Hello World\n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "200 Ok\nHello World\n");
}

// Test: SocketOutput handles empty strings
TEST(SocketOutputTest, HandlesEmptyString) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    // Writing empty string should not throw and should write nothing
    EXPECT_NO_THROW({
        output.write("");
    });
    
    // Verify nothing was written by checking no data is available
    EXPECT_FALSE(socketPair.hasDataAvailable());
}

// Test: SocketOutput correctly implements IOutput interface (polymorphism)
TEST(SocketOutputTest, ImplementsIOutputInterface) {
    SocketPairHelper socketPair;
    
    SocketOutput socketOutput(socketPair.getWriteFd());
    const IOutput* output = &socketOutput;  // Should work via polymorphism
    
    output->write("test message\n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "test message\n");
}

// Test: SocketOutput throws exception on closed socket
TEST(SocketOutputTest, ThrowsOnClosedSocket) {
    SocketPairHelper socketPair;
    socketPair.closeReadSocket();  // Close the read end
    
    SocketOutput output(socketPair.getWriteFd());
    
    EXPECT_THROW({
        output.write("test");
    }, std::runtime_error);
}

// Test: SocketOutput handles long messages (longer than socket buffer)
TEST(SocketOutputTest, HandlesLongMessage) {
    SocketPairHelper socketPair;
    
    // Create a long message (longer than typical socket buffer)
    std::string longMessage(5000, 'a');
    longMessage += "\n";
    
    SocketOutput output(socketPair.getWriteFd());
    output.write(longMessage);
    
    std::string received = socketPair.readFromSocket(6000);
    EXPECT_EQ(received, longMessage);
}

// Test: SocketOutput writes messages with special characters
TEST(SocketOutputTest, HandlesSpecialCharacters) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("POST file.txt hello\tworld\n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "POST file.txt hello\tworld\n");
}

// Test: SocketOutput writes Unicode/UTF-8 characters
TEST(SocketOutputTest, HandlesUnicodeCharacters) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("POST file.txt שלום\n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "POST file.txt שלום\n");
}

// Test: SocketOutput writes status codes
TEST(SocketOutputTest, WritesStatusCodes) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("200 Ok\n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "200 Ok\n");
}

// Test: SocketOutput writes POST response format
TEST(SocketOutputTest, WritesPostResponse) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("201 Created\n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "201 Created\n");
}

// Test: SocketOutput writes DELETE response format
TEST(SocketOutputTest, WritesDeleteResponse) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("204 No Content\n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "204 No Content\n");
}

// Test: SocketOutput writes GET response format with content
TEST(SocketOutputTest, WritesGetResponseWithContent) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("200 Ok\n\nHello World\n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "200 Ok\n\nHello World\n");
}

// Test: SocketOutput writes error responses
TEST(SocketOutputTest, WritesErrorResponses) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("400 Bad Request\n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "400 Bad Request\n");
}

// Test: SocketOutput writes 404 Not Found response
TEST(SocketOutputTest, WritesNotFoundResponse) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("404 Not Found\n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "404 Not Found\n");
}

// Test: SocketOutput writes messages with newlines correctly
TEST(SocketOutputTest, HandlesNewlinesCorrectly) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("Line 1\nLine 2\nLine 3\n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "Line 1\nLine 2\nLine 3\n");
}

// Test: SocketOutput writes messages without newlines
TEST(SocketOutputTest, WritesMessageWithoutNewline) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("No newline here");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "No newline here");
}

// Test: SocketOutput handles whitespace-only messages
TEST(SocketOutputTest, HandlesWhitespaceOnlyMessage) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("   \n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "   \n");
}

// Test: SocketOutput writes complete SEARCH response format
TEST(SocketOutputTest, WritesSearchResponseFormat) {
    SocketPairHelper socketPair;
    
    SocketOutput output(socketPair.getWriteFd());
    output.write("200 Ok\n\nfile1.txt\nfile2.txt\n");
    
    std::string received = socketPair.readFromSocket();
    EXPECT_EQ(received, "200 Ok\n\nfile1.txt\nfile2.txt\n");
}

// Test: SocketOutput ensures all data is written (handles partial writes)
TEST(SocketOutputTest, HandlesPartialWrites) {
    SocketPairHelper socketPair;
    
    // Create a message that might require multiple write calls
    std::string largeMessage(10000, 'x');
    largeMessage += "\n";
    
    SocketOutput output(socketPair.getWriteFd());
    output.write(largeMessage);
    
    // Read all data (readFromSocket now handles reading in chunks)
    std::string received = socketPair.readFromSocket(largeMessage.length() + 100);
    
    EXPECT_EQ(received, largeMessage);
}

