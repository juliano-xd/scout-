#include <gtest/gtest.h>
#include "engines/content_search_engine.hpp"
#include <regex>
#include <filesystem>
#include <fstream>

using namespace engines;

// 1. Context Tracking
TEST(ContentSearchAudit, UpdateContext) {
    ContentSearchEngine::ParseContext ctx;
    
    ContentSearchEngine::update_context(".class public Lcom/app/Main;", ctx);
    EXPECT_EQ(ctx.current_class, "Lcom/app/Main;");
    
    // O motor extrai tudo após ".method "
    ContentSearchEngine::update_context(".method public onCreate()V", ctx);
    EXPECT_EQ(ctx.current_method, "public onCreate()V");
    
    ContentSearchEngine::update_context(".end method", ctx);
    EXPECT_EQ(ctx.current_method, "");
}

// 2. String Match
TEST(ContentSearchAudit, MatchesString) {
    EXPECT_TRUE(ContentSearchEngine::matches_string("const-string v0, \"secret\"", "secret", true));
    EXPECT_TRUE(ContentSearchEngine::matches_string("const-string v0, \"Secret\"", "secret", false));
    EXPECT_FALSE(ContentSearchEngine::matches_string("const-string v0, \"Secret\"", "secret", true));
}

// 3. Regex Match
TEST(ContentSearchAudit, MatchesRegex) {
    std::regex pattern("const-string v\\d, \".*\"");
    EXPECT_TRUE(ContentSearchEngine::matches_regex("const-string v0, \"hello\"", pattern));
    EXPECT_FALSE(ContentSearchEngine::matches_regex("const-int v0, 123", pattern));
}

// 4. Integer/Hex Match (Crucial Boundaries)
TEST(ContentSearchAudit, MatchesInteger) {
    EXPECT_TRUE(ContentSearchEngine::matches_integer("const v0, 0x123", "0x123"));
    EXPECT_TRUE(ContentSearchEngine::matches_integer("sput v0, LApp;->ID:I = 0x123", "0x123"));
    
    // Boundary check: 0x123 não deve bater em 0x1234
    EXPECT_FALSE(ContentSearchEngine::matches_integer("const v0, 0x1234", "0x123"));
    
    // Decimal
    EXPECT_TRUE(ContentSearchEngine::matches_integer("const v0, 100", "100"));
    EXPECT_FALSE(ContentSearchEngine::matches_integer("const v0, 1000", "100"));
}

// 5. Search Workflow
TEST(ContentSearchAudit, SearchWorkflow) {
    auto root = std::filesystem::absolute("content_audit_root_super");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "smali");
    
    {
        std::ofstream out(root / "smali/Test.smali");
        out << ".class LTest;\n"
            << ".method public run()V\n"
            << "    const-string v0, \"found_me\"\n"
            << "    const v1, 0x7f01\n"
            << ".end method\n";
    }
    
    core::AnalysisContext ctx(root);
    ContentSearchEngine engine;
    
    // Mode String
    {
        SearchConfig config;
        config.query = "found_me";
        config.search_type = "string";
        auto results = engine.search(ctx, config);
        ASSERT_EQ(results.size(), 1);
        // O formato de contexto do Scout++ usa "method:" prefixado
        EXPECT_EQ(results[0].context, "method:public run()V");
    }
    
    std::filesystem::remove_all(root);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
