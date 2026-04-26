#include <gtest/gtest.h>
#include "engines/class_search_engine.hpp"
#include <fstream>

using namespace engines;

// 1. Dalvik Detection
TEST(ClassSearchAudit, DalvikDetection) {
    EXPECT_TRUE(ClassSearchEngine::is_dalvik_notation("Lcom/app/Main;"));
    EXPECT_FALSE(ClassSearchEngine::is_dalvik_notation("com/app/Main"));
    EXPECT_FALSE(ClassSearchEngine::is_dalvik_notation("Main"));
}

// 2. Normalization
TEST(ClassSearchAudit, Normalization) {
    EXPECT_EQ(ClassSearchEngine::normalize_dalvik("Lcom/app/Main;"), "com/app/Main");
    EXPECT_EQ(ClassSearchEngine::normalize_dalvik("Lcom/app/Main"), "com/app/Main");
    EXPECT_EQ(ClassSearchEngine::normalize_dalvik("com/app/Main;"), "com/app/Main");
}

// 3. Search Workflow
TEST(ClassSearchAudit, SearchWorkflow) {
    auto root = std::filesystem::absolute("class_audit_root_super");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "smali/com/app");
    
    {
        std::ofstream(root / "smali/com/app/Main.smali");
        std::ofstream(root / "smali/com/app/Other.smali");
    }
    
    core::AnalysisContext ctx(root);
    ClassSearchEngine engine;
    
    // Exact Match
    {
        SearchConfig config;
        config.query = "Lcom/app/Main;";
        auto results = engine.search(ctx, config);
        ASSERT_EQ(results.size(), 1);
        EXPECT_TRUE(results[0].file_path.string().find("Main.smali") != std::string::npos);
    }
    
    // Substring Match
    {
        SearchConfig config;
        config.query = "com/app";
        auto results = engine.search(ctx, config);
        EXPECT_GE(results.size(), 2);
    }
    
    std::filesystem::remove_all(root);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
