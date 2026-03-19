#include <gtest/gtest.h>
#include "Services/BufferOutput.h"

TEST(BufferOutputTest, Write_AppendsToBuffer) {
    BufferOutput buffer;
    buffer.write("Hello");
    buffer.write(" World");
    EXPECT_EQ(buffer.getBuffer(), "Hello World");
}

TEST(BufferOutputTest, GetBuffer_ReturnsAccumulatedContent) {
    BufferOutput buffer;
    buffer.write("Test");
    EXPECT_EQ(buffer.getBuffer(), "Test");
}

TEST(BufferOutputTest, Clear_EmptiesBuffer) {
    BufferOutput buffer;
    buffer.write("Data");
    buffer.clear();
    EXPECT_EQ(buffer.getBuffer(), "");
}

TEST(BufferOutputTest, EmptyBuffer_ReturnsEmptyString) {
    BufferOutput buffer;
    EXPECT_EQ(buffer.getBuffer(), "");
}
