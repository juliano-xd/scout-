#include <gtest/gtest.h>
#include "engines/cfg_engine.hpp"
#include "core/analysis_context.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class CFGMassiveTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / ("scout_cfg_massive_" + std::to_string(GetParam()));
        fs::create_directories(temp_dir / "smali");
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_smali(int i) {
        std::ofstream ofs(temp_dir / "smali/Test.smali");
        ofs << ".class public LTest;\n";
        ofs << ".method public complex" << i << "()V\n";
        ofs << "    .registers 2\n";
        
        for(int j=0; j < (i % 10) + 1; ++j) {
            ofs << "    if-eqz v0, :cond_" << j << "\n";
            ofs << "    const v0, " << j << "\n";
            ofs << "    :cond_" << j << "\n";
        }
        
        ofs << "    return-void\n.end method\n";
    }

    fs::path temp_dir;
};

TEST_P(CFGMassiveTest, FlowGraphComplexityTest) {
    int i = GetParam();
    write_smali(i);

    engines::CFGEngine engine;
    engines::SearchConfig config;
    config.query = "LTest;->complex" + std::to_string(i) + "()V";
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();

    ASSERT_FALSE(results.empty()) << "Falha ao gerar CFG para variante " << i;
    
    // Validar se o formato S-Expression contém os blocos
    std::string content = results[0].context;
    
    bool has_cfg = content.find("cfg-report") != std::string::npos;
    bool has_blocks = content.find("blocks") != std::string::npos;

    EXPECT_TRUE(has_cfg) << "S-Expression nao contem tag 'cfg-report'. Conteudo: " << content;
    EXPECT_TRUE(has_blocks) << "S-Expression nao contem 'blocks'. Conteudo: " << content;
}

INSTANTIATE_TEST_SUITE_P(MassiveSuite, CFGMassiveTest, ::testing::Range(0, 100));
