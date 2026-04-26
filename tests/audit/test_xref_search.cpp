#include <gtest/gtest.h>
#include "engines/xref_search_engine.hpp"
#include <fstream>

using namespace engines;

// 1. Context Tracking
TEST(XrefSearchAudit, UpdateContext) {
    XrefSearchEngine::ParseContext ctx;
    XrefSearchEngine::update_context(".class public Lcom/app/Main;", ctx);
    EXPECT_EQ(ctx.current_class, "Lcom/app/Main;");
    
    XrefSearchEngine::update_context(".method public onCreate(Landroid/os/Bundle;)V", ctx);
    // A implementação atual extrai o nome + assinatura de parâmetros
    EXPECT_EQ(ctx.current_method, "onCreate(Landroid/os/Bundle;)V");
    // O campo current_method_signature não é preenchido no .cpp (limitação conhecida)
}

// 2. Opcode Extraction
TEST(XrefSearchAudit, OpcodeExtraction) {
    EXPECT_EQ(XrefSearchEngine::extract_opcode("    invoke-virtual {v0}, Ljava/lang/String;->length()I"), "invoke-virtual");
}

// 3. System Class Detection
TEST(XrefSearchAudit, SystemClass) {
    EXPECT_TRUE(XrefSearchEngine::is_system_class("Landroid/app/Activity;"));
    EXPECT_FALSE(XrefSearchEngine::is_system_class("Lcom/app/MyClass;"));
}

// 4. Register Extraction
TEST(XrefSearchAudit, RegisterExtraction) {
    // A implementação inclui as chaves {}
    EXPECT_EQ(XrefSearchEngine::extract_registers("invoke-direct {v0, v1}, LApp;->m()V"), "{v0, v1}");
}

// 5. Taint Lite (Simplified)
TEST(XrefSearchAudit, TaintLite) {
    std::vector<std::string> history = {
        "const-string v0, \"secret_token\"",
        "invoke-static {v0}, LLogger;->log(Ljava/lang/String;)V"
    };
    // A implementação extrai o conteúdo SEM aspas
    EXPECT_EQ(XrefSearchEngine::trace_register_value("v0", history), "secret_token");
}

// 6. Search Workflow
TEST(XrefSearchAudit, SearchWorkflow) {
    auto root = std::filesystem::absolute("xref_audit_root_super");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "smali");
    {
        std::ofstream out(root / "smali/App.smali");
        out << ".class LApp;\n"
            << ".method public run()V\n"
            << "    invoke-static {}, LTarget;->hit()V\n"
            << ".end method\n";
    }
    core::AnalysisContext ctx(root);
    XrefSearchEngine engine;
    SearchConfig config;
    config.query = "LTarget;->hit()V";
    config.search_type = "xref";
    auto results = engine.search(ctx, config);
    ASSERT_EQ(results.size(), 1);
    // Contexto: LApp;->run()V regs:{} type:invoke [caller]
    EXPECT_TRUE(results[0].context.find("LApp;->run()V") != std::string::npos);
    EXPECT_TRUE(results[0].context.find("[caller]") != std::string::npos);
    std::filesystem::remove_all(root);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
