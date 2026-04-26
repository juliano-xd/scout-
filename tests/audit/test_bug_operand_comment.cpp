#include <gtest/gtest.h>
#include "engines/variable_tracker_engine.hpp"
#include "core/analysis_context.hpp"
#include <fstream>

using namespace engines;

TEST(BugInvestigation, SmaliOperandComment) {
    auto root = std::filesystem::absolute("bug_root_operand");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "smali");
    
    {
        std::ofstream out(root / "smali/App.smali");
        out << ".class LApp;\n"
            << ".method public run()V\n"
            << "    const-string v0, \"secret\" # This is a comment\n"
            << "    move-object v1, v0\n"
            << ".end method\n";
    }
    
    core::AnalysisContext ctx(root);
    VariableTrackerEngine engine;
    SearchConfig config;
    config.query = "LApp;->run()V?secret";
    
    auto results = engine.search(ctx, config);
    ASSERT_EQ(results.size(), 1);
    
    std::string context = results[0].context;
    std::cout << "DEBUG: Context: " << context << std::endl;
    
    // If bug exists, "secret" won't match "secret\" # This is a comment"
    // Unless normalized_val strips it (it doesn't seem to)
    EXPECT_TRUE(context.find("\"v1\"") != std::string::npos);
    
    std::filesystem::remove_all(root);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
