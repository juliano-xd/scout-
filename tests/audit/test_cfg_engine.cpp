#include <gtest/gtest.h>
#include "engines/cfg_engine.hpp"
#include <iostream>

using namespace engines;

TEST(CFGAudit, TrackerCase) {
    std::string_view body = 
        ".method public run()V\n"
        "    const-string v0, \"secret_token\"\n"
        "    move-object v1, v0\n"
        "    invoke-static {v1}, Landroid/util/Log;->d(Ljava/lang/String;Ljava/lang/String;)I\n"
        ".end method";
    
    CFG cfg = CFGEngine::build_cfg(body);
    
    std::cout << "DEBUG: CFG blocks found: " << cfg.blocks.size() << std::endl;
    for (const auto& b : cfg.blocks) {
        std::cout << "DEBUG: Block " << b.id << " content:\n" << b.code_content << std::endl;
    }
    
    ASSERT_GE(cfg.blocks.size(), 1);
    EXPECT_TRUE(cfg.blocks[0].code_content.find("const-string") != std::string::npos);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
