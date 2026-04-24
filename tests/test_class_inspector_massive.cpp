#include <gtest/gtest.h>
#include "engines/class_inspector_engine.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class ClassInspectorMassiveTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / ("scout_inspector_massive_" + std::to_string(GetParam()));
        fs::create_directories(temp_dir / "smali");
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_smali(int i) {
        std::ofstream ofs(temp_dir / "smali/Test.smali");
        ofs << ".class public LTest;\n";
        ofs << ".super Ljava/lang/Object;\n";
        ofs << ".method public testMethod" << i << "()V\n";
        ofs << "    return-void\n";
        ofs << ".end method\n";
    }

    fs::path temp_dir;
};

TEST_P(ClassInspectorMassiveTest, DNAExtractionTest) {
    int i = GetParam();
    write_smali(i);

    engines::ClassInspectorEngine engine;
    engines::SearchConfig config;
    config.query = "LTest;";
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();

    ASSERT_FALSE(results.empty()) << "Falha ao extrair DNA para variante " << i;
    
    // O DNA deve conter a tag class-dna e o nome do metodo
    std::string content = results[0].context;
    bool has_dna = content.find("class-dna") != std::string::npos;
    bool has_method = content.find("testMethod" + std::to_string(i)) != std::string::npos;

    EXPECT_TRUE(has_dna) << "DNA nao contem tag 'class-dna'. Conteudo: " << content;
    EXPECT_TRUE(has_method) << "DNA nao contem o metodo esperado. Conteudo: " << content;
}

INSTANTIATE_TEST_SUITE_P(MassiveSuite, ClassInspectorMassiveTest, ::testing::Range(0, 100));
