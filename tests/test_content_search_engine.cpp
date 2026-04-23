#include <gtest/gtest.h>
#include "engines/content_search_engine.hpp"
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
        fs::path temp_dir = fs::temp_directory_path() / "scout_test_content_engine";
        fs::create_directories(temp_dir);
        
        // Criar estrutura de diretórios
        fs::create_directories(temp_dir / "smali" / "com" / "example");
        fs::create_directories(temp_dir / "smali_classes2" / "android" / "support");
        
        // Arquivo 1: AuthManager.smali com strings e inteiros
        std::ofstream file1(temp_dir / "smali" / "com" / "example" / "AuthManager.smali");
        file1 << ".class public Lcom/example/AuthManager;\n";
        file1 << ".super Ljava/lang/Object;\n";
        file1 << "\n";
        file1 << ".method public doLogin(Ljava/lang/String;Ljava/lang/String;)Z\n";
        file1 << "    .locals 3\n";
        file1 << "    const-string v0, \"https://api.example.com/login\"\n";
        file1 << "    const/16 v1, 0x1\n";
        file1 << "    const v2, 0x3f800000    # 1.0f\n";
        file1 << "    invoke-static {v0, v1, v2}, Lcom/example/Network;->send(Ljava/lang/String;IZ)Z\n";
        file1 << "    move-result v0\n";
        file1 << "    return v0\n";
        file1 << ".end method\n";
        file1 << "\n";
        file1 << ".method public checkPassword(Ljava/lang/String;)Z\n";
        file1 << "    .locals 1\n";
        file1 << "    const-string v0, \"password_check\"\n";
        file1 << "    invoke-static {v0}, Landroid/util/Log;->d(Ljava/lang/String;Ljava/lang/String;)I\n";
        file1 << "    const/4 v0, 0x1\n";
        file1 << "    return v0\n";
        file1 << ".end method\n";
        file1.close();
        
        // Arquivo 2: LoginActivity.smali com mais padrões
        std::ofstream file2(temp_dir / "smali" / "com" / "example" / "LoginActivity.smali");
        file2 << ".class public Lcom/example/LoginActivity;\n";
        file2 << ".super Landroid/app/Activity;\n";
        file2 << "\n";
        file2 << ".method protected onCreate(Landroid/os/Bundle;)V\n";
        file2 << "    .locals 2\n";
        file2 << "    invoke-super {p0, p1}, Landroid/app/Activity;->onCreate(Landroid/os/Bundle;)V\n";
        file2 << "    const-string v0, \"login_activity_started\"\n";
        file2 << "    const/4 v1, 0x7f0a0001\n";
        file2 << "    invoke-static {v0, v1}, Lcom/example/Logger;->logEvent(Ljava/lang/String;I)V\n";
        file2 << "    return-void\n";
        file2 << ".end method\n";
        file2.close();
        
        // Arquivo 3: CryptoUtils.smali com padrões criptográficos
        std::ofstream file3(temp_dir / "smali" / "com" / "example" / "CryptoUtils.smali");
        file3 << ".class public Lcom/example/CryptoUtils;\n";
        file3 << ".super Ljava/lang/Object;\n";
        file3 << "\n";
        file3 << ".method public static encrypt([B)Ljava/lang/String;\n";
        file3 << "    .locals 5\n";
        file3 << "    const-string v0, \"AES\"\n";
        file3 << "    invoke-static {v0}, Ljavax/crypto/Cipher;->getInstance(Ljava/lang/String;)Ljavax/crypto/Cipher;\n";
        file3 << "    move-result-object v0\n";
        file3 << "    const/16 v1, 0x100\n";
        file3 << "    new-array v1, v1, [B\n";
        file3 << "    invoke-virtual {v0, v1}, Ljavax/crypto/Cipher;->doFinal([B)[B\n";
        file3 << "    move-result-object v0\n";
        file3 << "    return-object v0\n";
        file3 << ".end method\n";
        file3.close();
        
        // Arquivo 4: Classe do sistema (para teste de filtro)
        fs::create_directories(temp_dir / "smali_classes2" / "android" / "support" / "v4");
        std::ofstream file4(temp_dir / "smali_classes2" / "android" / "support" / "v4" / "Fragment.smali");
        file4 << ".class public Landroid/support/v4/app/Fragment;\n";
        file4 << ".super Ljava/lang/Object;\n";
        file4 << ".method public onCreate()V\n";
        file4 << "    const-string v0, \"Fragment.onCreate\"\n";
        file4 << "    invoke-static {v0}, Landroid/util/Log;->d(Ljava/lang/String;Ljava/lang/String;)I\n";
        file4 << "    return-void\n";
        file4 << ".end method\n";
        file4.close();
        
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
// Testes do ContentSearchEngine
// ==========================================

