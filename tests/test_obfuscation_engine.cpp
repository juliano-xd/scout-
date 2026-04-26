#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "engines/deobf_engine.hpp"
#include "core/analysis_context.hpp"

namespace fs = std::filesystem;

TEST(ObfuscationTest, ShouldDetectObfuscatedClass) {
    fs::path test_dir = "tmp_test_obf";
    fs::create_directories(test_dir);
    
    // Classe Ofuscada
    std::ofstream f1(test_dir / "a.smali");
    f1 << ".class Lcom/a/a/a;\n"
       << ".field public a:I\n"
       << ".method public a(I)V\n"
       << "    return-void\n"
       << ".end method\n";
    f1.close();

    core::AnalysisContext ctx(test_dir);
    engines::DeobfEngine engine;
    engines::SearchConfig config;
    config.query = "Lcom/a/a/a;";

    auto results = engine.search(ctx, config);
    
    fs::remove_all(test_dir);

    ASSERT_EQ(results.size(), 1);
    // Deve conter (obfuscated true) ou similar
    EXPECT_TRUE(results[0].line_content.find(":obfuscated true") != std::string::npos);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
