#include <gtest/gtest.h>
#include "formatters/sexpr_formatter.hpp"
#include "engines/i_search_engine.hpp"
#include "core/analysis_context.hpp"
#include "engines/register_engines.hpp"
#include "utils/sexpr.hpp"
#include <string>
#include <vector>

using namespace formatters;
using namespace engines;

class SExprEdgeTest : public ::testing::Test {
protected:
    SExprFormatter formatter;
};

// ==========================================
// Parênteses balanceados
// ==========================================

TEST_F(SExprEdgeTest, EmptyResultsBalancedParens) {
    auto out = formatter.format_search_results({});
    int depth = 0;
    for (char c : out) {
        if (c == '(') depth++;
        if (c == ')') depth--;
        EXPECT_GE(depth, 0) << "Parens desbalanceados: " << out;
    }
    EXPECT_EQ(depth, 0) << "Parens não fechados: " << out;
}

TEST_F(SExprEdgeTest, MultipleResultsBalancedParens) {
    std::vector<SearchResult> results;
    for (int i = 1; i <= 5; ++i) {
        SearchResult r;
        r.file_path   = "/file" + std::to_string(i) + ".smali";
        r.line_number = static_cast<std::size_t>(i);
        r.engine_name = "content";
        results.push_back(r);
    }
    auto out = formatter.format_search_results(results);
    EXPECT_NE(out.find(":total 5"), std::string::npos);
    int depth = 0;
    for (char c : out) {
        if (c == '(') depth++;
        if (c == ')') depth--;
    }
    EXPECT_EQ(depth, 0);
}

// ==========================================
// Escape de caracteres especiais
// ==========================================

TEST_F(SExprEdgeTest, QuotesAreEscaped) {
    auto n = sexpr::string("path/with\"quote/file.smali");
    auto out = formatter.format(n);
    EXPECT_NE(out.find("\\\""), std::string::npos) << "Aspas não escapadas: " << out;
}

TEST_F(SExprEdgeTest, BackslashEscaped) {
    auto out = formatter.format(sexpr::string("path\\to\\file"));
    EXPECT_NE(out.find("\\\\"), std::string::npos);
}

TEST_F(SExprEdgeTest, NewlineEscapedInString) {
    auto out = formatter.format(sexpr::string("line\nwith\nnewlines"));
    EXPECT_NE(out.find("\\n"), std::string::npos);
}

TEST_F(SExprEdgeTest, ResultWithQuotesInPath) {
    SearchResult r;
    r.file_path    = "/path/with\"quote/File.smali";
    r.line_content = "const-string v0, \"test\"";
    r.engine_name  = "content";
    r.line_number  = 1;
    auto out = formatter.format_search_results({r});
    EXPECT_NE(out.find("\\\""), std::string::npos);
    int depth = 0;
    for (char c : out) {
        if (c == '(') depth++;
        if (c == ')') depth--;
    }
    EXPECT_EQ(depth, 0);
}

// ==========================================
// Primitivos via format()
// ==========================================

TEST_F(SExprEdgeTest, NilNode)       { EXPECT_EQ(formatter.format(sexpr::nil()),           "nil"); }
TEST_F(SExprEdgeTest, BoolTrue)      { EXPECT_EQ(formatter.format(sexpr::boolean(true)),   "true"); }
TEST_F(SExprEdgeTest, BoolFalse)     { EXPECT_EQ(formatter.format(sexpr::boolean(false)),  "false"); }
TEST_F(SExprEdgeTest, IntegerNode)   { EXPECT_EQ(formatter.format(sexpr::integer(42)),     "42"); }
TEST_F(SExprEdgeTest, NegativeInt)   { EXPECT_EQ(formatter.format(sexpr::integer(-7)),     "-7"); }
TEST_F(SExprEdgeTest, StringNode)    { EXPECT_EQ(formatter.format(sexpr::string("hello")), "\"hello\""); }
TEST_F(SExprEdgeTest, EmptyList)     { EXPECT_EQ(formatter.format(sexpr::list()),          "()"); }
TEST_F(SExprEdgeTest, KeywordNode)   { EXPECT_EQ(formatter.format(sexpr::keyword("foo")),  ":foo"); }

// ==========================================
// Listas aninhadas
// ==========================================

TEST_F(SExprEdgeTest, NestedList) {
    auto inner = sexpr::list({sexpr::integer(3), sexpr::integer(4)});
    auto outer = sexpr::list({sexpr::integer(1), sexpr::integer(2), inner});
    auto out = formatter.format(outer);
    EXPECT_NE(out.find("1"), std::string::npos);
    EXPECT_NE(out.find("3"), std::string::npos);
    int depth = 0;
    for (char c : out) {
        if (c == '(') depth++;
        if (c == ')') depth--;
    }
    EXPECT_EQ(depth, 0);
}

// ==========================================
// Status e form
// ==========================================

TEST_F(SExprEdgeTest, StatusSuccess) {
    auto n = sexpr::form("scout:status");
    n.kv("status", sexpr::string("success"));
    auto out = formatter.format(n);
    EXPECT_NE(out.find(":scout:status"), std::string::npos);
    EXPECT_NE(out.find("success"),       std::string::npos);
}

TEST_F(SExprEdgeTest, ErrorNode) {
    auto n = sexpr::make_error("Class not found");
    auto out = formatter.format(n);
    EXPECT_NE(out.find("error"),          std::string::npos);
    EXPECT_NE(out.find("Class not found"), std::string::npos);
}

