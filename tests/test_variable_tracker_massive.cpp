#include <gtest/gtest.h>
#include "engines/variable_tracker_engine.hpp"
#include "core/analysis_context.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class VarTrackerExtremeTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / "scout_var_extreme";
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

TEST_F(VarTrackerExtremeTest, EmptyRegisters) {
    std::string smali = R"(.class public LEmptyReg;
.method public test()V
    .registers 5
    # v0 is never initialized
    move-object v1, v0
    return-void
.end method
)";
    write_file("EmptyReg.smali", smali);

    engines::VariableTrackerEngine engine;
    engines::SearchConfig config;
    config.query = "LEmptyReg;->test()V";
    config.var_name = "v1";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
}

TEST_F(VarTrackerExtremeTest, OverwrittenRegisters) {
    std::string smali = R"(.class public LOverwritten;
.method public test(Ljava/lang/String;)V
    .registers 5
    # p1 is input
    const-string v0, "safe"
    move-object p1, v0
    invoke-static {p1}, Landroid/util/Log;->i(Ljava/lang/String;)I
    return-void
.end method
)";
    write_file("Overwritten.smali", smali);

    engines::VariableTrackerEngine engine;
    engines::SearchConfig config;
    config.query = "LOverwritten;->test(Ljava/lang/String;)V";
    config.var_name = "p1";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
    // v0 (safe) overwrote p1. It should NOT be considered tainted if we were tracking p1 as source.
}

TEST_F(VarTrackerExtremeTest, NativeMethodMock) {
    std::string smali = R"(.class public LNative;
.method public native nativeMethod(Ljava/lang/String;)V
.end method

.method public test(Ljava/lang/String;)V
    .registers 3
    invoke-virtual {p0, p1}, LNative;->nativeMethod(Ljava/lang/String;)V
    return-void
.end method
)";
    write_file("Native.smali", smali);

    engines::VariableTrackerEngine engine;
    engines::SearchConfig config;
    config.query = "LNative;->test(Ljava/lang/String;)V";
    config.var_name = "p1";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
}

TEST_F(VarTrackerExtremeTest, CorruptedOperands) {
    std::string smali = R"(.class public LCorrupted;
.method public test()V
    .registers 2
    # move-object with same reg
    move-object v0, v0
    const-string v0, "secret"
    move-object v1, v0
    # junk after comma
    const-string v1, "fake", "junk"
    return-void
.end method
)";
    write_file("Corrupted.smali", smali);

    engines::VariableTrackerEngine engine;
    engines::SearchConfig config;
    config.query = "LCorrupted;->test()V";
    config.var_name = "v1";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
}
