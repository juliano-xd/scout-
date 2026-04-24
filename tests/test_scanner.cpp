#include <gtest/gtest.h>
#include "core/scanner.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class ScannerTest : public ::testing::Test {
protected:
    fs::path test_dir;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "scout_scanner_test";
        fs::create_directories(test_dir / "smali" / "com" / "example");
        fs::create_directories(test_dir / "smali_classes2" / "org" / "test");
        
        std::ofstream(test_dir / "smali" / "com" / "example" / "Auth.smali") << ".class public Lcom/example/Auth;";
        std::ofstream(test_dir / "smali_classes2" / "org" / "test" / "Util.smali") << ".class public Lorg/test/Util;";
        std::ofstream(test_dir / "smali" / "RootClass.smali") << ".class public LRootClass;";
    }

    void TearDown() override {
        fs::remove_all(test_dir);
    }
};

TEST_F(ScannerTest, ParsesClassInfoCorrectly) {
    auto info1 = core::ClassInfo::parse("Lcom/example/Auth;");
    EXPECT_EQ(info1.package_path, "com/example");
    EXPECT_EQ(info1.class_name, "Auth");
    EXPECT_EQ(info1.file_name, "Auth.smali");

    auto info2 = core::ClassInfo::parse("org.test.Util");
    EXPECT_EQ(info2.package_path, "org/test");
    EXPECT_EQ(info2.class_name, "Util");
    EXPECT_EQ(info2.file_name, "Util.smali");

    auto info3 = core::ClassInfo::parse("RootClass");
    EXPECT_EQ(info3.package_path, "");
    EXPECT_EQ(info3.class_name, "RootClass");
    EXPECT_EQ(info3.file_name, "RootClass.smali");
}

TEST_F(ScannerTest, FindsClassWithDalvikName) {
    auto res = core::find_class_file(test_dir, "Lcom/example/Auth;");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->filename(), "Auth.smali");
}

TEST_F(ScannerTest, FindsClassWithDotName) {
    auto res = core::find_class_file(test_dir, "org.test.Util");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->filename(), "Util.smali");
}

TEST_F(ScannerTest, FindsClassWithSimpleName) {
    auto res = core::find_class_file(test_dir, "RootClass");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->filename(), "RootClass.smali");
}

TEST_F(ScannerTest, ReturnsNulloptForNonExistent) {
    auto res = core::find_class_file(test_dir, "Lcom/example/NotFound;");
    EXPECT_FALSE(res.has_value());
}

TEST_F(ScannerTest, FindsClassesContainingName) {
    // Already have Auth.smali and Util.smali
    std::ofstream(test_dir / "smali" / "com" / "example" / "AuthManager.smali") << ".class public Lcom/example/AuthManager;";
    
    auto res = core::find_classes_containing(test_dir, "Auth");
    EXPECT_EQ(res.size(), 2);
    
    // Check if both files are present
    bool found_auth = false;
    bool found_auth_manager = false;
    for (const auto& path : res) {
        if (path.filename() == "Auth.smali") found_auth = true;
        if (path.filename() == "AuthManager.smali") found_auth_manager = true;
    }
    EXPECT_TRUE(found_auth);
    EXPECT_TRUE(found_auth_manager);
}

TEST_F(ScannerTest, FindsNoClassesContainingNonExistentName) {
    auto res = core::find_classes_containing(test_dir, "Xyzzy");
    EXPECT_TRUE(res.empty());
}

#include "engines/content_search_engine.hpp"

TEST_F(ScannerTest, SearchesContentInsideFiles) {
    std::ofstream(test_dir / "smali" / "com" / "example" / "Data.smali") 
        << ".class public Lcom/example/Data;\n"
        << ".field public static final MAGIC_NUMBER:I = 0x42\n"
        << ".method public getMagic()I\n"
        << "    const/16 v0, 0x42\n"
        << "    const-string v1, \"test_string_123\"\n"
        << "    return v0\n"
        << ".end method\n";

    engines::ContentSearchEngine engine;
    engines::SearchConfig config;
    config.query = "0x42";
    config.search_type = "string";

    auto results_num = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
    EXPECT_EQ(results_num.size(), 2);

    config.query = "string_123";
    auto results_str = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
    EXPECT_EQ(results_str.size(), 1);

    config.query = "MAGIC_\\w+";
    config.search_type = "regex";
    auto results_rgx = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
    EXPECT_EQ(results_rgx.size(), 1);
}

