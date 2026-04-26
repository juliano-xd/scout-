#include <gtest/gtest.h>
#include "engines/xref_search_engine.hpp"
#include "core/analysis_context.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class XrefExtremeTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / "scout_xref_extreme";
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

TEST_F(XrefExtremeTest, DeepCallGraph) {
    for (int i = 0; i < 20; ++i) {
        std::stringstream ss;
        ss << ".class public LClass" << i << ";\n";
        ss << ".method public call" << i << "()V\n";
        ss << "    .registers 1\n";
        if (i < 19) {
            ss << "    invoke-static {}, LClass" << (i + 1) << ";->call" << (i + 1) << "()V\n";
        }
        ss << "    return-void\n.end method\n";
        write_file("Class" + std::to_string(i) + ".smali", ss.str());
    }

    engines::XrefSearchEngine engine;
    engines::SearchConfig config;
    config.query = "LClass19;->call19()V";
    config.search_depth = 10;
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
}

TEST_F(XrefExtremeTest, CircularReferences) {
    write_file("A.smali", ".class public LA;\n.method public a()V\n    invoke-static {}, LB;->b()V\n    return-void\n.end method\n");
    write_file("B.smali", ".class public LB;\n.method public b()V\n    invoke-static {}, LA;->a()V\n    return-void\n.end method\n");

    engines::XrefSearchEngine engine;
    engines::SearchConfig config;
    config.query = "LA;->a()V";
    config.search_depth = 5;
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
}

TEST_F(XrefExtremeTest, MissingTarget) {
    write_file("Test.smali", ".class public LTest;\n.method public test()V\n    invoke-static {}, LNonExistent;->target()V\n    return-void\n.end method\n");

    engines::XrefSearchEngine engine;
    engines::SearchConfig config;
    config.query = "LNonExistent;->target()V";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].file_path.string(), "Test.smali");
}
