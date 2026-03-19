/**
 * CommandMenu Unit Tests (Socket Input)
 * 
 * These tests verify that CommandMenu works correctly with SocketInput.
 * CommandMenu is a generic menu that works with any IInput implementation.
 * 
 * Testing Strategy:
 * - Uses socketpair() to create connected socket pairs for testing
 * - SocketInput reads from one socket, we write test data to the other
 * - Tests cover core functionality: parsing, edge cases
 */

#include <gtest/gtest.h>
#include "CommandMenu.h"
#include "SocketInput.h"
#include "IMenu.h"
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <utility>

// Helper class to manage socket pairs for testing
class SocketPairHelper {
public:
    // Constructor to create the socket pair
    SocketPairHelper() {
        int sockets[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == -1) {
            throw std::runtime_error("Failed to create socket pair");
        }
        m_writeFd = sockets[0];
        m_readFd = sockets[1];
    }

    // Destructor to close the sockets
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

private:
    int m_writeFd = -1;
    int m_readFd = -1;
};

// Test: CommandMenu correctly implements IMenu interface (polymorphism)
TEST(CommandMenuSocketTest, ImplementsIMenuInterface) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("POST file.txt hello\n");
    
    SocketInput socketInput(socketPair.getReadFd());
    CommandMenu menu(&socketInput);
    IMenu* menuPtr = &menu;
    
    std::pair<std::string, std::string> result = menuPtr->next();
    EXPECT_EQ(result.first, "POST");
    EXPECT_EQ(result.second, "file.txt hello");
}

// Test: CommandMenu separates command and data by first space
TEST(CommandMenuSocketTest, SeparatesCommandAndDataByFirstSpace) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("POST file.txt hello world\n");
    
    SocketInput socketInput(socketPair.getReadFd());
    CommandMenu menu(&socketInput);
    
    std::pair<std::string, std::string> result = menu.next();
    
    EXPECT_EQ(result.first, "POST");
    EXPECT_EQ(result.second, "file.txt hello world");
}

// Test: CommandMenu handles command without data (single word)
TEST(CommandMenuSocketTest, HandlesCommandWithoutData) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("GET\n");
    
    SocketInput socketInput(socketPair.getReadFd());
    CommandMenu menu(&socketInput);
    
    std::pair<std::string, std::string> result = menu.next();
    
    EXPECT_EQ(result.first, "GET");
    EXPECT_EQ(result.second, "");
}

// Test: CommandMenu splits only at first space (multiple spaces in data)
TEST(CommandMenuSocketTest, SplitsOnlyAtFirstSpace) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("SEARCH text with multiple spaces\n");
    
    SocketInput socketInput(socketPair.getReadFd());
    CommandMenu menu(&socketInput);
    
    std::pair<std::string, std::string> result = menu.next();
    
    EXPECT_EQ(result.first, "SEARCH");
    EXPECT_EQ(result.second, "text with multiple spaces");
}

// Test: CommandMenu handles empty line
TEST(CommandMenuSocketTest, HandlesEmptyLine) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("\n");
    
    SocketInput socketInput(socketPair.getReadFd());
    CommandMenu menu(&socketInput);
    
    std::pair<std::string, std::string> result = menu.next();
    
    EXPECT_EQ(result.first, "");
    EXPECT_EQ(result.second, "");
}

// Test: CommandMenu handles multiple sequential commands
TEST(CommandMenuSocketTest, HandlesMultipleSequentialCommands) {
    SocketPairHelper socketPair;
    socketPair.writeToSocket("POST file1.txt content1\nGET file1.txt\nDELETE file1.txt\n");
    
    SocketInput socketInput(socketPair.getReadFd());
    CommandMenu menu(&socketInput);
    
    std::pair<std::string, std::string> result1 = menu.next();
    EXPECT_EQ(result1.first, "POST");
    EXPECT_EQ(result1.second, "file1.txt content1");
    
    std::pair<std::string, std::string> result2 = menu.next();
    EXPECT_EQ(result2.first, "GET");
    EXPECT_EQ(result2.second, "file1.txt");
    
    std::pair<std::string, std::string> result3 = menu.next();
    EXPECT_EQ(result3.first, "DELETE");
    EXPECT_EQ(result3.second, "file1.txt");
}

