#include <gtest/gtest.h>
#include "engines/variable_tracker_engine.hpp"
#include "core/analysis_context.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class VariableTrackerTest : public ::testing::Test {
protected:
    fs::path temp_dir;

    void SetUp() override {
        temp_dir = fs::temp_directory_path() / "scout_track_test";
        fs::create_directories(temp_dir / "smali/com/test");
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_smali(const std::string& rel_path, const std::string& content) {
        std::ofstream ofs(temp_dir / rel_path);
        ofs << content;
    }
};

TEST_F(VariableTrackerTest, TrackLifetimeAcrossMethods) {
    // Classe A que chama B
    write_smali("smali/com/test/A.smali", R"(.class Lcom/test/A;
.super Ljava/lang/Object;
.method public test(Ljava/lang/String;)V
    .registers 3
    move-object v0, p1
    invoke-static {v0}, Lcom/test/B;->sink(Ljava/lang/String;)V
    return-void
.end method
)");

    // Classe B que recebe a variável
    write_smali("smali/com/test/B.smali", R"(.class Lcom/test/B;
.super Ljava/lang/Object;
.method public static sink(Ljava/lang/String;)V
    .registers 2
    invoke-virtual {p0}, Ljava/lang/String;->length()I
    return-void
.end method
)");

    engines::VariableTrackerEngine engine;
    engines::SearchConfig config;
    config.query = "Lcom/test/A;->test(Ljava/lang/String;)V:p1";
    config.search_depth = 2;

    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();
    ASSERT_FALSE(results.empty());

    std::string context = results[0].context;
    EXPECT_NE(context.find("MOVE"), std::string::npos);
    EXPECT_NE(context.find("CALL"), std::string::npos);
    EXPECT_NE(context.find("sink"), std::string::npos);
}
