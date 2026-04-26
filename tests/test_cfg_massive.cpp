#include <gtest/gtest.h>
#include "engines/cfg_engine.hpp"
#include "core/analysis_context.hpp"
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

class CFGExtremeTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / "scout_cfg_extreme";
        fs::create_directories(temp_dir / "smali");
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_file(const std::string& name, const std::string& content) {
        std::ofstream ofs(temp_dir / "smali" / name);
        ofs << content;
    }

    fs::path temp_dir;
};

TEST_F(CFGExtremeTest, RecursiveJumps) {
    std::string smali = R"(.class public LRecursive;
.method public loop()V
    .registers 1
    :start
    goto :start
    return-void
.end method
)";
    write_file("Recursive.smali", smali);

    engines::CFGEngine engine;
    engines::SearchConfig config;
    config.query = "LRecursive;->loop()V";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
    EXPECT_TRUE(results[0].context.find("BB1") != std::string::npos);
}

TEST_F(CFGExtremeTest, MissingHeaders) {
    // Missing .end method
    std::string smali = R"(.class public LMissing;
.method public broken()V
    .registers 1
    return-void
)";
    write_file("Missing.smali", smali);

    engines::CFGEngine engine;
    engines::SearchConfig config;
    config.query = "LMissing;->broken()V";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    // Should not crash, might return empty results if it can't find .end method
    EXPECT_TRUE(results.empty());
}

TEST_F(CFGExtremeTest, MassiveCFG) {
    std::stringstream ss;
    ss << ".class public LMassive;\n";
    ss << ".method public big()V\n";
    ss << "    .registers 2\n";
    for (int i = 0; i < 5000; ++i) {
        ss << "    if-eqz v0, :label_" << i << "\n";
        ss << "    const v0, " << i << "\n";
        ss << "    :label_" << i << "\n";
    }
    ss << "    return-void\n.end method\n";
    write_file("Massive.smali", ss.str());

    engines::CFGEngine engine;
    engines::SearchConfig config;
    config.query = "LMassive;->big()V";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
    EXPECT_TRUE(results[0].context.find("BB5000") != std::string::npos);
}

TEST_F(CFGExtremeTest, DeepRecursionAndCrossJumps) {
    std::stringstream ss;
    ss << ".class public LDeep;\n";
    ss << ".method public deep()V\n";
    ss << "    .registers 2\n";
    for (int i = 0; i < 100; ++i) {
        ss << "    :l_" << i << "\n";
        ss << "    if-eqz v0, :l_" << (i + 1) % 100 << "\n";
        ss << "    goto :l_" << (i + 2) % 100 << "\n";
    }
    ss << "    return-void\n.end method\n";
    write_file("Deep.smali", ss.str());

    engines::CFGEngine engine;
    engines::SearchConfig config;
    config.query = "LDeep;->deep()V";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
}
