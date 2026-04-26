#include <gtest/gtest.h>
#include "engines/smali_dump_engine.hpp"
#include "core/analysis_context.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace engines;

namespace {
    fs::path create_test_smali() {
        fs::path temp_dir = fs::temp_directory_path() / "scout_test_dump_engine";
        fs::create_directories(temp_dir / "smali");
        
        std::ofstream file(temp_dir / "smali" / "Test.smali");
        file << ".class LTest;\n";
        file << ".super Ljava/lang/Object;\n";
        file << ".source \"Test.java\"\n";
        file << ".field private secret:Ljava/lang/String;\n";
        file << ".method public run()V\n";
        file << "    .registers 2\n";
        file << "    const-string v0, \"hello\"\n";
        file << "    return-void\n";
        file << ".end method\n";
        file.close();
        
        return temp_dir;
    }

    void cleanup(const fs::path& p) {
        if (fs::exists(p)) fs::remove_all(p);
    }
}

TEST(SmaliDumpEngine, BasicDump) {
    fs::path temp_dir = create_test_smali();
    core::AnalysisContext ctx(temp_dir);
    SmaliDumpEngine engine;
    
    SearchConfig config;
    config.query = "LTest;";
    
    auto results = engine.search(ctx, config);
    ASSERT_EQ(results.size(), 1);
    
    std::string output = results[0].line_content;
    EXPECT_NE(output.find(":smali-ast"), std::string::npos);
    EXPECT_NE(output.find(":class"), std::string::npos);
    EXPECT_NE(output.find("\"LTest;\""), std::string::npos);
    EXPECT_NE(output.find(":methods"), std::string::npos);
    EXPECT_NE(output.find(":fields"), std::string::npos);
    EXPECT_NE(output.find("secret:Ljava/lang/String;"), std::string::npos);
    EXPECT_NE(output.find("const-string v0, \\\"hello\\\""), std::string::npos);
    
    cleanup(temp_dir);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
