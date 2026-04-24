#include <gtest/gtest.h>
#include "engines/resource_mapping_engine.hpp"
#include "engines/cfg_engine.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class NewEnginesTest : public ::testing::Test {
protected:
    fs::path test_dir;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "scout_new_engines_test";
        fs::create_directories(test_dir / "res" / "values");
        fs::create_directories(test_dir / "smali" / "com" / "example");
        
        // public.xml
        std::ofstream(test_dir / "res" / "values" / "public.xml") 
            << "<resources>\n"
            << "  <public type=\"string\" name=\"app_name\" id=\"0x7f0b0001\" />\n"
            << "  <public type=\"id\" name=\"btn_login\" id=\"0x7f0b0002\" />\n"
            << "</resources>\n";

        // R$string.smali
        std::ofstream(test_dir / "smali" / "com" / "example" / "R$string.smali") 
            << ".class public Lcom/example/R$string;\n"
            << ".field public static final app_name:I = 0x7f0b0001\n";

        // Target for CFG
        std::ofstream(test_dir / "smali" / "com" / "example" / "Target.smali") 
            << ".class public Lcom/example/Target;\n"
            << ".method public complexMethod(Z)V\n"
            << "    if-eqz p1, :cond_0\n"
            << "    const-string v0, \"True\"\n"
            << "    goto :goto_0\n"
            << "    :cond_0\n"
            << "    const-string v0, \"False\"\n"
            << "    :goto_0\n"
            << "    return-void\n"
            << ".end method\n";
    }

    void TearDown() override {
        fs::remove_all(test_dir);
    }
};

TEST_F(NewEnginesTest, ResourceMappingWorks) {
    engines::ResourceMappingEngine engine;
    
    // Test ID lookup
    engines::SearchConfig config;
    config.query = "0x7f0b0001";
    auto results = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
    
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].line_content, "string/app_name");

    // Test Name lookup
    config.query = "btn_login";
    results = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].context, "0x7f0b0002");
}

TEST_F(NewEnginesTest, CFGGenerationWorks) {
    engines::CFGEngine engine;
    
    engines::SearchConfig config;
    config.query = "Lcom/example/Target;->complexMethod(Z)V";
    
    auto results = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
    
    ASSERT_EQ(results.size(), 1);
    // Verificar se contém S-Expression com blocos
    EXPECT_TRUE(results[0].line_content.find("(cfg") != std::string::npos);
    EXPECT_TRUE(results[0].line_content.find("BB0") != std::string::npos);
    EXPECT_TRUE(results[0].line_content.find("BB1") != std::string::npos);
    EXPECT_TRUE(results[0].line_content.find("BB2") != std::string::npos);
}
