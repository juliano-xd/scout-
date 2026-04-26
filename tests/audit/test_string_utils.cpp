#include <gtest/gtest.h>
#include "utils/string_utils.hpp"
#include <filesystem>
#include <fstream>

using namespace utils;

TEST(StringUtilsAudit, Trim) {
    EXPECT_EQ(trim("  hello  "), "hello");
    EXPECT_EQ(trim("\t\r\nworld\n"), "world");
    EXPECT_EQ(trim("   "), "");
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("inner spaces preserved"), "inner spaces preserved");
    EXPECT_EQ(trim(" \t\r\n "), "");
    EXPECT_EQ(trim("a"), "a");
}

TEST(StringUtilsAudit, ToHex) {
    EXPECT_EQ(to_hex(0), "0x00000000");
    EXPECT_EQ(to_hex(255), "0x000000ff");
    EXPECT_EQ(to_hex(0xCAFEBABE), "0xcafebabe");
    EXPECT_EQ(to_hex(0xFFFFFFFF), "0xffffffff");
}

TEST(StringUtilsAudit, IsBase64) {
    EXPECT_TRUE(is_base64("SGVsbG8gV29ybGQ=")); 
    EXPECT_TRUE(is_base64("SGVsbG8="));        
    EXPECT_FALSE(is_base64("Invalid!Char#"));   
    EXPECT_FALSE(is_base64("too_short")); // Implementation requires length >= 12
    EXPECT_TRUE(is_base64("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"));
}

TEST(StringUtilsAudit, DecodeBase64) {
    EXPECT_EQ(decode_base64("SGVsbG8="), "Hello");
    EXPECT_EQ(decode_base64("SGVsbG8gV29ybGQ="), "Hello World");
    EXPECT_EQ(decode_base64("SGVsbG8h"), "Hello!");
    EXPECT_EQ(decode_base64("SGVsbG8#"), "Hello"); 
    EXPECT_EQ(decode_base64(""), "");
    EXPECT_EQ(decode_base64("AAA="), std::string("\0\0", 2)); // Handles null bytes if needed, though std::string might truncate in some views
}

TEST(StringUtilsAudit, Entropy) {
    EXPECT_DOUBLE_EQ(calculate_entropy(""), 0.0);
    EXPECT_DOUBLE_EQ(calculate_entropy("aaaaa"), 0.0);
    EXPECT_NEAR(calculate_entropy("abcd"), 2.0, 0.001);
    // Max entropy for 256 chars all different
    std::string all_chars;
    for(int i=0; i<256; ++i) all_chars += (char)i;
    EXPECT_NEAR(calculate_entropy(all_chars), 8.0, 0.001);
}

TEST(StringUtilsAudit, ContainsInsensitive) {
    EXPECT_TRUE(contains_insensitive("Hello World", "world"));
    EXPECT_TRUE(contains_insensitive("SCout++", "SCOUT"));
    EXPECT_FALSE(contains_insensitive("Hello", "earth"));
    EXPECT_TRUE(contains_insensitive("any", ""));
    EXPECT_FALSE(contains_insensitive("", "not_empty"));
    EXPECT_TRUE(contains_insensitive("", ""));
}

TEST(StringUtilsAudit, LineIterator) {
    std::string_view data = "line1\nline2\r\nline3\rline4";
    LineIterator it(data);
    std::string_view line;
    
    ASSERT_TRUE(it.next(line));
    EXPECT_EQ(line, "line1");
    
    ASSERT_TRUE(it.next(line));
    EXPECT_EQ(line, "line2");
    
    // Note: current implementation of LineIterator only splits on \n
    ASSERT_TRUE(it.next(line));
    EXPECT_EQ(line, "line3\rline4");
    
    EXPECT_FALSE(it.next(line));

    // Test with only \r\n
    LineIterator it2("a\r\nb\r\n");
    ASSERT_TRUE(it2.next(line)); EXPECT_EQ(line, "a");
    ASSERT_TRUE(it2.next(line)); EXPECT_EQ(line, "b");
    EXPECT_FALSE(it2.next(line));

    // Test empty
    LineIterator it3("");
    EXPECT_FALSE(it3.next(line));
}

TEST(StringUtilsAudit, ExtractSmali) {
    auto path = std::filesystem::absolute("audit_test_string.smali");
    {
        std::ofstream out(path);
        out << ".class LTest;\n"
            << ".method public test()V\n"
            << "    const/4 v0, 1\n"
            << "    return-void\n"
            << ".end method\n";
    }
    
    auto body = extract_smali_method(path, "test()V");
    ASSERT_EQ(body.size(), 2);
    EXPECT_TRUE(body[0].find("const/4 v0, 1") != std::string::npos);
    
    std::filesystem::remove(path);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
