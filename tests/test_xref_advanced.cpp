#include <gtest/gtest.h>
#include "engines/xref_search_engine.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class XrefAdvancedTest : public ::testing::Test {
protected:
    fs::path test_dir;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "scout_xref_adv_test";
        fs::create_directories(test_dir / "smali" / "com" / "example");
        
        // C -> B -> A
        std::ofstream(test_dir / "smali" / "com" / "example" / "A.smali") 
            << ".class public Lcom/example/A;\n"
            << ".method public targetMethod()V\n"
            << "    return-void\n"
            << ".end method\n";

        std::ofstream(test_dir / "smali" / "com" / "example" / "B.smali") 
            << ".class public Lcom/example/B;\n"
            << ".method public callerOfA()V\n"
            << "    invoke-virtual {p0}, Lcom/example/A;->targetMethod()V\n"
            << "    return-void\n"
            << ".end method\n";

        std::ofstream(test_dir / "smali" / "com" / "example" / "C.smali") 
            << ".class public Lcom/example/C;\n"
            << ".method public callerOfB()V\n"
            << "    invoke-static {}, Lcom/example/B;->callerOfA()V\n"
            << "    return-void\n"
            << ".end method\n";
            
        std::ofstream(test_dir / "smali" / "com" / "example" / "D.smali") 
            << ".class public Lcom/example/D;\n"
            << ".method public fieldAccessor()V\n"
            << "    sget-object v0, Lcom/example/A;->someField:I\n"
            << "    return-void\n"
            << ".end method\n";
    }

    void TearDown() override {
        fs::remove_all(test_dir);
    }
};

TEST_F(XrefAdvancedTest, RecursiveXrefDepth2) {
    engines::XrefSearchEngine engine;
    engine.set_depth(2);
    
    engines::SearchConfig config;
    config.query = "Lcom/example/A;->targetMethod()V";
    
    auto results = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
    
    // Depth 1: B calling A
    // Depth 2: C calling B
    ASSERT_EQ(results.size(), 2);
    
    bool found_b = false;
    bool found_c = false;
    
    for (const auto& r : results) {
        if (r.file_path.filename() == "B.smali") found_b = true;
        if (r.file_path.filename() == "C.smali") found_c = true;
    }
    
    EXPECT_TRUE(found_b);
    EXPECT_TRUE(found_c);
}

TEST_F(XrefAdvancedTest, OpcodeFiltering) {
    engines::XrefSearchEngine engine;
    engine.set_opcodes({"sget"});
    
    engines::SearchConfig config;
    config.query = "Lcom/example/A;";
    
    auto results = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
    
    // Only D should be found because it uses sget-object
    // B uses invoke-virtual, so it should be filtered out
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].file_path.filename(), "D.smali");
    EXPECT_TRUE(results[0].line_content.find("sget-object") != std::string::npos);
}

TEST_F(XrefAdvancedTest, MultiOpcodeFiltering) {
    engines::XrefSearchEngine engine;
    engine.set_opcodes({"invoke-virtual", "sget"});
    
    engines::SearchConfig config;
    config.query = "Lcom/example/A;";
    
    auto results = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
    
    // Found B (invoke-virtual) and D (sget-object)
    ASSERT_EQ(results.size(), 2);
}

TEST_F(XrefAdvancedTest, RegisterExtraction) {
    engines::XrefSearchEngine engine;
    engines::SearchConfig config;
    config.query = "Lcom/example/A;->targetMethod()V";
    
    auto results = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
    
    ASSERT_EQ(results.size(), 1);
    EXPECT_TRUE(results[0].context.find("regs:{p0}") != std::string::npos);
}

TEST_F(XrefAdvancedTest, FindsCallees) {
    engines::XrefSearchEngine engine;
    engine.set_direction("callees");
    
    engines::SearchConfig config;
    config.query = "Lcom/example/B;->callerOfA()V";
    
    auto results = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
    
    // B calls A
    ASSERT_EQ(results.size(), 1);
    EXPECT_TRUE(results[0].line_content.find("Lcom/example/A;->targetMethod()V") != std::string::npos);
}

TEST_F(XrefAdvancedTest, TaintAnalysisLite) {
    std::ofstream(test_dir / "smali" / "com" / "example" / "Taint.smali") 
        << ".class public Lcom/example/Taint;\n"
        << ".method public sensitiveCall()V\n"
        << "    const-string v0, \"SECRET_API_KEY\"\n"
        << "    invoke-static {v0}, Lcom/example/A;->targetMethod(Ljava/lang/String;)V\n"
        << "    return-void\n"
        << ".end method\n";

    engines::XrefSearchEngine engine;
    engine.set_enable_taint(true);
    
    engines::SearchConfig config;
    config.query = "Lcom/example/A;->targetMethod(Ljava/lang/String;)V";
    
    auto results = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
    
    ASSERT_EQ(results.size(), 1);
    EXPECT_TRUE(results[0].context.find("taint:SECRET_API_KEY") != std::string::npos);
    EXPECT_TRUE(results[0].context.find("type:invoke") != std::string::npos);
}

TEST_F(XrefAdvancedTest, AccessClassification) {
    engines::XrefSearchEngine engine;
    
    engines::SearchConfig config;
    config.query = "Lcom/example/A;";
    
    auto results = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
    
    // Find D.smali (sget)
    bool found_d = false;
    for (const auto& r : results) {
        if (r.file_path.filename() == "D.smali") {
            found_d = true;
            EXPECT_TRUE(r.context.find("type:read") != std::string::npos);
        }
    }
    EXPECT_TRUE(found_d);
}
