#include <gtest/gtest.h>
#include "engines/xref_search_engine.hpp"
#include "engines/engine_registry.hpp"
#include "engines/register_engines.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>

namespace fs = std::filesystem;
using namespace engines;

namespace {

    fs::path create_xref_test_structure() {
        fs::path temp_dir = fs::temp_directory_path() / "scout_test_xref_engine";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir / "smali" / "com" / "example");
        fs::create_directories(temp_dir / "smali" / "android" / "system");

        // AuthManager: define checkPassword, chama String.equals e SecretManager.getKey
        std::ofstream auth(temp_dir / "smali" / "com" / "example" / "AuthManager.smali");
        auth << ".class public Lcom/example/AuthManager;\n"
             << ".super Ljava/lang/Object;\n"
             << "\n"
             << ".method public checkPassword(Ljava/lang/String;)Z\n"
             << "    .registers 3\n"
             << "    invoke-virtual {p1}, Ljava/lang/String;->length()I\n"
             << "    invoke-static {p1}, Lcom/example/SecretManager;->getKey()Ljava/lang/String;\n"
             << "    invoke-virtual {p1, v0}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z\n"
             << "    const/4 v0, 0x1\n"
             << "    return v0\n"
             << ".end method\n"
             << "\n"
             << ".method public doLogin(Ljava/lang/String;Ljava/lang/String;)V\n"
             << "    .registers 4\n"
             << "    invoke-virtual {p0, p1}, Lcom/example/AuthManager;->checkPassword(Ljava/lang/String;)Z\n"
             << "    return-void\n"
             << ".end method\n";
        auth.close();

        // LoginActivity: chama checkPassword e AuthManager
        std::ofstream login(temp_dir / "smali" / "com" / "example" / "LoginActivity.smali");
        login << ".class public Lcom/example/LoginActivity;\n"
              << ".super Landroid/app/Activity;\n"
              << "\n"
              << ".method public onCreate()V\n"
              << "    .registers 3\n"
              << "    new-instance v0, Lcom/example/AuthManager;\n"
              << "    invoke-direct {v0}, Lcom/example/AuthManager;-><init>()V\n"
              << "    const-string v1, \"password\"\n"
              << "    invoke-virtual {v0, v1}, Lcom/example/AuthManager;->checkPassword(Ljava/lang/String;)Z\n"
              << "    return-void\n"
              << ".end method\n";
        login.close();

        // SecretManager: referenced by others
        std::ofstream secret(temp_dir / "smali" / "com" / "example" / "SecretManager.smali");
        secret << ".class public Lcom/example/SecretManager;\n"
               << ".super Ljava/lang/Object;\n"
               << "\n"
               << ".method public static getKey()Ljava/lang/String;\n"
               << "    .registers 1\n"
               << "    const-string v0, \"secret_key_123\"\n"
               << "    return-object v0\n"
               << ".end method\n";
        secret.close();

        // System class (should be filtered out by default)
        std::ofstream sys(temp_dir / "smali" / "android" / "system" / "SystemClass.smali");
        sys << ".class public Landroid/system/SystemClass;\n"
            << ".super Ljava/lang/Object;\n"
            << "\n"
            << ".method public callAuth()V\n"
            << "    .registers 2\n"
            << "    invoke-static {}, Lcom/example/AuthManager;->checkPassword(Ljava/lang/String;)Z\n"
            << "    return-void\n"
            << ".end method\n";
        sys.close();

        return temp_dir;
    }

    void cleanup_test_structure(const fs::path& d) {
        if (fs::exists(d)) fs::remove_all(d);
    }

} // namespace

// ==========================================
// Testes básicos do XrefSearchEngine
// ==========================================

TEST(XrefSearchEngine, ConstructorAndName) {
    XrefSearchEngine engine;
    EXPECT_EQ(engine.name(), "xref");
    EXPECT_FALSE(engine.description().empty());
}

TEST(XrefSearchEngine, SupportsConfig) {
    XrefSearchEngine engine;
    SearchConfig cfg;
    EXPECT_TRUE(engine.supports_config(cfg));
}

TEST(XrefSearchEngine, EmptyQueryReturnsEmpty) {
    XrefSearchEngine engine;
    fs::path temp_dir = create_xref_test_structure();

    SearchConfig cfg;
    cfg.query = "";
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, cfg); })();
    EXPECT_TRUE(results.empty());

    cleanup_test_structure(temp_dir);
}

TEST(XrefSearchEngine, InvalidDirectoryReturnsEmpty) {
    XrefSearchEngine engine;

    SearchConfig cfg;
    cfg.query = "Lcom/example/AuthManager;";
    auto results = ([&](){ core::AnalysisContext ctx("/nonexistent/path/12345"); return engine.search(ctx, cfg); })();
    EXPECT_TRUE(results.empty());
}

TEST(XrefSearchEngine, FindsCallers) {
    // BUG FIX REGRESSION: update_context deve receber linha trimada
    // LoginActivity.onCreate chama AuthManager->checkPassword
    XrefSearchEngine engine;
    fs::path temp_dir = create_xref_test_structure();

    SearchConfig cfg;
    cfg.query = "Lcom/example/AuthManager;->checkPassword";
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, cfg); })();

    EXPECT_GE(results.size(), 1u);

    bool found_login = false;
    for (const auto& r : results) {
        if (r.file_path.string().find("LoginActivity") != std::string::npos) {
            found_login = true;
            // Contexto deve conter o xref type (caller ou callee)
            bool has_type = r.context.find("caller") != std::string::npos ||
                            r.context.find("callee") != std::string::npos ||
                            r.context.find("reference") != std::string::npos;
            EXPECT_TRUE(has_type) << "Tipo ausente no contexto: " << r.context;
        }
    }
    EXPECT_TRUE(found_login);

    cleanup_test_structure(temp_dir);
}

