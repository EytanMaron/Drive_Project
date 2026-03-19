#include <gtest/gtest.h>
#include "FileSystemStorage.h"
#include <string>
#include <cstdio>
#include <filesystem>
#include <algorithm>
using namespace std;


// Sanity check - test writing and reading a file using FileSystemStorage
TEST(FileSystemStorage, WriteReadFile) {
    FileSystemStorage storage;

    string testPath = "testfile.txt";
    string testContent = "Hello, World!";
    
    storage.write(testPath, testContent);
    string content = storage.read(testPath);
    
    EXPECT_EQ(content, testContent);

    // Clean up
    filesystem::remove(testPath);
}

// Sanity check - test listing files in FileSystemStorage
TEST(FileSystemStorage, ListFiles) {
    FileSystemStorage storage;

    string testDir = "testdir";
    filesystem::create_directory(testDir);
    
    // Create some test files
    vector<string> testFiles = {"file1.txt", "file2.txt", "file3.txt"};
    for (const auto& file : testFiles) {
        storage.write(testDir + "/" + file, "Test content");
    }
    
    vector<string> files = storage.listFiles(testDir);
    
    // Sort both vectors to ensure order doesn't affect the test
    sort(files.begin(), files.end());
    sort(testFiles.begin(), testFiles.end());
    
    EXPECT_EQ(files, testFiles);
    
    // Clean up
    for (const auto& file : testFiles) {
        filesystem::remove(testDir + "/" + file);
    }
    filesystem::remove(testDir);
}

//Negative test - reading a non-existent file should return error
TEST(FileSystemStorage, ReadNonExistentFile){  
    FileSystemStorage storage;
    string testPath = "non_existent_file.txt";
    
    EXPECT_THROW({
        storage.read(testPath);
    }, out_of_range);
}

// Boundary test - writing an empty file should create a 0-byte file on disk
TEST(FileSystemStorage, WriteReadEmptyFile) {
    FileSystemStorage storage;
    string testPath = "emptyfile.txt";
    string testContent = "";
    
    storage.write(testPath, testContent);

    // Verify PHYSICAL existence
    EXPECT_TRUE(filesystem::exists(testPath)); 
    
    // Verify file size is exactly 0
    EXPECT_EQ(filesystem::file_size(testPath), 0);

    // Verify your read logic handles it correctly
    string content = storage.read(testPath);
    EXPECT_EQ(content, testContent);

    // Cleanup
    filesystem::remove(testPath);
}


// Boundary test - writing and reading a large file (10MB)
TEST(FileSystemStorage, WriteReadLargeFile) {
    FileSystemStorage storage;
    string testPath = "largefile.txt";
    
    // Create 10MB content
    string testContent(10 * 1024 * 1024, 'A'); 
    
    storage.write(testPath, testContent);
    string content = storage.read(testPath);
    
    // Check size first (Printable and informative)
    ASSERT_EQ(content.size(), testContent.size()) << "File size mismatch!";

    // Check content using EXPECT_TRUE to avoid printing 10MB to console
    // I add a custom message using "<<" to know what happened
    EXPECT_TRUE(content == testContent) << "Content mismatch! (Output suppressed due to length)";

    // Cleanup
    filesystem::remove(testPath);
}

// Test removing an existing file
TEST(FileSystemStorage, RemoveExistingFile) {
    FileSystemStorage storage;
    string testPath = "file_to_delete.txt";
    
    // Create a file first
    storage.write(testPath, "content to delete");
    ASSERT_TRUE(filesystem::exists(testPath));
    
    // Remove the file
    storage.remove(testPath);
    
    // Verify file no longer exists
    EXPECT_FALSE(filesystem::exists(testPath));
}

// Test removing a non-existent file throws exception
TEST(FileSystemStorage, RemoveNonExistentFile) {
    FileSystemStorage storage;
    string testPath = "non_existent_file_to_delete.txt";
    
    // Ensure file doesn't exist
    ASSERT_FALSE(filesystem::exists(testPath));
    
    // Attempt to remove should throw
    EXPECT_THROW({
        storage.remove(testPath);
    }, out_of_range);
}


