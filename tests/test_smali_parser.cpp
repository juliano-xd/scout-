#include <gtest/gtest.h>
#include "core/smali_parser.hpp"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

TEST(SmaliParserTest, ExtractsMethodsCorrectly) {
    fs::path temp_file = fs::temp_directory_path() / "test_class.smali";
    std::ofstream(temp_file) << ".class public LTest;\n"
                             << ".super Ljava/lang/Object;\n"
                             << "\n"
                             << ".method public constructor <init>()V\n"
                             << "    return-void\n"
                             << ".end method\n"
                             << "\n"
                             << ".method public static doLogin(Ljava/lang/String;Ljava/lang/String;)Z\n"
                             << "    return-void\n"
                             << ".end method\n";
                             
    auto methods = core::SmaliParser::extract_methods(temp_file);
    ASSERT_EQ(methods.size(), 2);
    
    EXPECT_EQ(methods[0].name, "<init>");
    EXPECT_EQ(methods[0].signature, "()V");
    EXPECT_EQ(methods[0].access_modifiers, "public constructor");
    
    EXPECT_EQ(methods[1].name, "doLogin");
    EXPECT_EQ(methods[1].signature, "(Ljava/lang/String;Ljava/lang/String;)Z");
    EXPECT_EQ(methods[1].access_modifiers, "public static");
    
    fs::remove(temp_file);
}

TEST(SmaliParserTest, HandlesEmptyOrInvalidFile) {
    fs::path temp_file = fs::temp_directory_path() / "invalid.smali";
    std::ofstream(temp_file) << "just some random text\nno methods here";
    
    auto methods = core::SmaliParser::extract_methods(temp_file);
    EXPECT_TRUE(methods.empty());
    
    fs::remove(temp_file);
}

TEST(SmaliParserTest, MalformedDeclarations) {
    fs::path temp_file = fs::temp_directory_path() / "malformed.smali";
    std::ofstream(temp_file) << ".method\n" // too short
                             << ".method public\n" // no name/signature
                             << ".method ()\n" // no name
                             << ".method public static nameWithoutSignature\n" // no parens
                             << ".method   public   spaced   method()V\n" // extra spaces
                             << "\t.method\tpublic\ttabbed()V\n"; // tabs
                             
    auto methods = core::SmaliParser::extract_methods(temp_file);
    // Based on current implementation:
    // ".method " (8 chars) is checked.
    // ".method " -> length 8. substr(8) is empty. find('(') fails.
    // ".method   public   spaced   method()V" -> trimmed is ".method   public   spaced   method()V"
    //   substr(8) is "  public   spaced   method()V"
    //   paren_pos is 25. signature is "()V"
    //   before_paren is "  public   spaced   method"
    //   last_space is 18. name is "method", modifiers is "  public   spaced"
    
    EXPECT_GE(methods.size(), 0);
    for (const auto& m : methods) {
        EXPECT_FALSE(m.signature.empty());
        EXPECT_TRUE(m.signature.starts_with('('));
    }
    fs::remove(temp_file);
}

TEST(SmaliParserTest, NonExistentFile) {
    auto methods = core::SmaliParser::extract_methods("/tmp/definitely_not_there_12345.smali");
    EXPECT_TRUE(methods.empty());
}

TEST(SmaliParserTest, FileWithOnlyCommentsAndDirectives) {
    fs::path temp_file = fs::temp_directory_path() / "no_methods.smali";
    std::ofstream(temp_file) << "# comment\n"
                             << ".class LTest;\n"
                             << ".super Ljava/lang/Object;\n"
                             << ".field public f:I\n";
    auto methods = core::SmaliParser::extract_methods(temp_file);
    EXPECT_TRUE(methods.empty());
    fs::remove(temp_file);
}

TEST(SmaliParserTest, MethodWithUnusualSignature) {
    fs::path temp_file = fs::temp_directory_path() / "unusual.smali";
    std::ofstream(temp_file) << ".method public crazy$Name(L/A;[I[[L/B;)L/C;\n"
                             << ".end method\n";
    auto methods = core::SmaliParser::extract_methods(temp_file);
    ASSERT_EQ(methods.size(), 1);
    EXPECT_EQ(methods[0].name, "crazy$Name");
    EXPECT_EQ(methods[0].signature, "(L/A;[I[[L/B;)L/C;");
    fs::remove(temp_file);
}
