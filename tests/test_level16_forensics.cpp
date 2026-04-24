#include <gtest/gtest.h>
#include "engines/variable_tracker_engine.hpp"
#include "core/analysis_context.hpp"
#include "formatters/sexpr_formatter.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class Level16ForensicsTest : public ::testing::Test {
protected:
    fs::path temp_dir;

    void SetUp() override {
        temp_dir = fs::current_path() / "scout_level16_test";
        fs::create_directories(temp_dir / "smali/com/evil");
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_smali(const std::string& rel_path, const std::string& content) {
        fs::path p = temp_dir / rel_path;
        fs::create_directories(p.parent_path());
        std::ofstream ofs(p);
        ofs << content;
    }
};

/**
 * @brief Testa se o motor identifica o abismo da Reflexão (EES/EPS).
 */
TEST_F(Level16ForensicsTest, ReflectionDetectionTest) {
    write_smali("smali/com/evil/Reflect.smali", R"(.class Lcom/evil/Reflect;
.super Ljava/lang/Object;
.method public static hide(Ljava/lang/reflect/Method;)V
    .registers 3
    # v0 é o receptor (Method.invoke)
    move-object v0, p0
    const-string v1, "Landroid/telephony/SmsManager;"
    invoke-virtual {v0, v1}, Ljava/lang/reflect/Method;->invoke(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;
    return-void
.end method
)");

    engines::VariableTrackerEngine engine;
    engines::SearchConfig config;
    // Marcamos p0 (o objeto Method) como contaminado
    config.query = "Lcom/evil/Reflect;->hide(Ljava/lang/reflect/Method;)V:p0";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
    std::string context = results[0].context;
    
    // Verifica se o marcador de EES (External Execution Sink) foi ativado
    EXPECT_NE(context.find("EES_OPAQUE_ENTRY"), std::string::npos);
    EXPECT_NE(context.find("EXTERNAL_PAYLOAD_SHADOWING"), std::string::npos);
}

/**
 * @brief Testa se o motor identifica o uso de DexClassLoader (Código Não-Residente).
 */
TEST_F(Level16ForensicsTest, DexClassLoaderDetectionTest) {
    write_smali("smali/com/evil/Loader.smali", R"(.class Lcom/evil/Loader;
.super Ljava/lang/Object;
.method public static load(Ljava/lang/String;)V
    .registers 5
    # p0 contém o path do dex contaminado
    move-object v1, p0
    new-instance v0, Ldalvik/system/DexClassLoader;
    # v1 é o argumento do constructor
    invoke-direct {v0, v1}, Ldalvik/system/DexClassLoader;-><init>(Ljava/lang/String;)V
    return-void
.end method
)");

    engines::VariableTrackerEngine engine;
    engines::SearchConfig config;
    config.query = "Lcom/evil/Loader;->load(Ljava/lang/String;)V:p0";
    
    core::AnalysisContext ctx(temp_dir);
    auto results = engine.search(ctx, config);

    ASSERT_FALSE(results.empty());
    std::string context = results[0].context;
    
    // Verifica se o EPS (External Payload Shadowing) capturou o carregamento
    EXPECT_NE(context.find("EES_OPAQUE_ENTRY"), std::string::npos);
}

/**
 * @brief Testa a serialização semântica (CIS) para IA.
 */
TEST_F(Level16ForensicsTest, CausalIntentSerializationTest) {
    std::vector<engines::VariableEvent> events = {
        {"Lcom/test;->m()V", 10, "v0", "TAINT_PROP", "Ljava/lang/StringBuilder;->append", ""},
        {"Lcom/test;->m()V", 15, "v1", "SINK_LEAK", "Landroid/util/Log;->d", "SENSITIVE_DATA"},
        {"Lcom/test;->m()V", 20, "v2", "EES_OPAQUE_ENTRY", "Method;->invoke", "REFLECTIVE_EVASION"}
    };

    std::string output = formatters::format_causal_intent(events);
    
    // Verifica a estrutura da S-Expression (Trigger -> Bridge -> Action)
    EXPECT_NE(output.find("scout:causal-report"), std::string::npos);
    EXPECT_NE(output.find("intent-chain"), std::string::npos);
    EXPECT_NE(output.find("h1"), std::string::npos); // Handle semântico
    EXPECT_NE(output.find("EES_OPAQUE_ENTRY"), std::string::npos);
}