TEST_F(SExprEdgeTest, FormKvPairs) {
    auto n = sexpr::form("result");
    n.kv("a", sexpr::integer(1)).kv("b", sexpr::string("two"));
    auto out = formatter.format(n);
    EXPECT_NE(out.find(":a 1"),      std::string::npos);
    EXPECT_NE(out.find(":b \"two\""), std::string::npos);
}

// ==========================================
// Opções de formatação via sexpr::Node
// ==========================================

TEST_F(SExprEdgeTest, PrettyLargerThanCompact) {
    SearchResult r;
    r.file_path   = "/f.smali";
    r.engine_name = "content";
    r.line_number = 1;

    auto pretty_opts  = sexpr::list({sexpr::keyword("pretty"),  sexpr::boolean(true)});
    auto compact_opts = sexpr::list({sexpr::keyword("compact"), sexpr::boolean(true)});

    auto pretty  = formatter.format_search_results({r}, pretty_opts);
    auto compact = formatter.format_search_results({r}, compact_opts);
    EXPECT_LT(compact.size(), pretty.size());
}

TEST_F(SExprEdgeTest, MetadataDefault) {
    auto out = formatter.format_search_results({});
    EXPECT_NE(out.find(":timestamp"), std::string::npos);
    EXPECT_NE(out.find(":version"),   std::string::npos);
}

TEST_F(SExprEdgeTest, MetadataDisabled) {
    auto opts = sexpr::list({sexpr::keyword("metadata"), sexpr::boolean(false)});
    auto out  = formatter.format_search_results({}, opts);
    EXPECT_EQ(out.find(":timestamp"), std::string::npos);
    EXPECT_EQ(out.find(":version"),   std::string::npos);
    EXPECT_NE(out.find(":total"),     std::string::npos);
}

// ==========================================
// Opções suportadas
// ==========================================

TEST_F(SExprEdgeTest, SupportsKnownOptions) {
    EXPECT_TRUE(formatter.supports_option("compact"));
    EXPECT_TRUE(formatter.supports_option("pretty"));
    EXPECT_TRUE(formatter.supports_option("metadata"));
}

TEST_F(SExprEdgeTest, DoesNotSupportUnknownOptions) {
    EXPECT_FALSE(formatter.supports_option("colors"));
    EXPECT_FALSE(formatter.supports_option("json"));
    EXPECT_FALSE(formatter.supports_option(""));
}

// ==========================================
// XREF
// ==========================================

TEST_F(SExprEdgeTest, XrefResultsHeader) {
    SearchResult r;
    r.file_path   = "/A.smali";
    r.engine_name = "xref";
    r.context     = "Lcom/A;->m() [caller]";
    auto out = formatter.format_xref_results({r});
    EXPECT_NE(out.find("scout:xref"), std::string::npos);
    EXPECT_NE(out.find(":total 1"),   std::string::npos);
}

// ==========================================
// Testes de Robustez e Casos Irreais
// ==========================================

TEST_F(SExprEdgeTest, DeepNestingStackOverflow) {
    sexpr::Node root = sexpr::list();
    sexpr::Node* current = &root;
    for (int i = 0; i < 1000; ++i) {
        current->push(sexpr::list());
        current = &current->items.back();
    }
    current->push(sexpr::string("deep"));
    
    // Deve serializar sem crashar
    EXPECT_NO_THROW({
        std::string out = root.to_string();
        EXPECT_FALSE(out.empty());
    });
}

TEST_F(SExprEdgeTest, NonListPushThrows) {
    auto n = sexpr::integer(10);
    EXPECT_THROW(n.push(sexpr::nil()), std::logic_error);
}

TEST_F(SExprEdgeTest, NonListKvThrows) {
    auto n = sexpr::string("not a list");
    EXPECT_THROW(n.kv("key", sexpr::boolean(true)), std::logic_error);
}

TEST_F(SExprEdgeTest, MalformedOptsFrom) {
    // Lista com número ímpar de elementos (chave sem valor)
    auto bad_opts = sexpr::list({sexpr::keyword("pretty")});
    auto opts = SExprFormatter::Opts::from(bad_opts);
    // Deve ignorar o item ímpar e usar defaults
    EXPECT_FALSE(opts.pretty);
}

TEST_F(SExprEdgeTest, MixedTypeOptsFrom) {
    // Chave correta, mas valor de tipo errado
    auto bad_opts = sexpr::list({sexpr::keyword("pretty"), sexpr::integer(123)});
    auto opts = SExprFormatter::Opts::from(bad_opts);
    EXPECT_FALSE(opts.pretty);
}

TEST_F(SExprEdgeTest, NonPrintableCharacters) {
    std::string binary_data;
    for (int i = 0; i < 32; ++i) binary_data += static_cast<char>(i);
    auto n = sexpr::string(binary_data);
    auto out = formatter.format(n);
    // Deve conter escapes hexadecimais como \x01, \x02 etc.
    EXPECT_NE(out.find("\\x01"), std::string::npos);
    EXPECT_NE(out.find("\\x1f"), std::string::npos);
}

TEST_F(SExprEdgeTest, HugeIntegers) {
    long long huge = 9223372036854775807LL; // LLONG_MAX
    auto n = sexpr::integer(huge);
    EXPECT_EQ(formatter.format(n), std::to_string(huge));
}

TEST_F(SExprEdgeTest, HugeFloats) {
    double huge = 1e308;
    auto n = sexpr::real(huge);
    auto out = formatter.format(n);
    EXPECT_FALSE(out.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    scout::register_all_components();
    return RUN_ALL_TESTS();
}
