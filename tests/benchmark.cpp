#include <benchmark/benchmark.h>
#include "core/scanner.hpp"
#include "engines/content_search_engine.hpp"
#include "engines/xref_search_engine.hpp"
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

class ScoutFixture : public benchmark::Fixture {
public:
    fs::path test_dir;

    void SetUp(const ::benchmark::State& state) {
        test_dir = fs::temp_directory_path() / "scout_benchmark_test";
        fs::create_directories(test_dir / "smali" / "com" / "example");
        
        // Generate a bunch of dummy files to test scale
        for (int i = 0; i < 1000; ++i) {
            std::string class_name = "DummyClass" + std::to_string(i);
            std::ofstream(test_dir / "smali" / "com" / "example" / (class_name + ".smali")) 
                << ".class public Lcom/example/" << class_name << ";\n"
                << ".super Ljava/lang/Object;\n"
                << ".method public doSomething()V\n"
                << "    const-string v0, \"test_string_" << i << "\"\n"
                << "    invoke-static {}, Lcom/example/Target;->importantMethod()Z\n"
                << "    return-void\n"
                << ".end method\n";
        }
        
        // Target file to find
        std::ofstream(test_dir / "smali" / "com" / "example" / "Target.smali") 
            << ".class public Lcom/example/Target;\n"
            << ".super Ljava/lang/Object;\n"
            << ".method public importantMethod()Z\n"
                << "    const/4 v0, 0x1\n"
                << "    return v0\n"
                << ".end method\n";
    }

    void TearDown(const ::benchmark::State& state) {
        fs::remove_all(test_dir);
    }
};

BENCHMARK_F(ScoutFixture, FindClassFileExact)(benchmark::State& state) {
    for (auto _ : state) {
        auto res = core::find_class_file(test_dir, "Lcom/example/Target;");
        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_F(ScoutFixture, FindClassesContaining)(benchmark::State& state) {
    for (auto _ : state) {
        auto res = core::find_classes_containing(test_dir, "DummyClass");
        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_F(ScoutFixture, SearchContentString)(benchmark::State& state) {
    engines::ContentSearchEngine engine;
    engines::SearchConfig config;
    config.query = "test_string_500";
    config.search_type = "string";

    for (auto _ : state) {
        auto res = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_F(ScoutFixture, SearchContentRegex)(benchmark::State& state) {
    engines::ContentSearchEngine engine;
    engines::SearchConfig config;
    config.query = "test_string_[0-9]+";
    config.search_type = "regex";

    for (auto _ : state) {
        auto res = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_F(ScoutFixture, XrefEngineCallers)(benchmark::State& state) {
    engines::XrefSearchEngine engine;
    engines::SearchConfig config;
    config.query = "Lcom/example/Target;->importantMethod()Z";
    
    for (auto _ : state) {
        auto res = ([&](){ core::AnalysisContext ctx(test_dir); return engine.search(ctx, config); })();
        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_MAIN();
