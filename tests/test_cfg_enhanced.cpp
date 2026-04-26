#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "engines/cfg_engine.hpp"
#include "core/analysis_context.hpp"
#include "engines/i_search_engine.hpp"

namespace fs = std::filesystem;

TEST(CFGEnhancedTest, ShouldIncludeInstructionsInSExpr) {
    // Setup: Criar diretório e arquivo de teste
    fs::path test_dir = "tmp_test_cfg";
    fs::create_directories(test_dir);
    
    std::ofstream smali_file(test_dir / "Test.smali");
    smali_file << ".class Ltest/Test;\n"
               << ".method public static main()V\n"
               << "    const-string v0, \"Hello\"\n"
               << "    if-eqz v0, :label\n"
               << "    invoke-static {v0}, Ltest/Log;->i(Ljava/lang/String;)V\n"
               << "    :label\n"
               << "    return-void\n"
               << ".end method\n";
    smali_file.close();

    core::AnalysisContext ctx(test_dir);
    engines::CFGEngine engine;
    engines::SearchConfig config;
    config.query = "Ltest/Test;->main()V";

    auto results = engine.search(ctx, config);
    
    // Cleanup
    fs::remove_all(test_dir);

    ASSERT_EQ(results.size(), 1);

    // O teste deve falhar agora porque ":ops" ainda não existe na implementação
    EXPECT_TRUE(results[0].line_content.find(":ops") != std::string::npos);
    EXPECT_TRUE(results[0].line_content.find("if-eqz") != std::string::npos);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
