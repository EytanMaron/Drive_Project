#include "gtest/gtest.h"
#include "GetCommand.h"

// Integration Test: using Real implementations instead of Mocks.
// We are testing the actual integration between the modules (Config -> Storage -> Compressor -> Output).
#include "RleCompressor.h"
#include "FileSystemStorage.h"
#include "EnvVarConfig.h"
#include "ConsoleOutput.h"

#include <fstream>
#include <stdlib.h> // For setenv
#include <cstdio>   // For remove
#include <iostream>

class GetCommandIntegrationTest : public ::testing::Test {
protected:
    const std::string TEST_FILE = "integration_test.txt";
    
    // Instantiating real dependencies on the stack (RAII).
    // No need for new/delete, keeping memory management safe.
    RleCompressor compressor;
    FileSystemStorage storage;
    EnvVarConfig config;
    
    // Capturing output:
    // We inject a stringstream into ConsoleOutput to verify what is actually printed to the user.
    std::stringstream outputBuffer;
    std::unique_ptr<ConsoleOutput> consoleOutput;

    void SetUp() override {
        // 1. Environment Configuration.
        // We must set the exact Env Var key that EnvVarConfig expects ("STORAGE_DIR").
        // Using "." (current directory) to avoid permission issues on the testing server.
        setenv("STORAGE_DIR", ".", 1);

        // 2. Output Injection.
        consoleOutput = std::make_unique<ConsoleOutput>(outputBuffer);
    }

    void TearDown() override {
        // Cleanup: Remove the test file to ensure a clean state for the next test.
        // This prevents "file already exists" errors in subsequent runs.
        try {
            std::string path = config.getStoragePath();
            // Manual path construction required here
            if (!path.empty() && path.back() != '/') path += "/";
            std::string fullPath = path + TEST_FILE;
            std::remove(fullPath.c_str());
        } catch (...) {
            // Best effort cleanup. If Config fails, we can't delete the file, but we shouldn't crash the test.
        }
    }

    // Helper function to create input files.
    void createDummyFile(const std::string& content) {
        // Critical: We ask the config object for the path.
        // This ensures the Test (writer) and the App (reader) are synchronized on the directory location.
        std::string path = config.getStoragePath();
        
        if (!path.empty() && path.back() != '/') {
            path += "/";
        }

        std::string fullPath = path + TEST_FILE;
        
        std::ofstream outFile(fullPath);
        if (!outFile.is_open()) {
             // Logging error to stderr for debugging permissions issues.
            std::cerr << "Test Error: Cannot write to " << fullPath << std::endl;
        }
        outFile << content;
        outFile.close();
    }
};

// --- Test Cases ---

// Test 1: Standard Flow (The Happy Path)
// Verifies that a standard compressed file is correctly read, decompressed, and printed.
TEST_F(GetCommandIntegrationTest, Flow_ValidFile_DecompressesCorrectly) {
    // Setup: "a3b2" should decompress to "aaabb"
    createDummyFile("a3b2");

    // Execution: injecting real pointers.
    GetCommand cmd(&compressor, &storage, &config, consoleOutput.get());
    std::string args = TEST_FILE;
    cmd.execute(args);

    // Verification
    std::string expected = "aaabb\n";
    EXPECT_EQ(outputBuffer.str(), expected);
}

// Test 2: Complex Logic / Edge Case
// Verifies the specific RLE requirements involving digits and escape characters.
TEST_F(GetCommandIntegrationTest, Flow_ComplexContent_WithDigits) {
    // Input: "X2@12" 
    // Logic: 
    // X2 -> XX
    // @1 -> 1 (Escape char handled)
    // 2 -> count of previous char (1) => 11
    // Expected Result: "XX11"

    createDummyFile("X2@12");

    GetCommand cmd(&compressor, &storage, &config, consoleOutput.get());
    cmd.execute(TEST_FILE);

    EXPECT_EQ(outputBuffer.str(), "XX11\n");
}

// Test 3: Negative Test - File Not Found
// Verifies that the system correctly propagates exceptions when storage fails.
TEST_F(GetCommandIntegrationTest, Flow_FileNotFound_ThrowsException) {
    // Ensure file does not exist
    TearDown();

    GetCommand cmd(&compressor, &storage, &config, consoleOutput.get());
    
    // We expect the Command to let the storage exception bubble up.
    // The Main loop is responsible for catching this.
    EXPECT_THROW(cmd.execute(TEST_FILE), std::out_of_range);
}

// Test 4: Negative Test - Configuration Error
// Verifies behavior when the mandatory Environment Variable is missing.
TEST_F(GetCommandIntegrationTest, Flow_EnvVarNotSet_ThrowsException) {
    // Simulation: Unsetting the variable to mimic a bad environment.
    unsetenv("STORAGE_DIR");

    GetCommand cmd(&compressor, &storage, &config, consoleOutput.get());
    
    // EnvVarConfig should throw immediately when accessed.
    EXPECT_THROW(cmd.execute(TEST_FILE), std::runtime_error);
}