#include <gtest/gtest.h>
#include "engines/resource_mapping_engine.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace fs = std::filesystem;

class ResMapMassiveTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / ("scout_res_massive_" + std::to_string(GetParam()));
        fs::create_directories(temp_dir / "res/values");
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_public_xml(int count) {
        std::ofstream ofs(temp_dir / "res/values/public.xml");
        ofs << "<resources>\n";
        for(int i=0; i<count; ++i) {
            std::stringstream ss;
            ss << "0x7f" << std::setfill('0') << std::setw(6) << std::hex << i;
            ofs << "    <public type=\"string\" name=\"string_" << i << "\" id=\"" << ss.str() << "\" />\n";
        }
        ofs << "</resources>\n";
    }

    fs::path temp_dir;
};

TEST_P(ResMapMassiveTest, ResourceResolutionTest) {
    int i = GetParam();
    write_public_xml(100); // 100 recursos no arquivo

    engines::ResourceMappingEngine engine;
    engines::SearchConfig config;
    
    std::stringstream ss;
    ss << "0x7f" << std::setfill('0') << std::setw(6) << std::hex << i;
    config.query = ss.str();
    
    auto results = ([&](){ core::AnalysisContext ctx(temp_dir); return engine.search(ctx, config); })();

    ASSERT_FALSE(results.empty()) << "Falha ao resolver Recurso " << i;
    EXPECT_TRUE(results[0].line_content.find("string_" + std::to_string(i)) != std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(MassiveSuite, ResMapMassiveTest, ::testing::Range(0, 100));
