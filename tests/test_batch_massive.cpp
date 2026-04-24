#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

class BatchMassiveTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / "scout_batch_massive";
        fs::create_directories(temp_dir);
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    void write_batch_file(const std::vector<std::string>& commands) {
        std::ofstream ofs(temp_dir / "commands.txt");
        for(const auto& cmd : commands) ofs << cmd << "\n";
    }

    fs::path temp_dir;
};

TEST_P(BatchMassiveTest, CommandSequencingTest) {
    int i = GetParam();
    std::vector<std::string> cmds;
    for(int j=0; j < (i % 10) + 1; ++j) {
        cmds.push_back("--search \"Query_" + std::to_string(j) + "\"");
    }
    write_batch_file(cmds);

    // O teste aqui foca na existência e leitura correta do arquivo de batch
    EXPECT_TRUE(fs::exists(temp_dir / "commands.txt"));
    std::ifstream ifs(temp_dir / "commands.txt");
    std::string line;
    int count = 0;
    while(std::getline(ifs, line)) count++;
    EXPECT_EQ(count, cmds.size());
}

INSTANTIATE_TEST_SUITE_P(MassiveSuite, BatchMassiveTest, ::testing::Range(0, 100));
