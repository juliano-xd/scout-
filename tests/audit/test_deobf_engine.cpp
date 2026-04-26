#include <gtest/gtest.h>
#include "engines/deobf_engine.hpp"
#include <fstream>

using namespace engines;

// 1. Suspicious Detection
TEST(DeobfAudit, Suspicious) {
    DeobfEngine engine;
    // Base64 curto (< 20)
    EXPECT_FALSE(engine.is_suspicious("SGVsbG8="));
    // Base64 longo (>= 20)
    EXPECT_TRUE(engine.is_suspicious("SGVsbG8gV29ybGQgTW9yZSBUaGFuIDIwIENoYXJz"));
    // Não base64
    EXPECT_FALSE(engine.is_suspicious("Not a base64 string at all!!!!"));
}

// 2. Simple Decode
TEST(DeobfAudit, Decode) {
    DeobfEngine engine;
    EXPECT_EQ(engine.decode_simple("SGVsbG8="), "Hello");
}

// 3. Search Workflow
TEST(DeobfAudit, Search) {
    auto root = std::filesystem::absolute("deobf_audit_root_super");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "smali");
    
    {
        std::ofstream out(root / "smali/Obf.smali");
        // "SGVsbG8gV29ybGQgTW9yZSBUaGFuIDIwIENoYXJz" -> Hello World More Than 20 Chars
        out << ".class LObf;\n"
            << ".method public run()V\n"
            << "    const-string v0, \"SGVsbG8gV29ybGQgTW9yZSBUaGFuIDIwIENoYXJz\"\n"
            << ".end method\n";
    }
    
    core::AnalysisContext ctx(root);
    DeobfEngine engine;
    SearchConfig config;
    
    auto results = engine.search(ctx, config);
    ASSERT_EQ(results.size(), 1);
    
    // Check context (S-Expression)
    EXPECT_TRUE(results[0].context.find(":preview") != std::string::npos);
    EXPECT_TRUE(results[0].context.find("Hello World") != std::string::npos);
    
    std::filesystem::remove_all(root);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
