#include <gtest/gtest.h>
#include "engines/class_inspector_engine.hpp"
#include "core/analysis_context.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class ClassInspectorExtremeTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / "scout_inspector_extreme";
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

TEST_F(ClassInspectorExtremeTest, HugeClass) {
    std::stringstream ss;
    ss << ".class public LHuge;\n";
    ss << ".super Ljava/lang/Object;\n";
    for (int i = 0; i < 5000; ++i) {
        ss << ".field public f" << i << ":I\n";
        ss << ".method public m" << i << "()V\n    return-void\n.end method\n";
    }
    write_file("Huge.smali", ss.str());

    engines::ClassInspectorEngine engine;
    engines::SearchConfig config;
    config.query = "LHuge;";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
    EXPECT_TRUE(results[0].context.find("m4999") != std::string::npos);
}

TEST_F(ClassInspectorExtremeTest, CorruptedSyntax) {
    std::string smali = R"(.class public LCorrupted;
.super 
.implements
.field
.method
)";
    write_file("Corrupted.smali", smali);

    engines::ClassInspectorEngine engine;
    engines::SearchConfig config;
    config.query = "LCorrupted;";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    // Should not crash
    ASSERT_FALSE(results.empty());
}

TEST_F(ClassInspectorExtremeTest, MaliciousMethodNames) {
    std::string smali = R"(.class public LMalicious;
.method public "()V()V"()V
    return-void
.end method
)";
    write_file("Malicious.smali", smali);

    engines::ClassInspectorEngine engine;
    engines::SearchConfig config;
    config.query = "LMalicious;";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
}