TEST(XrefSearchEngine, FindsReferenceToClass) {
    XrefSearchEngine engine;
    fs::path temp_dir = create_xref_test_structure();

    SearchConfig cfg;
    cfg.query = "Lcom/example/SecretManager;";
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, cfg); })();

    // AuthManager.checkPassword chama SecretManager.getKey
    EXPECT_GE(results.size(), 1u);

    cleanup_test_structure(temp_dir);
}

TEST(XrefSearchEngine, FilterSystemClassesByDefault) {
    // O SystemClass no dir android/ chama AuthManager, mas system classes
    // devem ser filtradas quando include_system_ == false (default)
    XrefSearchEngine engine;
    engine.set_include_system(false);
    fs::path temp_dir = create_xref_test_structure();

    SearchConfig cfg;
    cfg.query = "Lcom/example/AuthManager;";
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, cfg); })();

    // Não deve ter resultados cujo caller_class começa com "Landroid/"
    for (const auto& r : results) {
        EXPECT_EQ(r.context.find("android/system"), std::string::npos)
            << "Sistema: resultado inesperado de classe de sistema: " << r.context;
    }

    cleanup_test_structure(temp_dir);
}

TEST(XrefSearchEngine, IncludesSystemClassesWhenEnabled) {
    XrefSearchEngine engine;
    engine.set_include_system(true);
    fs::path temp_dir = create_xref_test_structure();

    SearchConfig cfg;
    cfg.query = "Lcom/example/AuthManager;";
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, cfg); })();

    // Agora deve incluir a referência de Landroid/system/SystemClass
    bool found_system = false;
    for (const auto& r : results) {
        if (r.context.find("android/system") != std::string::npos) {
            found_system = true;
            break;
        }
    }
    EXPECT_TRUE(found_system);

    cleanup_test_structure(temp_dir);
}

TEST(XrefSearchEngine, MaxResultsRespected) {
    XrefSearchEngine engine;
    engine.set_include_system(true);
    fs::path temp_dir = create_xref_test_structure();

    SearchConfig cfg;
    cfg.query = "Lcom/example/AuthManager;";
    cfg.max_results = 1;
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, cfg); })();

    EXPECT_LE(results.size(), 1u);

    cleanup_test_structure(temp_dir);
}

TEST(XrefSearchEngine, StatsAreUpdatedAfterSearch) {
    // BUG FIX REGRESSION: stats_.matches_found deve ser xref_results.size(),
    // não results.size() (que estava 0 no momento errado)
    XrefSearchEngine engine;
    fs::path temp_dir = create_xref_test_structure();

    SearchConfig cfg;
    cfg.query = "Lcom/example/AuthManager;->checkPassword";
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, cfg); })();

    auto stats = engine.get_stats();
    // matches_found deve refletir o que foi encontrado
    EXPECT_EQ(stats.matches_found, results.size());
    EXPECT_GE(stats.files_scanned, 1u);
    EXPECT_GE(stats.total_time.count(), 0);

    cleanup_test_structure(temp_dir);
}

TEST(XrefSearchEngine, ResultContextContainsBothClassAndMethod) {
    XrefSearchEngine engine;
    fs::path temp_dir = create_xref_test_structure();

    SearchConfig cfg;
    cfg.query = "Lcom/example/AuthManager;->checkPassword";
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, cfg); })();

    for (const auto& r : results) {
        // Contexto deve estar no formato "class->method [type]"
        EXPECT_NE(r.context.find("->"), std::string::npos)
            << "Contexto malformado: " << r.context;
        EXPECT_NE(r.context.find("["), std::string::npos)
            << "Tipo ausente no contexto: " << r.context;
    }

    cleanup_test_structure(temp_dir);
}

TEST(XrefSearchEngine, DirectionCallersOnlyFindsCallers) {
    XrefSearchEngine engine;
    engine.set_direction("callers");
    fs::path temp_dir = create_xref_test_structure();

    SearchConfig cfg;
    cfg.query = "Lcom/example/AuthManager;->checkPassword";
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, cfg); })();

    for (const auto& r : results) {
        // Com direction=callers, o xref_type deve ser "caller"
        EXPECT_NE(r.context.find("caller"), std::string::npos)
            << "Tipo inesperado no modo callers: " << r.context;
    }

    cleanup_test_structure(temp_dir);
}

TEST(XrefSearchEngine, EngineNameInResults) {
    XrefSearchEngine engine;
    fs::path temp_dir = create_xref_test_structure();

    SearchConfig cfg;
    cfg.query = "Lcom/example/SecretManager;";
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, cfg); })();

    for (const auto& r : results) {
        EXPECT_EQ(r.engine_name, "xref");
    }

    cleanup_test_structure(temp_dir);
}

TEST(XrefSearchEngine, RegistryIntegration) {
    auto& registry = EngineRegistry::instance();
    auto engine = registry.create("xref");
    EXPECT_NE(engine, nullptr);
    EXPECT_EQ(engine->name(), "xref");
}

TEST(XrefSearchEngine, FactoryFunction) {
    auto engine = create_xref_search_engine();
    EXPECT_NE(engine, nullptr);
    EXPECT_TRUE(dynamic_cast<XrefSearchEngine*>(engine.get()) != nullptr);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    scout::register_all_components();
    return RUN_ALL_TESTS();
}
