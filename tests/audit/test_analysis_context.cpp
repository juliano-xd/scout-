#include <gtest/gtest.h>
#include "core/analysis_context.hpp"
#include <fstream>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace core;

TEST(AnalysisContextAudit, FullWorkflow) {
    fs::path root = "context_audit_root";
    fs::remove_all(root);
    fs::create_directories(root / "smali/com/app");
    
    std::string content = ".class Lcom/app/Test;\n.super Ljava/lang/Object;";
    {
        std::ofstream out(root / "smali/com/app/Test.smali");
        out << content;
    }
    
    AnalysisContext ctx(root);
    
    // 1. Get existing class
    auto view1 = ctx.get_class_content("Lcom/app/Test;");
    EXPECT_EQ(view1, content);
    
    // 2. Cache hit (view pointer check)
    auto view2 = ctx.get_class_content("Lcom/app/Test;");
    EXPECT_EQ(view1.data(), view2.data()); // Pointer identity
    
    // 3. Missing class
    auto view3 = ctx.get_class_content("Lcom/app/Missing;");
    EXPECT_TRUE(view3.empty());
    
    fs::remove_all(root);
}

TEST(AnalysisContextAudit, Concurrency) {
    fs::path root = "context_audit_mt";
    fs::remove_all(root);
    fs::create_directories(root / "smali/pkg");
    
    for(int i=0; i<10; ++i) {
        std::ofstream(root / ("smali/pkg/Class" + std::to_string(i) + ".smali")) << "content" << i;
    }
    
    AnalysisContext ctx(root);
    
    std::vector<std::thread> threads;
    for(int i=0; i<20; ++i) {
        threads.emplace_back([&ctx, i]() {
            std::string name = "Lpkg/Class" + std::to_string(i % 10) + ";";
            for(int j=0; j<100; ++j) {
                auto v = ctx.get_class_content(name);
                ASSERT_FALSE(v.empty());
            }
        });
    }
    
    for(auto& t : threads) t.join();
    
    fs::remove_all(root);
}

TEST(AnalysisContextAudit, MalformedSmali) {
    fs::path root = "context_audit_malformed";
    fs::remove_all(root);
    fs::create_directories(root);

    // No .class line
    {
        std::ofstream out(root / "NoClass.smali");
        out << ".method public static main()V\n.end method";
    }

    // Malformed .class line
    {
        std::ofstream out(root / "Malformed.smali");
        out << ".class public"; // Missing name
    }

    AnalysisContext ctx(root);
    EXPECT_TRUE(ctx.get_class_content("LNoClass;").empty());
    EXPECT_TRUE(ctx.get_class_content("LMalformed;").empty());

    fs::remove_all(root);
}

TEST(AnalysisContextAudit, InvalidRootDir) {
    AnalysisContext ctx("/non/existent/path");
    EXPECT_TRUE(ctx.get_class_content("LA;").empty());
}

TEST(AnalysisContextAudit, LargeSmaliIndex) {
    fs::path root = "context_audit_large";
    fs::remove_all(root);
    fs::create_directories(root);
    for(int i=0; i<100; ++i) {
        std::string name = "LClass" + std::to_string(i) + ";";
        std::ofstream out(root / ("Class" + std::to_string(i) + ".smali"));
        out << ".class " << name << "\n";
    }

    AnalysisContext ctx(root);
    for(int i=0; i<100; ++i) {
        std::string name = "LClass" + std::to_string(i) + ";";
        EXPECT_FALSE(ctx.get_class_content(name).empty());
    }
    fs::remove_all(root);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
