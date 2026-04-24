#include <gtest/gtest.h>
#include "engines/variable_tracker_engine.hpp"
#include "core/analysis_context.hpp"
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

class VariableTrackerMassiveTest : public ::testing::Test {
protected:
    fs::path temp_dir;

    void SetUp() override {
        temp_dir = fs::temp_directory_path() / ("scout_track_massive_" + std::to_string(rand()));
        fs::create_directories(temp_dir / "smali/com/massive");
        fs::create_directories(temp_dir / "smali_classes2/com/massive");
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_smali(const fs::path& base, const std::string& class_name, const std::string& content) {
        fs::path p = base / (class_name.substr(1, class_name.size() - 2) + ".smali");
        fs::create_directories(p.parent_path());
        std::ofstream ofs(p);
        ofs << content;
    }
};

// Gerador de testes para atingir a marca de 100 variantes
TEST_F(VariableTrackerMassiveTest, MassiveLifetimeTracking) {
    engines::VariableTrackerEngine engine;

    for (int i = 0; i < 100; ++i) {
        std::string class_a = "Lcom/massive/A" + std::to_string(i) + ";";
        std::string class_b = "Lcom/massive/B" + std::to_string(i) + ";";
        
        // Smali da Classe A
        std::string smali_a = ".class " + class_a + "\n"
                             ".super Ljava/lang/Object;\n"
                             ".method public start(Ljava/lang/String;)V\n"
                             "    .registers 4\n"
                             "    move-object v0, p1\n";
        
        // Adicionar ruído (moves extras)
        for (int j = 0; j < (i % 5); ++j) {
            smali_a += "    move-object v" + std::to_string(j+1) + ", v" + std::to_string(j) + "\n";
        }
        
        smali_a += "    invoke-static {v" + std::to_string(i % 5) + "}, " + class_b + "->process(Ljava/lang/String;)V\n"
                   "    return-void\n"
                   ".end method\n";

        // Smali da Classe B (em smali_classes2 para testar multi-dex)
        std::string smali_b = ".class " + class_b + "\n"
                             ".super Ljava/lang/Object;\n"
                             ".method public static process(Ljava/lang/String;)V\n"
                             "    .registers 2\n"
                             "    iput-object p0, p0, " + class_b + "->stored:Ljava/lang/String;\n"
                             "    return-void\n"
                             ".end method\n";

        write_smali(temp_dir / "smali", class_a, smali_a);
        write_smali(temp_dir / "smali_classes2", class_b, smali_b);

        engines::SearchConfig config;
        config.query = class_a + "->start(Ljava/lang/String;)V:p1";
        config.search_depth = 5;

        auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();
        
        // Verificações
        ASSERT_FALSE(results.empty()) << "Falha no teste " << i;
        std::string ctx = results[0].context;
        
        EXPECT_NE(ctx.find("MOVE"), std::string::npos) << "Erro de alias no teste " << i;
        EXPECT_NE(ctx.find("CALL"), std::string::npos) << "Erro de salto no teste " << i;
        EXPECT_NE(ctx.find("STORE"), std::string::npos) << "Erro de persistência no teste " << i;
        EXPECT_NE(ctx.find("stored"), std::string::npos) << "Erro de campo no teste " << i;
    }
}
