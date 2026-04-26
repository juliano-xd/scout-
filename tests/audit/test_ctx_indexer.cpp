#include <gtest/gtest.h>
#include "core/analysis_context.hpp"
#include <fstream>

TEST(AnalysisContextAudit, IndexerRobustness) {
    auto root = std::filesystem::absolute("indexer_test");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "smali/com/app");
    
    {
        std::ofstream out(root / "smali/com/app/Main.smali");
        out << ".class Lcom/app/Main;\n";
    }
    
    core::AnalysisContext ctx(root);
    // Internally we can't see the index, but we can call get_class_content
    auto content = ctx.get_class_content("Lcom/app/Main;");
    EXPECT_FALSE(content.empty());
    EXPECT_TRUE(content.find(".class Lcom/app/Main;") != std::string_view::npos);
    
    std::filesystem::remove_all(root);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
