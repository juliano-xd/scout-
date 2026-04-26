#include <gtest/gtest.h>
#include "cli/parser.hpp"

using namespace cli;

TEST(CLIParserAudit, BasicParsing) {
    bool flag = false;
    std::string opt = "";
    int val = 0;
    std::vector<std::string> list;
    
    Parser parser("test", "epilog");
    parser.add_flag("--flag", "-f", "help", "en", flag);
    parser.add_option("--opt", "-o", "STR", "help", "en", opt);
    parser.add_option("--val", "-v", "INT", "help", "en", val);
    parser.add_list("--list", "-l", "LIST", "help", "en", list);
    
    const char* argv[] = {"prog", "-f", "--opt", "hello", "--val", "123", "--list", "a", "b", "c"};
    parser.parse(10, argv);
    
    EXPECT_TRUE(flag);
    EXPECT_EQ(opt, "hello");
    EXPECT_EQ(val, 123);
    ASSERT_EQ(list.size(), 3);
    EXPECT_EQ(list[0], "a");
    EXPECT_EQ(list[2], "c");
}

TEST(CLIParserAudit, Errors) {
    int val = 0;
    Parser parser("test", "epilog");
    parser.add_option("--val", "-v", "INT", "help", "en", val);
    
    const char* argv1[] = {"prog", "--val", "abc"};
    EXPECT_THROW(parser.parse(3, argv1), ParseError);
    
    const char* argv2[] = {"prog", "--val"};
    EXPECT_THROW(parser.parse(2, argv2), ParseError);
    
    const char* argv3[] = {"prog", "--unknown"};
    EXPECT_THROW(parser.parse(2, argv3), ParseError);
}

TEST(CLIParserAudit, AIHelp) {
    Parser parser("test", "epilog");
    parser.add_flag("--flag", "-f", "help", "en", *(new bool(false))); // leak for test
    
    std::stringstream ss;
    parser.print_ai_help(ss);
    std::string out = ss.str();
    EXPECT_TRUE(out.find("scout:ai-documentation") != std::string::npos);
    EXPECT_TRUE(out.find("--flag") != std::string::npos);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
