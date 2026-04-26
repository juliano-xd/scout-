#include <gtest/gtest.h>
#include "engines/deobf_engine.hpp"
#include "core/analysis_context.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class DeobfExtremeTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / "scout_deobf_extreme";
        fs::create_directories(temp_dir / "smali");
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_file(const std::string& name, const std::string& content) {
        std::ofstream ofs(temp_dir / "smali" / name, std::ios::binary);
        ofs << content;
    }

    fs::path temp_dir;
};

TEST_F(DeobfExtremeTest, MixedHexAndBase64) {
    std::string content = ".class public LMixed;\n"
                          ".method public test()V\n"
                          "    const-string v0, \"SGVsbG8gV29ybGQhCg==\"\n" // Hello World!
                          "    const-string v1, \"\\x41\\x42\\x43\"\n"
                          "    return-void\n"
                          ".end method\n";
    write_file("Mixed.smali", content);

    engines::DeobfEngine engine;
    engines::SearchConfig config;
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
}

TEST_F(DeobfExtremeTest, NullBytesInString) {
    std::string content = ".class public LNullBytes;\n"
                          ".method public test()V\n"
                          "    const-string v0, \"Base64WithNullBytes" + std::string(1, '\0') + "SGVsbG8=\"\n"
                          "    return-void\n"
                          ".end method\n";
    write_file("NullBytes.smali", content);

    engines::DeobfEngine engine;
    engines::SearchConfig config;
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    // Should not crash
    (void)results;
}

TEST_F(DeobfExtremeTest, BrokenSequences) {
    std::string content = ".class public LBroken;\n"
                          ".method public test()V\n"
                          "    const-string v0, \"SGVsbG8\" # Missing padding\n"
                          "    const-string v1, \"SGVsbG8gV29ybGQhCg====\" # Too much padding\n"
                          "    const-string v2, \"!!!NOTBASE64!!!\"\n"
                          "    return-void\n"
                          ".end method\n";
    write_file("Broken.smali", content);

    engines::DeobfEngine engine;
    engines::SearchConfig config;
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    (void)results;
}
