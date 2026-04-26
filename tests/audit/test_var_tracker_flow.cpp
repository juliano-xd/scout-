#include <gtest/gtest.h>
#include "engines/variable_tracker_engine.hpp"
#include "core/analysis_context.hpp"
#include <fstream>
#include <iostream>

using namespace engines;

TEST(VarTrackerFlow, ParamTaintPropagation) {
    auto root = std::filesystem::absolute("var_audit_root_param");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "smali");
    
    {
        std::ofstream out(root / "smali/App.smali");
        out << ".class LApp;\n"
            << ".method public static run(Ljava/lang/String;)V\n"
            << "    .registers 2\n"
            << "    move-object v0, p0\n"
            << "    invoke-static {v0}, Landroid/util/Log;->d(Ljava/lang/String;Ljava/lang/String;)I\n"
            << ".end method\n";
    }
    
    core::AnalysisContext ctx(root);
    VariableTrackerEngine engine;
    SearchConfig config;
    config.query = "LApp;->run(Ljava/lang/String;)V";
    config.var_name = "p0"; // Track parameter
    config.search_depth = 1;
    
    auto results = engine.search(ctx, config);
    ASSERT_EQ(results.size(), 1);
    
    std::string context = results[0].context;
    std::cout << "DEBUG: Result Context: " << context << std::endl;
    
    EXPECT_TRUE(context.find("\"v0\"") != std::string::npos); // Move event
    EXPECT_TRUE(context.find("\"SINK_LEAK\"") != std::string::npos); // Log event
    
    std::filesystem::remove_all(root);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
