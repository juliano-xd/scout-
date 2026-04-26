#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "engines/content_search_engine.hpp"
#include "core/analysis_context.hpp"

namespace fs = std::filesystem;

TEST(ConstantSearchTest, ShouldFindStringConstantWithContext) {
    fs::path test_dir = "tmp_test_const";
    fs::create_directories(test_dir);
    
    std::ofstream f1(test_dir / "ConstTest.smali");
    f1 << ".class Lcom/test/ConstTest;\n"
       << ".method public static secret()V\n"
       << "    const-string v0, \"SUPER_SECRET_TOKEN_123\"\n"
       << "    return-void\n"
       << ".end method\n";
    f1.close();

    core::AnalysisContext ctx(test_dir);
    engines::ContentSearchEngine engine;
    engines::SearchConfig config;
    config.query = "SUPER_SECRET_TOKEN_123";

    auto results = engine.search(ctx, config);
    
    fs::remove_all(test_dir);

    ASSERT_GE(results.size(), 1);
    // Deve conter o nome do método no contexto
    EXPECT_TRUE(results[0].context.find("secret") != std::string::npos);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
