#include <gtest/gtest.h>
#include "engines/deobf_engine.hpp"
#include <filesystem>
#include <fstream>
#include <random>

namespace fs = std::filesystem;

class DeobfMassiveTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / ("scout_deobf_massive_" + std::to_string(GetParam()));
        fs::create_directories(temp_dir / "smali");
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_smali(const std::string& name, const std::string& content) {
        fs::path smali_path = temp_dir / "smali";
        fs::create_directories(smali_path);
        std::ofstream ofs(smali_path / (name + ".smali"));
        ofs << content;
    }

    fs::path temp_dir;
};

std::string random_base64(int len) {
    static const char* b64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string res;
    for(int i=0; i<len; ++i) res += b64_chars[rand() % 64];
    return res;
}

std::string generate_deobf_variant(int i) {
    std::string content = ".class public LDeobfTest;\n";
    content += ".method public test()V\n";
    
    // Injetar strings "suspeitas" e strings normais
    if (i % 2 == 0) {
        // String Base64 longa (deve ser detectada)
        content += "    const-string v0, \"" + random_base64(25 + (i % 50)) + "\"\n";
    } else {
        // String normal curta (não deve ser detectada pela heurística de 20+ chars)
        content += "    const-string v0, \"short_string_" + std::to_string(i) + "\"\n";
    }

    content += "    return-void\n.end method\n";
    return content;
}

TEST_P(DeobfMassiveTest, HeuristicDetectionTest) {
    int variant = GetParam();
    write_smali("Test" + std::to_string(variant), generate_deobf_variant(variant));

    engines::DeobfEngine engine;
    engines::SearchConfig config;
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();

    if (variant % 2 == 0) {
        // Variantes pares têm strings longas e DEVEM ser detectadas
        ASSERT_FALSE(results.empty()) << "Falha ao detectar Base64 na variante " << variant;
        EXPECT_EQ(results[0].engine_name, "deobf");
    } else {
        // Variantes ímpares têm strings curtas e NÃO devem gerar alertas
        EXPECT_TRUE(results.empty()) << "Falso positivo na variante " << variant;
    }
}

INSTANTIATE_TEST_SUITE_P(
    MassiveSuite,
    DeobfMassiveTest,
    ::testing::Range(0, 100)
);
