#include <gtest/gtest.h>
#include "engines/class_search_engine.hpp"
#include "engines/i_search_engine.hpp"
#include "core/analysis_context.hpp"
#include "engines/engine_registry.hpp"
#include "engines/register_engines.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>

namespace fs = std::filesystem;
using namespace engines;

namespace {

    // Helper: cria estrutura de diretório Smali temporária para testes
    fs::path create_test_smali_structure() {
        fs::path temp_dir = fs::temp_directory_path() / "scout_test_class_engine";
        fs::create_directories(temp_dir);
        
        // Criar alguns arquivos .smali em estrutura de pacote
        fs::create_directories(temp_dir / "smali" / "com" / "example");
        fs::create_directories(temp_dir / "smali_classes2" / "android" / "support");
        
        // Arquivo 1: Lcom/example/AuthManager.smali
        std::ofstream file1(temp_dir / "smali" / "com" / "example" / "AuthManager.smali");
        file1 << ".class public Lcom/example/AuthManager;\n";
        file1 << ".super Ljava/lang/Object;\n";
        file1 << ".method public doLogin()V\n";
        file1 << "    return-void\n";
        file1 << ".end method\n";
        file1.close();
        
        // Arquivo 2: Lcom/example/LoginActivity.smali
        std::ofstream file2(temp_dir / "smali" / "com" / "example" / "LoginActivity.smali");
        file2 << ".class public Lcom/example/LoginActivity;\n";
        file2 << ".super Landroid/app/Activity;\n";
        file2 << ".method public onCreate()V\n";
        file2 << "    invoke-super {p0}, Landroid/app/Activity;->onCreate()V\n";
        file2 << "    return-void\n";
        file2 << ".end method\n";
        file2.close();
        
        // Arquivo 3: Landroid/support/v4/app/Fragment.smali (classe do sistema)
        std::ofstream file3(temp_dir / "smali_classes2" / "android" / "support" / "v4" / "app" / "Fragment.smali");
        file3 << ".class public Landroid/support/v4/app/Fragment;\n";
        file3 << ".super Ljava/lang/Object;\n";
        file3 << ".method public onCreate()V\n";
        file3 << "    return-void\n";
        file3 << ".end method\n";
        file3.close();
        
        return temp_dir;
    }

    // Helper: limpa estrutura de teste
    void cleanup_test_structure(const fs::path& temp_dir) {
        if (fs::exists(temp_dir)) {
            fs::remove_all(temp_dir);
        }
    }

} // namespace

// ==========================================
// Testes do ClassSearchEngine
// ==========================================

TEST(ClassSearchEngine, ConstructorAndName) {
    ClassSearchEngine engine;
    EXPECT_EQ(engine.name(), "class");
    EXPECT_FALSE(engine.description().empty());
}

TEST(ClassSearchEngine, SearchExactDalvikNotation) {
    ClassSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "Lcom/example/AuthManager;";
    config.max_results = 100;
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();
    
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].file_path.filename().string(), "AuthManager.smali");
    EXPECT_EQ(results[0].context, "class:AuthManager");
    EXPECT_EQ(results[0].engine_name, "class");
    
    cleanup_test_structure(temp_dir);
}

TEST(ClassSearchEngine, SearchSubstringClassName) {
    ClassSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "Login";
    config.max_results = 100;
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();
    
    ASSERT_GE(results.size(), 1);
    // Deve encontrar LoginActivity.smali
    bool found_login = false;
    for (const auto& r : results) {
        if (r.file_path.filename().string() == "LoginActivity.smali") {
            found_login = true;
            break;
        }
    }
    EXPECT_TRUE(found_login);
    
    cleanup_test_structure(temp_dir);
}

TEST(ClassSearchEngine, SearchPartialPackageName) {
    ClassSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "example";
    config.max_results = 100;
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();
    
    // Deve encontrar AuthManager e LoginActivity (ambos em com/example/)
    ASSERT_GE(results.size(), 2);
    
    cleanup_test_structure(temp_dir);
}

