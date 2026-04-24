#include <gtest/gtest.h>
#include "engines/ui_mapper_engine.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace fs = std::filesystem;

class UiMapperMassiveTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / "scout_ui_massive";
        fs::create_directories(temp_dir / "smali");
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_smali(const std::string& name, const std::string& content) {
        fs::path smali_path = temp_dir / "smali";
        fs::create_directories(smali_path);
        std::ofstream ofs(smali_path / (name + ".smali"));
        ofs << content;
    }

    fs::path temp_dir;
};

std::string generate_ui_smali(int i) {
    std::stringstream ss;
    ss << "0x7f" << std::setfill('0') << std::setw(6) << std::hex << i;
    std::string hex_id = ss.str();

    std::string content = ".class public Lcom/ui/Activity" + std::to_string(i) + ";\n";
    content += ".method public onCreate()V\n";
    content += "    const v0, " + hex_id + "\n";
    content += "    invoke-virtual {p0, v0}, Landroid/app/Activity;->findViewById(I)Landroid/view/View;\n";
    content += "    return-void\n.end method\n";
    return content;
}

TEST_P(UiMapperMassiveTest, ResourceIdTrackingTest) {
    int variant = GetParam();
    write_smali("Activity" + std::to_string(variant), generate_ui_smali(variant));

    std::stringstream ss;
    ss << "0x7f" << std::setfill('0') << std::setw(6) << std::hex << variant;
    std::string target_id = ss.str();

    engines::UiMapperEngine engine;
    engines::SearchConfig config;
    config.query = target_id;
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();

    ASSERT_FALSE(results.empty()) << "Falha ao mapear ID " << target_id << " na variante " << variant;
    EXPECT_EQ(results[0].engine_name, "ui_mapper");
}

INSTANTIATE_TEST_SUITE_P(
    MassiveSuite,
    UiMapperMassiveTest,
    ::testing::Range(0, 100)
);
