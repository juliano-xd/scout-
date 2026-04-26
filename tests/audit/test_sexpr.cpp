#include <gtest/gtest.h>
#include "utils/sexpr.hpp"
#include <thread>
#include <vector>

using namespace sexpr;

// 1. Construtores e Predicados
TEST(SExprAudit, ConstructorsAndPredicates) {
    Node n;
    EXPECT_TRUE(n.is_nil());
    EXPECT_FALSE(n.is_list());
    
    Node l = list();
    EXPECT_TRUE(l.is_list());
    EXPECT_FALSE(l.is_nil());
    
    Node s = string("test");
    EXPECT_TRUE(s.is_string());
    
    Node k = keyword("key");
    EXPECT_TRUE(k.is_keyword());
}

// 2. Factories Atômicas
TEST(SExprAudit, AtomicFactories) {
    EXPECT_EQ(boolean(true).to_string(), "true");
    EXPECT_EQ(boolean(false).to_string(), "false");
    
    EXPECT_EQ(integer(123).to_string(), "123");
    EXPECT_EQ(integer(-456).to_string(), "-456");
    EXPECT_EQ(integer(0).to_string(), "0");
    
    // Teste do template integer
    size_t sz = 999;
    EXPECT_EQ(integer(sz).to_string(), "999");
    
    EXPECT_EQ(real(3.14).to_string(), "3.14");
    EXPECT_EQ(symbol("my-sym").to_string(), "my-sym");
}

// 3. Keyword Logic
TEST(SExprAudit, Keywords) {
    EXPECT_EQ(keyword("test").to_string(), ":test");
    EXPECT_EQ(keyword(":already").to_string(), ":already");
    EXPECT_EQ(keyword("").to_string(), ":");
}

// 4. String Escaping (Extreme Cases)
TEST(SExprAudit, StringEscaping) {
    EXPECT_EQ(string("hello").to_string(), "\"hello\"");
    EXPECT_EQ(string("quote \" inside").to_string(), "\"quote \\\" inside\"");
    EXPECT_EQ(string("slash \\ inside").to_string(), "\"slash \\\\ inside\"");
    EXPECT_EQ(string("new\nline").to_string(), "\"new\\nline\"");
    EXPECT_EQ(string("tab\tchar").to_string(), "\"tab\\tchar\"");
    
    // Caracteres de controle (< 32)
    char ctrl[] = {1, 2, 3, 0};
    EXPECT_EQ(string(ctrl).to_string(), "\"\\x01\\x02\\x03\"");
}

// 5. List Manipulation (Push/KV)
TEST(SExprAudit, ListManipulation) {
    Node l = list();
    l.push(integer(1)).push(integer(2));
    EXPECT_EQ(l.to_string(), "(1 2)");
    
    // Erro ao dar push em não-lista
    Node n = integer(10);
    EXPECT_THROW(n.push(integer(1)), std::logic_error);
    
    // KV pairs
    Node f = form("test");
    f.kv("id", integer(101)).kv("name", string("alice"));
    EXPECT_EQ(f.to_string(), "(:test :id 101 :name \"alice\")");
    
    EXPECT_THROW(n.kv("key", integer(1)), std::logic_error);
}

// 6. Pretty Printing (Indentation Check)
TEST(SExprAudit, PrettyPrinting) {
    auto root = form("root");
    root.kv("child", list({integer(1), integer(2)}));
    root.kv("meta", string("data"));
    
    std::string pretty = root.to_string(true);
    // Esperado (2 espaços por nível):
    // (:root
    //   :child
    //   (1
    //     2)
    //   :meta
    //   "data")
    
    EXPECT_TRUE(pretty.find("\n  :child") != std::string::npos);
    EXPECT_TRUE(pretty.find("\n    2") != std::string::npos);
}

// 7. Initializer Lists
TEST(SExprAudit, InitializerLists) {
    Node l = list({integer(1), string("a"), boolean(true)});
    EXPECT_EQ(l.to_string(), "(1 \"a\" true)");
}

// 8. Thread Safety (Metadata helper)
TEST(SExprAudit, CurrentTimestamp) {
    std::string t1 = current_timestamp();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::string t2 = current_timestamp();
    
    // Formato ISO 2026-04-26T...
    EXPECT_EQ(t1.size(), 20); // YYYY-MM-DDTHH:MM:SSZ
    EXPECT_NE(t1, t2);
}

// 9. Standard Status/Error nodes
TEST(SExprAudit, StatusNodes) {
    Node ok = make_status("ok", "everything fine");
    EXPECT_EQ(ok.items[0].to_string(), ":scout:status");
    
    Node err = make_error("critical failure");
    EXPECT_EQ(err.items[0].to_string(), ":scout:error");
    EXPECT_TRUE(err.to_string().find("critical failure") != std::string::npos);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
