/**
 * These tests are written before implementing SocketInput class,
 * and are designed to validate its functionality once implemented.
 * we make sure tests fail before implementation of SocketInput (red phase).
 * Testing Strategy:
 * - Uses socketpair() to create connected socket pairs for testing
 * - One socket is used to write test data, the other to read (via SocketInput)
 * - Tests cover various scenarios: single lines, multiple lines, errors, edge cases
 */

#include <gtest/gtest.h>
#include "SocketInput.h"
#include "IInput.h"
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <cstring>

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

    // Get the file descriptor for reading (used by SocketInput)
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

// Test: SocketInput reads a single line correctly
TEST(SocketInputTest, ReadsSingleValidLine) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("POST file.txt hello\n");
    
    SocketInput input(socketPair.getReadFd());
    std::string line = input.readLine();
    
    EXPECT_EQ(line, "POST file.txt hello");
}

// Test: SocketInput reads multiple lines sequentially
TEST(SocketInputTest, ReadsMultipleLinesSequentially) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("POST file1.txt hello\nGET file1.txt\n");
    
    SocketInput input(socketPair.getReadFd());
    
    std::string line1 = input.readLine();
    EXPECT_EQ(line1, "POST file1.txt hello");
    
    std::string line2 = input.readLine();
    EXPECT_EQ(line2, "GET file1.txt");
}

// Test: SocketInput handles empty lines (newline only)
TEST(SocketInputTest, HandlesEmptyLine) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("\n");
    
    SocketInput input(socketPair.getReadFd());
    std::string line = input.readLine();
    
    EXPECT_EQ(line, "");
}

// Test: SocketInput handles lines with only whitespace
TEST(SocketInputTest, HandlesWhitespaceOnlyLine) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("   \n");
    
    SocketInput input(socketPair.getReadFd());
    std::string line = input.readLine();
    
    EXPECT_EQ(line, "   ");
}

// Test: SocketInput correctly implements IInput interface (polymorphism)
TEST(SocketInputTest, ImplementsIInputInterface) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("test line\n");
    
    SocketInput socketInput(socketPair.getReadFd());
    IInput* input = &socketInput;  // Should work via polymorphism
    
    std::string line = input->readLine();
    EXPECT_EQ(line, "test line");
}

// Test: SocketInput throws exception on closed socket
TEST(SocketInputTest, ThrowsOnClosedSocket) {
    SocketPairHelper socketPair;
    socketPair.closeWriteSocket();  // Close the write end
    
    SocketInput input(socketPair.getReadFd());
    
    EXPECT_THROW({
        input.readLine();
    }, std::runtime_error);
}

// Test: SocketInput handles partial reads (data arrives in chunks)
TEST(SocketInputTest, HandlesPartialReads) {
    SocketPairHelper socketPair;
    
    // Write data in two separate chunks to simulate network behavior
    socketPair.writeToSocket("POST file");
    // Small delay simulation - in real scenario, data might arrive later
    socketPair.writeToSocket(".txt hello\n");
    
    SocketInput input(socketPair.getReadFd());
    std::string line = input.readLine();
    
    EXPECT_EQ(line, "POST file.txt hello");
}

// Test: SocketInput handles long lines (longer than buffer)
TEST(SocketInputTest, HandlesLongLine) {
    SocketPairHelper socketPair;
    
    // Create a long line (longer than socket buffer)
    std::string longLine(5000, 'a');
    longLine += "\n";
    socketPair.writeToSocket(longLine);
    
    SocketInput input(socketPair.getReadFd());
    std::string line = input.readLine();
    
    EXPECT_EQ(line, std::string(5000, 'a'));
}

// Test: SocketInput handles multiple empty lines
TEST(SocketInputTest, HandlesMultipleEmptyLines) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("\n\n\n");
    
    SocketInput input(socketPair.getReadFd());
    
    std::string line1 = input.readLine();
    EXPECT_EQ(line1, "");
    
    std::string line2 = input.readLine();
    EXPECT_EQ(line2, "");
    
    std::string line3 = input.readLine();
    EXPECT_EQ(line3, "");
}

// Test: SocketInput handles lines with special characters
TEST(SocketInputTest, HandlesSpecialCharacters) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("POST file.txt hello\tworld\n");
    
    SocketInput input(socketPair.getReadFd());
    std::string line = input.readLine();
    
    EXPECT_EQ(line, "POST file.txt hello\tworld");
}

// Test: SocketInput handles lines with unicode/UTF-8 characters
TEST(SocketInputTest, HandlesUnicodeCharacters) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("POST file.txt שלום\n");
    
    SocketInput input(socketPair.getReadFd());
    std::string line = input.readLine();
    
    EXPECT_EQ(line, "POST file.txt שלום");
}

// Test: SocketInput correctly strips newline character
TEST(SocketInputTest, StripsNewlineCharacter) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("test\n");
    
    SocketInput input(socketPair.getReadFd());
    std::string line = input.readLine();
    
    // Should not contain the newline character
    EXPECT_EQ(line, "test");
    EXPECT_NE(line, "test\n");
}

// Test: SocketInput handles mixed case commands
TEST(SocketInputTest, HandlesMixedCaseCommands) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("PoSt FiLe.TxT HeLlO\n");
    
    SocketInput input(socketPair.getReadFd());
    std::string line = input.readLine();
    
    EXPECT_EQ(line, "PoSt FiLe.TxT HeLlO");
}

// Test: SocketInput blocks until newline is received
// Note: This test verifies that readLine waits for complete line
TEST(SocketInputTest, WaitsForCompleteLine) {
    SocketPairHelper socketPair;
    
    // Write data without newline first
    socketPair.writeToSocket("POST file.txt");
    
    // In a separate operation, add the newline
    // This simulates data arriving in separate network packets
    socketPair.writeToSocket(" hello\n");
    
    SocketInput input(socketPair.getReadFd());
    std::string line = input.readLine();
    
    EXPECT_EQ(line, "POST file.txt hello");
}

// Test: SocketInput handles search command format
TEST(SocketInputTest, HandlesSearchCommand) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("SEARCH hello world\n");
    
    SocketInput input(socketPair.getReadFd());
    std::string line = input.readLine();
    
    EXPECT_EQ(line, "SEARCH hello world");
}

// Test: SocketInput handles DELETE command format
TEST(SocketInputTest, HandlesDeleteCommand) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("DELETE file.txt\n");
    
    SocketInput input(socketPair.getReadFd());
    std::string line = input.readLine();
    
    EXPECT_EQ(line, "DELETE file.txt");
}

// Test: SocketInput throws exception for invalid socket file descriptor
TEST(SocketInputTest, ThrowsOnInvalidSocketFd) {
    EXPECT_THROW({
        SocketInput input(-1);
    }, std::runtime_error);
}

// Test: SocketInput handles CRLF line endings (Windows/HTTP style)
TEST(SocketInputTest, HandlesCRLFLineEndings) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("POST file.txt hello\r\n");
    
    SocketInput input(socketPair.getReadFd());
    std::string line = input.readLine();
    
    // Should strip the \r and return only the line content
    EXPECT_EQ(line, "POST file.txt hello");
    EXPECT_NE(line, "POST file.txt hello\r");
}



