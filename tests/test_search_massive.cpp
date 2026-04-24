#include <gtest/gtest.h>
#include "engines/content_search_engine.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class SearchMassiveTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / ("scout_search_massive_" + std::to_string(GetParam()));
        fs::create_directories(temp_dir / "smali");
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_smali(const std::string& name, const std::string& content) {
        std::ofstream ofs(temp_dir / "smali" / (name + ".smali"));
        ofs << content;
    }

    fs::path temp_dir;
};

TEST_P(SearchMassiveTest, RegexAndTextualTest) {
    int i = GetParam();
    std::string secret = "SECRET_TOKEN_" + std::to_string(i);
    std::string content = ".class public LTest;\n.method public run()V\n";
    content += "    const-string v0, \"" + secret + "\"\n";
    content += "    return-void\n.end method\n";
    
    write_smali("File" + std::to_string(i), content);

    engines::ContentSearchEngine engine;
    engines::SearchConfig config;
    
    // Alternar entre busca literal e regex
    if (i % 2 == 0) {
        config.query = secret;
        config.search_type = "string";
    } else {
        config.query = "SECRET_TOKEN_\\d+";
        config.search_type = "regex";
    }
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();

    ASSERT_FALSE(results.empty()) << "Falha na busca da variante " << i;
    EXPECT_TRUE(results[0].line_content.find("SECRET_TOKEN") != std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(MassiveSuite, SearchMassiveTest, ::testing::Range(0, 100));
