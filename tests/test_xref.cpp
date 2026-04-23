#include <gtest/gtest.h>
#include "analysis/xref.hpp"
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
    auto results = analysis::XrefEngine::find_callers(test_dir, "Lcom/example/Target;->importantMethod()Z");
    
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].caller_class, "Lcom/example/Caller;");
    EXPECT_EQ(results[0].caller_method, "public doSomething()V");
    EXPECT_EQ(results[0].line_number, 5);
    EXPECT_EQ(results[0].instruction, "invoke-static {}, Lcom/example/Target;->importantMethod()Z");
}

TEST_F(XrefTest, FindsClassCallersCorrectly) {
    auto results = analysis::XrefEngine::find_callers(test_dir, "Lcom/example/Target;");
    
    // We expect 2 references to the Target class in Caller.smali
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].caller_method, "public doSomething()V");
    EXPECT_EQ(results[1].caller_method, "public doSomething()V");
    
    EXPECT_EQ(results[0].instruction, "invoke-static {}, Lcom/example/Target;->importantMethod()Z");
    EXPECT_EQ(results[1].instruction, "sput-object v0, Lcom/example/Target;->someField:I");
}
