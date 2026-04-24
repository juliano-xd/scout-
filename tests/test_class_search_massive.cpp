#include <gtest/gtest.h>
#include "engines/class_search_engine.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class ClassSearchMassiveTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / "scout_class_search_massive";
        fs::create_directories(temp_dir);
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void create_deep_structure(int depth, const std::string& name) {
        fs::path p = temp_dir / "smali";
        for(int i=0; i<depth; ++i) p /= "pkg" + std::to_string(i);
        fs::create_directories(p);
        std::ofstream ofs(p / (name + ".smali"));
        ofs << ".class public L" << name << ";\n";
    }

    fs::path temp_dir;
};

TEST_P(ClassSearchMassiveTest, DeepPathDiscoveryTest) {
    int i = GetParam();
    std::string name = "TargetClass" + std::to_string(i);
    create_deep_structure((i % 15) + 1, name);

    engines::ClassSearchEngine engine;
    engines::SearchConfig config;
    config.query = name;
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();

    ASSERT_FALSE(results.empty()) << "Falha ao encontrar classe em profundidade " << (i % 15);
    EXPECT_TRUE(results[0].file_path.string().find(name) != std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(MassiveSuite, ClassSearchMassiveTest, ::testing::Range(0, 100));
