#include <gtest/gtest.h>
#include "engines/xref_search_engine.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class XrefMassiveTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / ("scout_xref_massive_" + std::to_string(GetParam()));
        fs::create_directories(temp_dir / "smali");
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_smali(int i) {
        // Criar um alvo e um chamador distintos
        std::ofstream ofs(temp_dir / "smali/Target.smali");
        ofs << ".class public LTarget;\n";
        ofs << ".method public static targetMethod()V\n";
        ofs << "    return-void\n";
        ofs << ".end method\n";

        std::ofstream ofs2(temp_dir / "smali/Caller.smali");
        ofs2 << ".class public LCaller;\n";
        ofs2 << ".method public test" << i << "()V\n";
        ofs2 << "    invoke-static {}, LTarget;->targetMethod()V\n";
        ofs2 << ".end method\n";
    }

    fs::path temp_dir;
};

TEST_P(XrefMassiveTest, OpcodeDiversityTest) {
    int i = GetParam();
    write_smali(i);

    engines::XrefSearchEngine engine;
    engines::SearchConfig config;
    config.query = "LTarget;->targetMethod()V";
    config.direction = "callers";
    config.search_depth = 1;
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();

    // Debug: Mostrar o que foi encontrado
    if (results.empty()) {
        std::cout << "DEBUG: Nenhum resultado para variante " << i << "\n";
        std::cout << "DEBUG: Arquivos no diretório: \n";
        for (const auto& entry : fs::recursive_directory_iterator(temp_dir)) {
            if (fs::is_regular_file(entry.path())) {
                std::cout << "  - " << entry.path() << " (Size: " << fs::file_size(entry.path()) << ")\n";
            } else {
                std::cout << "  - " << entry.path() << " (Dir)\n";
            }
        }
    }

    bool found_caller = false;
    std::cout << "DEBUG: Resultados encontrados (" << results.size() << "):\n";
    for (const auto& res : results) {
        std::cout << "  - File: " << res.file_path << " | Context: " << res.context << "\n";
        if (res.context.find("LCaller;") != std::string::npos) {
            found_caller = true;
        }
    }
    
    ASSERT_TRUE(found_caller) << "Nao encontrou a chamada em LCaller; para variante " << i;
}

INSTANTIATE_TEST_SUITE_P(MassiveSuite, XrefMassiveTest, ::testing::Range(0, 100));