TEST(ContentSearchEngine, ConstructorAndName) {
    ContentSearchEngine engine;
    EXPECT_EQ(engine.name(), "content");
    EXPECT_FALSE(engine.description().empty());
}

TEST(ContentSearchEngine, SearchStringLiteral) {
    ContentSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "password_check";
    config.max_results = 100;
    // config.search_type não é usado explicitamente, mas o motor detecta pelo uso
    // Para este teste, assumimos busca por string literal
    
    auto results = engine.search(temp_dir, config);
    
    ASSERT_GE(results.size(), 1);
    // Deve encontrar a linha com "password_check" em AuthManager.checkPassword
    bool found = false;
    for (const auto& r : results) {
        if (r.line_content.find("password_check") != std::string::npos) {
            found = true;
            EXPECT_TRUE(r.context.find("checkPassword") != std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found);
    
    cleanup_test_structure(temp_dir);
}

TEST(ContentSearchEngine, SearchStringCaseInsensitive) {
    ContentSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "LOGIN_ACTIVITY_STARTED"; // maiúsculas, mas arquivo tem minúsculas
    config.max_results = 100;
    config.case_sensitive = false;
    
    auto results = engine.search(temp_dir, config);
    
    // Deve encontrar mesmo com case insensitive
    bool found = false;
    for (const auto& r : results) {
        if (r.line_content.find("login_activity_started") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    
    cleanup_test_structure(temp_dir);
}

TEST(ContentSearchEngine, SearchRegex) {
    ContentSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = R"(const-string v\d+,\s*".*")"; // Regex para const-string com qualquer texto
    config.max_results = 100;
    config.search_type = "regex";
    
    auto results = engine.search(temp_dir, config);
    
    // Deve encontrar múltiplas linhas com const-string
    ASSERT_GE(results.size(), 3); // Pelo menos 3 const-strings nos arquivos de teste
    
    cleanup_test_structure(temp_dir);
}

TEST(ContentSearchEngine, SearchIntegerDecimal) {
    ContentSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "0x1"; // Valor hexadecimal que aparece em vários lugares
    config.max_results = 100;
    
    auto results = engine.search(temp_dir, config);
    
    // Deve encontrar linhas com 0x1 (ex: const/16 v1, 0x1)
    ASSERT_GE(results.size(), 1);
    
    bool found_in_auth = false;
    for (const auto& r : results) {
        if (r.file_path.filename().string() == "AuthManager.smali" && 
            r.line_content.find("0x1") != std::string::npos) {
            found_in_auth = true;
            break;
        }
    }
    EXPECT_TRUE(found_in_auth);
    
    cleanup_test_structure(temp_dir);
}

TEST(ContentSearchEngine, SearchIntegerHex) {
    ContentSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "0x7f0a0001"; // Resource ID hexadecimal
    config.max_results = 100;
    
    auto results = engine.search(temp_dir, config);
    
    // Deve encontrar em LoginActivity.smali
    bool found = false;
    for (const auto& r : results) {
        if (r.file_path.filename().string() == "LoginActivity.smali" &&
            r.line_content.find("0x7f0a0001") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    
    cleanup_test_structure(temp_dir);
}

TEST(ContentSearchEngine, SearchContextTracking) {
    ContentSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "invoke-static";
    config.max_results = 100;
    
    auto results = engine.search(temp_dir, config);
    
    // Verificar que o contexto (método atual) é rastreado corretamente
    bool found_in_check_password = false;
    for (const auto& r : results) {
        if (r.context.find("checkPassword") != std::string::npos && 
            r.file_path.filename().string() == "AuthManager.smali") {
            found_in_check_password = true;
            break;
        }
    }
    EXPECT_TRUE(found_in_check_password);
    
    cleanup_test_structure(temp_dir);
}

TEST(ContentSearchEngine, SearchMaxResultsLimit) {
    ContentSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "const"; // Muitas ocorrências
    config.max_results = 2; // Limite baixo
    
    auto results = engine.search(temp_dir, config);
    
    EXPECT_LE(results.size(), 2);
    
    cleanup_test_structure(temp_dir);
}

TEST(ContentSearchEngine, SearchEmptyQuery) {
    ContentSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "";
    config.max_results = 100;
    
    auto results = engine.search(temp_dir, config);
    
    EXPECT_TRUE(results.empty());
    
    cleanup_test_structure(temp_dir);
}

TEST(ContentSearchEngine, SearchInvalidDirectory) {
    ContentSearchEngine engine;
    fs::path invalid_dir = "/path/that/does/not/exist";
    
    SearchConfig config;
    config.query = "test";
    config.max_results = 100;
    
    auto results = engine.search(invalid_dir, config);
    
    EXPECT_TRUE(results.empty());
}



TEST(ContentSearchEngine, GetStats) {
    ContentSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "const";
    config.max_results = 100;
    
    auto results = engine.search(temp_dir, config);
    auto stats = engine.get_stats();
    
    EXPECT_GE(stats.files_scanned, 1); // Pelo menos 1 arquivo escaneado
    EXPECT_GE(stats.matches_found, 0);
    EXPECT_GE(stats.total_time.count(), 0);
    EXPECT_GT(stats.files_scanned, 0); // Deve ter escaneado arquivos
    
    cleanup_test_structure(temp_dir);
}

TEST(ContentSearchEngine, SupportsConfig) {
    ContentSearchEngine engine;
    SearchConfig config;
    EXPECT_TRUE(engine.supports_config(config));
}

TEST(ContentSearchEngine, ParallelExecution) {
    // Teste simples para verificar que paralelização não causa crashes
    ContentSearchEngine engine;
    fs::path temp_dir = create_test_smali_structure();
    
    SearchConfig config;
    config.query = "const";
    config.max_results = 1000;
    
    // Executar múltiplas buscas paralelas (o motor internamente usa std::execution::par_unseq)
    auto results1 = engine.search(temp_dir, config);
    auto results2 = engine.search(temp_dir, config);
    
    // Ambos devem retornar resultados consistentes
    EXPECT_EQ(results1.size(), results2.size());
    
    cleanup_test_structure(temp_dir);
}

// ==========================================
// Testes de Integração com EngineRegistry
// ==========================================

TEST(ContentSearchEngine, RegistryIntegration) {
    auto& registry = EngineRegistry::instance();
    
    auto engine = registry.create("content");
    EXPECT_NE(engine, nullptr);
    EXPECT_EQ(engine->name(), "content");
    
    auto engines = registry.list_engines();
    EXPECT_TRUE(std::find(engines.begin(), engines.end(), "content") != engines.end());
}

TEST(ContentSearchEngine, FactoryFunction) {
    auto engine = create_content_search_engine();
    EXPECT_NE(engine, nullptr);
    EXPECT_TRUE(dynamic_cast<ContentSearchEngine*>(engine.get()) != nullptr);
}



int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    scout::register_all_components();
    return RUN_ALL_TESTS();
}