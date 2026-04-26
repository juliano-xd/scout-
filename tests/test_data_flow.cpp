#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "engines/variable_tracker_engine.hpp"
#include "core/analysis_context.hpp"

namespace fs = std::filesystem;

TEST(DataFlowTest, ShouldTraceConstantUsage) {
    fs::path test_dir = fs::absolute("tmp_test_flow");
    if (fs::exists(test_dir)) fs::remove_all(test_dir);
    fs::create_directories(test_dir);
    
    std::ofstream f1(test_dir / "FlowTest.smali");
    f1 << ".class Lcom/test/FlowTest;\n"
       << ".super Ljava/lang/Object;\n"
       << ".method public static trace()V\n"
       << "    .registers 2\n"
       << "    const-string v0, \"secret\"\n"
       << "    invoke-static {v0}, Ltest/Log;->i(Ljava/lang/String;)V\n"
       << "    move-object v1, v0\n"
       << "    return-void\n"
       << ".end method\n";
    f1.close();

    core::AnalysisContext ctx(test_dir);
    engines::VariableTrackerEngine engine;
    
    // A query para o VariableTracker deve ser METHOD?CONST
    engines::SearchConfig config;
    config.query = "Lcom/test/FlowTest;->trace()V?secret";

    auto results = engine.search(ctx, config);
    
    ASSERT_FALSE(results.empty());
    std::cout << "Resultado: " << results[0].context << std::endl;

    fs::remove_all(test_dir);

    EXPECT_TRUE(results[0].context.find("CONST") != std::string::npos);
    EXPECT_TRUE(results[0].context.find("CALL") != std::string::npos);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
