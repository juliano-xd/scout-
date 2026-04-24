#include <gtest/gtest.h>
#include "cli/parser.hpp"

class CLIMassiveTest : public ::testing::TestWithParam<int> {
};

TEST_P(CLIMassiveTest, ArgumentCombinationTest) {
    int i = GetParam();
    
    // Simular diferentes combinações de argumentos
    std::vector<const char*> args = {"scout", "--path", ".", "--machine-sexpr"};
    
    std::string search_val = "query_" + std::to_string(i);
    if (i % 3 == 0) {
        args.push_back("--search");
        args.push_back(search_val.c_str());
    }
    
    if (i % 2 == 0) {
        args.push_back("--xref-depth");
        args.push_back("5");
    }

    auto config = cli::ScoutConfig::parse(static_cast<int>(args.size()), args.data());
    
    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(config->machine_sexpr);
    if (i % 3 == 0) EXPECT_EQ(config->search, search_val);
}

INSTANTIATE_TEST_SUITE_P(MassiveSuite, CLIMassiveTest, ::testing::Range(0, 100));
