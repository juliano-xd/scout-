#include <gtest/gtest.h>
#include "utils/sexpr.hpp"

class SExprMassiveTest : public ::testing::TestWithParam<int> {
};

TEST_P(SExprMassiveTest, DeepNestingAndEscapingTest) {
    int i = GetParam();
    
    auto root = sexpr::form("massive-test");
    root.kv("id", sexpr::integer(i));
    
    // Criar aninhamento profundo baseado em i
    auto current = sexpr::list();
    for(int j=0; j < (i % 20); ++j) {
        auto next = sexpr::list();
        next.push(sexpr::string("level_" + std::to_string(j)));
        current.push(std::move(next));
    }
    root.kv("tree", current);
    
    // Injetar caracteres especiais para testar o escape
    std::string special = "Value with \"quotes\" and \n newlines " + std::to_string(i);
    root.kv("special", sexpr::string(special));

    std::string output = root.to_string();
    
    EXPECT_FALSE(output.empty());
    EXPECT_TRUE(output.find("massive-test") != std::string::npos);
    EXPECT_TRUE(output.find("\\\"quotes\\\"") != std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(MassiveSuite, SExprMassiveTest, ::testing::Range(0, 100));
