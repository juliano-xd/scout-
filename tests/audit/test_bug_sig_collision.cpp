#include <gtest/gtest.h>
#include "engines/variable_tracker_engine.hpp"
#include "core/analysis_context.hpp"
#include <fstream>
#include <iostream>

using namespace engines;

TEST(BugInvestigation, SmaliSignatureCollision) {
    auto root = std::filesystem::absolute("bug_root_collision");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "smali");
    
    {
        std::ofstream out(root / "smali/App.smali");
        out << ".class LApp;\n"
            << ".method public abc()V\n"
            << "    const-string v0, \"WRONG\"\n"
            << ".end method\n"
            << ".method public a()V\n"
            << "    const-string v0, \"RIGHT\"\n"
            << ".end method\n";
    }
    
    core::AnalysisContext ctx(root);
    // Check if class is indexed
    auto content = ctx.get_class_content("LApp;");
    std::cout << "DEBUG: Class LApp; content size: " << content.size() << std::endl;
    if (content.empty()) {
        std::cout << "ERROR: Class LApp; not found!" << std::endl;
    }

    VariableTrackerEngine engine;
    SearchConfig config;
    config.query = "LApp;->a()V?RIGHT";
    
    auto results = engine.search(ctx, config);
    std::cout << "DEBUG: Results size: " << results.size() << std::endl;
    if (!results.empty()) {
        std::cout << "DEBUG: Context: " << results[0].context << std::endl;
    }
    
    ASSERT_EQ(results.size(), 1);
    EXPECT_TRUE(results[0].context.find("\"RIGHT\"") != std::string::npos);
    
    std::filesystem::remove_all(root);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
