#include <gtest/gtest.h>
#include "engines/xref_search_engine.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class XrefTest : public ::testing::Test {
protected:
    fs::path test_dir;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "scout_xref_test";
        fs::create_directories(test_dir / "smali" / "com" / "example");
        
        std::ofstream(test_dir / "smali" / "com" / "example" / "Caller.smali") 
            << ".class public Lcom/example/Caller;\n"
            << ".super Ljava/lang/Object;\n"
            << "\n"
            << ".method public doSomething()V\n"
            << "    invoke-static {}, Lcom/example/Target;->importantMethod()Z\n"
            << "    move-result v0\n"
            << "    sput-object v0, Lcom/example/Target;->someField:I\n"
            << "    return-void\n"
            << ".end method\n";
    }

    void TearDown() override {
        fs::remove_all(test_dir);
    }
};

TEST_F(XrefTest, FindsMethodCallersCorrectly) {
    engines::XrefSearchEngine engine;
    engines::SearchConfig config;
    config.query = "Lcom/example/Target;->importantMethod()Z";
    config.search_type = "string";
    
    auto results = engine.search(test_dir, config);
    
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].line_number, 5);
    EXPECT_EQ(results[0].line_content, "invoke-static {}, Lcom/example/Target;->importantMethod()Z");
}

TEST_F(XrefTest, FindsClassCallersCorrectly) {
    engines::XrefSearchEngine engine;
    engines::SearchConfig config;
    config.query = "Lcom/example/Target;";
    config.search_type = "string";
    
    auto results = engine.search(test_dir, config);
    
    // We expect 2 references to the Target class in Caller.smali
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].line_content, "invoke-static {}, Lcom/example/Target;->importantMethod()Z");
    EXPECT_EQ(results[1].line_content, "sput-object v0, Lcom/example/Target;->someField:I");
}
