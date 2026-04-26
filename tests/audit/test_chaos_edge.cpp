#include <gtest/gtest.h>
#include "engines/variable_tracker_engine.hpp"
#include "engines/content_search_engine.hpp"
#include "core/analysis_context.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <thread>

namespace fs = std::filesystem;

/**
 * @brief Testes de estresse para o gerenciamento de memória do PointsToSet (SVO).
 */
TEST(ChaosEdgeTest, PointsToSetStressTest) {
    using engines::PointsToSet;
    using engines::LocusID;

    // Teste 1: Transição Small -> Large
    {
        PointsToSet pts;
        pts.add(1);
        pts.add(2);
        EXPECT_FALSE(pts.is_large);
        pts.add(3); // Transição aqui
        EXPECT_TRUE(pts.is_large);
        EXPECT_EQ(pts.count, 3);
        EXPECT_TRUE(pts.contains(1));
        EXPECT_TRUE(pts.contains(2));
        EXPECT_TRUE(pts.contains(3));
    }

    // Teste 2: Cópia e Atribuição com Large Storage
    {
        PointsToSet pts1;
        for(int i=0; i<10; ++i) pts1.add(i);
        
        PointsToSet pts2 = pts1; // Copy constructor
        EXPECT_TRUE(pts2.is_large);
        EXPECT_EQ(pts2.count, 10);
        
        PointsToSet pts3;
        pts3 = pts1; // Assignment operator
        EXPECT_TRUE(pts3.is_large);
        EXPECT_EQ(pts3.count, 10);
        
        pts1.clear();
        EXPECT_FALSE(pts1.is_large);
        EXPECT_TRUE(pts2.contains(5));
        EXPECT_TRUE(pts3.contains(9));
    }

    // Teste 3: Auto-atribuição
    {
        PointsToSet pts;
        for(int i=0; i<5; ++i) pts.add(i);
        pts = pts; 
        EXPECT_TRUE(pts.is_large);
        EXPECT_EQ(pts.count, 5);
    }
}

/**
 * @brief Teste de concorrência no AnalysisContext.
 */
TEST(ChaosEdgeTest, AnalysisContextConcurrency) {
    fs::path temp_dir = fs::temp_directory_path() / "scout_concurrency_test";
    fs::create_directories(temp_dir / "smali");
    
    std::string content = ".class Lcom/test/A;\n.method public test()V\nreturn-void\n.end method";
    std::ofstream(temp_dir / "smali/A.smali") << content;

    core::AnalysisContext ctx(temp_dir);
    std::vector<std::thread> threads;
    
    for(int i=0; i<20; ++i) {
        threads.emplace_back([&ctx]() {
            for(int j=0; j<100; ++j) {
                auto view = ctx.get_class_content("Lcom/test/A;");
                EXPECT_FALSE(view.empty());
            }
        });
    }

    for(auto& t : threads) t.join();
    fs::remove_all(temp_dir);
}

/**
 * @brief Teste de busca com query malformada ou gigante.
 */
TEST(ChaosEdgeTest, ContentSearchEdgeCases) {
    fs::path temp_dir = fs::temp_directory_path() / "scout_content_edge";
    fs::create_directories(temp_dir / "smali");
    std::ofstream(temp_dir / "smali/Edge.smali") << ".class Lcom/edge/A;\n# Comment\nconst v0, 0x123";

    engines::ContentSearchEngine engine;
    engines::SearchConfig config;
    core::AnalysisContext ctx(temp_dir);

    // Query vazia
    config.query = "";
    auto res1 = engine.search(ctx, config);
    EXPECT_TRUE(res1.empty());

    // Regex inválido
    config.query = "[a-z";
    config.search_type = "regex";
    auto res2 = engine.search(ctx, config);
    EXPECT_TRUE(res2.empty());

    // Query gigante (DoS attack)
    config.query = std::string(10000, 'A');
    config.search_type = "string";
    auto res3 = engine.search(ctx, config);
    EXPECT_TRUE(res3.empty());

    fs::remove_all(temp_dir);
}