TEST(ClassSearchEngine, SearchNonExistentClass) {
    ClassSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "Lcom/example/NonExistent;";
    config.max_results = 100;
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();
    
    EXPECT_TRUE(results.empty());
    
    cleanup_test_structure(temp_dir);
}

TEST(ClassSearchEngine, SearchWithMaxResultsLimit) {
    ClassSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "L"; // Vai pegar todas as classes que começam com L (todas)
    config.max_results = 2; // Limite baixo
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();
    
    EXPECT_LE(results.size(), 2);
    
    cleanup_test_structure(temp_dir);
}

TEST(ClassSearchEngine, SearchEmptyQuery) {
    ClassSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "";
    config.max_results = 100;
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();
    
    EXPECT_TRUE(results.empty());
    
    cleanup_test_structure(temp_dir);
}

TEST(ClassSearchEngine, SearchInvalidDirectory) {
    ClassSearchEngine engine;
    fs::path invalid_dir = "/path/that/does/not/exist";
    
    SearchConfig config;
    config.query = "Lcom/example/A;";
    config.max_results = 100;
    
    auto results = ([&](){ core::AnalysisContext ctx(invalid_dir); return engine.search(ctx, config); })();
    
    EXPECT_TRUE(results.empty());
}



TEST(ClassSearchEngine, GetStats) {
    ClassSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "Login";
    config.max_results = 100;
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();
    auto stats = engine.get_stats();
    
    EXPECT_GE(stats.matches_found, 0);
    EXPECT_GE(stats.total_time.count(), 0);
    
    cleanup_test_structure(temp_dir);
}

TEST(ClassSearchEngine, SupportsConfig) {
    ClassSearchEngine engine;
    SearchConfig config;
    EXPECT_TRUE(engine.supports_config(config));
}

TEST(ClassSearchEngine, DalvikNotationDetection) {
    // Testa função estática helper
    EXPECT_TRUE(ClassSearchEngine::is_dalvik_notation("Lcom/example/A;"));
    EXPECT_TRUE(ClassSearchEngine::is_dalvik_notation("Ljava/lang/String;"));
    EXPECT_FALSE(ClassSearchEngine::is_dalvik_notation("AuthManager"));
    EXPECT_FALSE(ClassSearchEngine::is_dalvik_notation("Lcom/example/A")); // sem ;
    EXPECT_FALSE(ClassSearchEngine::is_dalvik_notation(""));
}

TEST(ClassSearchEngine, NormalizeDalvik) {
    EXPECT_EQ(ClassSearchEngine::normalize_dalvik("Lcom/example/A;"), "com/example/A");
    EXPECT_EQ(ClassSearchEngine::normalize_dalvik("com/example/A"), "com/example/A");
    EXPECT_EQ(ClassSearchEngine::normalize_dalvik("Lcom/example/A"), "com/example/A");
    EXPECT_EQ(ClassSearchEngine::normalize_dalvik(""), "");
}

// ==========================================
// Testes de Integração com EngineRegistry
// ==========================================

TEST(ClassSearchEngine, RegistryIntegration) {
    auto& registry = EngineRegistry::instance();
    
    // Criar motor via registry
    auto engine = registry.create("class");
    EXPECT_NE(engine, nullptr);
    EXPECT_EQ(engine->name(), "class");
    
    // Verificar que está na lista
    auto engines = registry.list_engines();
    EXPECT_TRUE(std::find(engines.begin(), engines.end(), "class") != engines.end());
}

TEST(ClassSearchEngine, FactoryFunction) {
    auto engine = create_class_search_engine();
    EXPECT_NE(engine, nullptr);
    EXPECT_TRUE(dynamic_cast<ClassSearchEngine*>(engine.get()) != nullptr);
}



int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    scout::register_all_components();
    return RUN_ALL_TESTS();
}