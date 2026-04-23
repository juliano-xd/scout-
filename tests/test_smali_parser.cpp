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
