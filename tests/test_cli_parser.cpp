#include <gtest/gtest.h>
#include "cli/parser.hpp"

TEST(CliParserTest, ParsesFlagCorrectly) {
    const char* argv[] = {"scout", "--manifest"};
    auto config = cli::ScoutConfig::parse(2, argv);
    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(config->manifest);
    EXPECT_FALSE(config->verbose);
}

TEST(CliParserTest, ParsesStringOptionCorrectly) {
    const char* argv[] = {"scout", "--search", "Lcom/example/Auth;"};
    auto config = cli::ScoutConfig::parse(3, argv);
    ASSERT_TRUE(config.has_value());
    ASSERT_TRUE(config->search.has_value());
    EXPECT_EQ(config->search.value(), "Lcom/example/Auth;");
}

TEST(CliParserTest, ParsesIntOptionCorrectly) {
    const char* argv[] = {"scout", "--search-max", "500"};
    auto config = cli::ScoutConfig::parse(3, argv);
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->search_max, 500);
}

TEST(CliParserTest, ThrowsParseErrorOnInvalidInt) {
    const char* argv[] = {"scout", "--search-max", "invalid"};
    EXPECT_THROW(cli::ScoutConfig::parse(3, argv), cli::ParseError);
}

TEST(CliParserTest, ParsesStringListCorrectly) {
    const char* argv[] = {"scout", "--obf-types", "reflection", "native"};
    auto config = cli::ScoutConfig::parse(4, argv);
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->obf_types.size(), 2);
    EXPECT_EQ(config->obf_types[0], "reflection");
    EXPECT_EQ(config->obf_types[1], "native");
}

TEST(CliParserTest, ReturnsNulloptOnHelp) {
    const char* argv[] = {"scout", "--help"};
    // Redireciona stdout temporariamente para não poluir o console do teste
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
    
    auto config = cli::ScoutConfig::parse(2, argv);
    
    std::cout.rdbuf(old);
    
    EXPECT_FALSE(config.has_value());
    EXPECT_NE(buffer.str().find("Uso:"), std::string::npos);
}

TEST(CliParserTest, ReturnsNulloptOnEmptyArgs) {
    const char* argv[] = {"scout"};
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
    
    auto config = cli::ScoutConfig::parse(1, argv);
    
    std::cout.rdbuf(old);
    
    EXPECT_FALSE(config.has_value());
}

TEST(CliParserTest, ThrowsParseErrorOnMissingValue) {
    const char* argv[] = {"scout", "--search"};
    EXPECT_THROW(cli::ScoutConfig::parse(2, argv), cli::ParseError);
}

TEST(CliParserTest, ThrowsParseErrorOnUnknownArg) {
    const char* argv[] = {"scout", "--unknown-flag"};
    EXPECT_THROW(cli::ScoutConfig::parse(2, argv), cli::ParseError);
}
