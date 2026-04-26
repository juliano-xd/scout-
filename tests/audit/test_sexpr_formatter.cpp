#include <gtest/gtest.h>
#include "formatters/sexpr_formatter.hpp"

using namespace formatters;

TEST(SExprFormatterAudit, OptionsParsing) {
    auto n = sexpr::list();
    n.push(sexpr::keyword("pretty"));
    n.push(sexpr::boolean(true));
    n.push(sexpr::keyword("metadata"));
    n.push(sexpr::boolean(false));
    
    auto opts = SExprFormatter::Opts::from(n);
    EXPECT_TRUE(opts.pretty);
    EXPECT_FALSE(opts.metadata);
}

TEST(SExprFormatterAudit, FormatResults) {
    SExprFormatter fmt;
    std::vector<engines::SearchResult> results;
    
    engines::SearchResult r1;
    r1.engine_name = "test";
    r1.file_path = "path/to/class";
    r1.line_number = 10;
    r1.line_content = "code line";
    r1.context = "full context";
    results.push_back(r1);
    
    // Test without options
    std::string out = fmt.format_search_results(results);
    EXPECT_TRUE(out.find("scout:search") != std::string::npos);
    EXPECT_TRUE(out.find("code line") != std::string::npos);
    
    // Test with metadata option
    auto n = sexpr::list();
    n.push(sexpr::keyword("metadata"));
    n.push(sexpr::boolean(true));
    std::string out_meta = fmt.format_search_results(results, n);
    EXPECT_TRUE(out_meta.find(":version") != std::string::npos);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
