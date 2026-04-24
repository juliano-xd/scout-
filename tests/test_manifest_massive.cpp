#include <gtest/gtest.h>
#include "engines/manifest_engine.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class ManifestMassiveTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / ("scout_class_test_" + std::to_string(GetParam()));
        fs::create_directories(temp_dir / "smali");
        fs::create_directories(temp_dir);
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_manifest(const std::string& content) {
        std::ofstream ofs(temp_dir / "AndroidManifest.xml");
        ofs << content;
    }

    fs::path temp_dir;
};

// Gerador de manifestos variados
std::string generate_manifest_variant(int i) {
    std::string pkg = "com.test.variant" + std::to_string(i);
    std::string content = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    content += "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" package=\"" + pkg + "\">\n";
    
    // Adicionar permissões variadas
    for(int p=0; p < (i % 10); ++p) {
        content += "    <uses-permission android:name=\"android.permission.TEST_PERM_" + std::to_string(p) + "\" />\n";
    }

    // Casos de borda: tags malformadas, espaços extras, atributos em novas linhas
    if (i % 5 == 0) {
        content += "    <activity android:name=\".MainActivity\" \n              android:exported=\"" + std::string(i % 2 == 0 ? "true" : "false") + "\" />\n";
    } else {
        content += "    <activity android:name=\".Variant" + std::to_string(i) + "\" android:exported=\"true\" />\n";
    }

    content += "</manifest>";
    return content;
}

TEST_P(ManifestMassiveTest, RobustnessTest) {
    int variant = GetParam();
    write_manifest(generate_manifest_variant(variant));

    engines::ManifestEngine engine;
    engines::SearchConfig config;
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();

    ASSERT_FALSE(results.empty()) << "Falha na variante " << variant;
    EXPECT_EQ(results[0].engine_name, "manifest");
    
    // Verificar se o package foi extraído corretamente
    std::string expected_pkg = "com.test.variant" + std::to_string(variant);
    EXPECT_TRUE(results[0].context.find(expected_pkg) != std::string::npos);
}

// Instanciar 100 testes para esta funcionalidade
INSTANTIATE_TEST_SUITE_P(
    MassiveSuite,
    ManifestMassiveTest,
    ::testing::Range(0, 100)
);
