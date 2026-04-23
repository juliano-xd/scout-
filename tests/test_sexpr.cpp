#include <gtest/gtest.h>
#include "formatters/sexpr_formatter.hpp"
#include "engines/i_search_engine.hpp"
#include "engines/register_engines.hpp"
#include "utils/sexpr.hpp"
#include <filesystem>
#include <vector>
#include <string>

using namespace formatters;
using namespace engines;

// ==========================================
// Testes básicos do SExprFormatter (API nativa sexpr)
// ==========================================

TEST(SExprFormatterTest, NameAndExtension) {
    SExprFormatter fmt;
    EXPECT_EQ(fmt.name(), "sexpr");
    EXPECT_EQ(fmt.default_extension(), ".sexpr");
}

TEST(SExprFormatterTest, FormatSearchResults) {
    SExprFormatter fmt;
    SearchResult r;
    r.file_path    = std::filesystem::path("/smali/com/example/A.smali");
    r.line_number  = 42;
    r.line_content = "invoke-virtual {p0}, Lcom/example/A;->doLogin()V";
    r.context      = "method:doLogin";
    r.engine_name  = "content";

    auto out = fmt.format_search_results({r});
    EXPECT_NE(out.find(":total 1"), std::string::npos);
    EXPECT_NE(out.find("com/example/A.smali"), std::string::npos);
    EXPECT_NE(out.find("doLogin"), std::string::npos);
}

TEST(SExprFormatterTest, FormatStatus) {
    SExprFormatter fmt;
    auto node = sexpr::form("scout:status");
    node.kv("status", sexpr::string("success"));

    auto out = fmt.format(node);
    EXPECT_NE(out.find(":scout:status"), std::string::npos);
    EXPECT_NE(out.find("success"), std::string::npos);
}

TEST(SExprFormatterTest, FormatRawNode) {
    SExprFormatter fmt;
    EXPECT_EQ(fmt.format(sexpr::nil()),          "nil");
    EXPECT_EQ(fmt.format(sexpr::boolean(true)),  "true");
    EXPECT_EQ(fmt.format(sexpr::boolean(false)), "false");
    EXPECT_EQ(fmt.format(sexpr::integer(42)),    "42");
    EXPECT_EQ(fmt.format(sexpr::string("hi")),   "\"hi\"");
}

TEST(SExprFormatterTest, FormatList) {
    SExprFormatter fmt;
    auto node = sexpr::list({
        sexpr::keyword("a"),
        sexpr::integer(1),
        sexpr::keyword("b"),
        sexpr::string("two")
    });
    auto out = fmt.format(node);
    EXPECT_EQ(out, "(:a 1 :b \"two\")");
}

TEST(SExprFormatterTest, PrettyMode) {
    SExprFormatter fmt;
    SearchResult r;
    r.file_path   = "/f.smali";
    r.engine_name = "content";

    auto opts = sexpr::list({sexpr::keyword("pretty"), sexpr::boolean(true)});
    auto compact = fmt.format_search_results({r});
    auto pretty  = fmt.format_search_results({r}, opts);

    EXPECT_LT(compact.size(), pretty.size());
}

TEST(SExprFormatterTest, MetadataOnByDefault) {
    SExprFormatter fmt;
    auto out = fmt.format_search_results({});
    EXPECT_NE(out.find(":timestamp"), std::string::npos);
    EXPECT_NE(out.find(":version"),   std::string::npos);
}

TEST(SExprFormatterTest, MetadataCanBeDisabled) {
    SExprFormatter fmt;
    auto opts = sexpr::list({sexpr::keyword("metadata"), sexpr::boolean(false)});
    auto out = fmt.format_search_results({}, opts);
    EXPECT_EQ(out.find(":timestamp"), std::string::npos);
    EXPECT_EQ(out.find(":version"),   std::string::npos);
}

TEST(SExprFormatterTest, XrefResults) {
    SExprFormatter fmt;
    SearchResult r;
    r.file_path   = "/com/example/A.smali";
    r.context     = "Lcom/example/A;->onCreate() [caller]";
    r.engine_name = "xref";

    auto out = fmt.format_xref_results({r});
    EXPECT_NE(out.find("scout:xref"), std::string::npos);
    EXPECT_NE(out.find(":total 1"),   std::string::npos);
}

TEST(SExprFormatterTest, EmptyResultsValidSExpr) {
    SExprFormatter fmt;
    auto out = fmt.format_search_results({});
    EXPECT_NE(out.find(":total 0"), std::string::npos);
    int depth = 0;
    for (char c : out) {
        if (c == '(') depth++;
        if (c == ')') depth--;
        EXPECT_GE(depth, 0);
    }
    EXPECT_EQ(depth, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    scout::register_all_components();
    return RUN_ALL_TESTS();
}
