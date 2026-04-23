#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <chrono>
#include <ctime>

/**
 * @brief Módulo nativo de S-Expression para o Scout++.
 *
 * O programa opera *nativamente* neste formato — não há conversão de JSON → SExpr
 * no pipeline de saída. JSON é aceito apenas como formato de ENTRADA (ex: leitura
 * de AndroidManifest.xml), nunca como estrutura interna de dados de saída.
 *
 * Uso básico:
 *   auto r = sexpr::form("scout:results")
 *               .kv("status",  sexpr::string("success"))
 *               .kv("total",   sexpr::integer(3))
 *               .build();
 *   std::cout << r.to_string() << "\n";
 *   // (:scout:results :status "success" :total 3)
 */
namespace sexpr {

// ==========================================
// Tipo Nativo: Node
// ==========================================

struct Node {
    enum class Kind { Nil, Bool, Int, Float, String, Symbol, Keyword, List };

    Kind kind = Kind::Nil;
    std::string str_val;
    long long   int_val   = 0;
    double      float_val = 0.0;
    bool        bool_val  = false;
    std::vector<Node> items; // válido apenas para Kind::List

    Node() : kind(Kind::Nil) {}

    bool is_nil()     const noexcept { return kind == Kind::Nil; }
    bool is_list()    const noexcept { return kind == Kind::List; }
    bool is_string()  const noexcept { return kind == Kind::String; }
    bool is_keyword() const noexcept { return kind == Kind::Keyword; }

    // Adiciona um nó ao final de uma lista
    Node& push(Node n) {
        if (kind != Kind::List)
            throw std::logic_error("sexpr::Node::push() em nó que não é lista");
        items.push_back(std::move(n));
        return *this;
    }

    // Adiciona par :keyword valor à lista
    Node& kv(std::string_view key, Node val);

    // Serializa para string S-Expression
    std::string to_string(bool pretty = false, int depth = 0) const;
};

// ==========================================
// Factories
// ==========================================

inline Node nil() { return Node{}; }

inline Node boolean(bool b) {
    Node n; n.kind = Node::Kind::Bool; n.bool_val = b; return n;
}

inline Node integer(long long v) {
    Node n; n.kind = Node::Kind::Int; n.int_val = v; return n;
}

// Aceita qualquer inteiro (int, size_t, etc.) via conversão implícita para long long
template<typename T>
    requires std::is_integral_v<T>
inline Node integer(T v) {
    Node n; n.kind = Node::Kind::Int; n.int_val = static_cast<long long>(v); return n;
}

inline Node real(double v) {
    Node n; n.kind = Node::Kind::Float; n.float_val = v; return n;
}

inline Node string(std::string_view s) {
    Node n; n.kind = Node::Kind::String; n.str_val = std::string(s); return n;
}

inline Node symbol(std::string_view s) {
    Node n; n.kind = Node::Kind::Symbol; n.str_val = std::string(s); return n;
}

// Cria um keyword: :nome  (strip ':' se já presente)
inline Node keyword(std::string_view s) {
    Node n; n.kind = Node::Kind::Keyword;
    n.str_val = s.starts_with(':') ? std::string(s.substr(1)) : std::string(s);
    return n;
}

// Lista vazia ou com filhos via vector
inline Node list(std::vector<Node> children = {}) {
    Node n; n.kind = Node::Kind::List; n.items = std::move(children); return n;
}

// Lista a partir de initializer_list
inline Node list(std::initializer_list<Node> children) {
    Node n; n.kind = Node::Kind::List;
    n.items.reserve(children.size());
    for (auto& c : children) n.items.push_back(c);
    return n;
}

// Abre uma lista com keyword inicial: (:nome ...)
inline Node form(std::string_view name) {
    Node n; n.kind = Node::Kind::List;
    n.items.push_back(keyword(name));
    return n;
}

// ==========================================
// Implementações inline de Node
// ==========================================

inline Node& Node::kv(std::string_view key, Node val) {
    if (kind != Kind::List)
        throw std::logic_error("sexpr::Node::kv() em nó que não é lista");
    items.push_back(keyword(key));
    items.push_back(std::move(val));
    return *this;
}

inline std::string escape_string(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 2);
    r += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:
                if (c < 32) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\x%02x", (unsigned)c);
                    r += buf;
                } else {
                    r += static_cast<char>(c);
                }
        }
    }
    r += '"';
    return r;
}

inline std::string Node::to_string(bool pretty, int depth) const {
    const int W = 2;
    std::string ci(static_cast<std::size_t>((depth + 1) * W), ' ');
    std::string oi(static_cast<std::size_t>(depth * W), ' ');

    switch (kind) {
        case Kind::Nil:     return "nil";
        case Kind::Bool:    return bool_val ? "true" : "false";
        case Kind::Int:     return std::to_string(int_val);
        case Kind::Float: {
            std::ostringstream ss;
            ss << std::setprecision(6) << float_val;
            return ss.str();
        }
        case Kind::String:  return escape_string(str_val);
        case Kind::Symbol:  return str_val;
        case Kind::Keyword: return ":" + str_val;
        case Kind::List: {
            if (items.empty()) return "()";
            std::string r = "(";
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (i > 0) r += pretty ? "\n" + ci : " ";
                r += items[i].to_string(pretty, depth + 1);
            }
            if (pretty && items.size() > 1) r += "\n" + oi;
            r += ")";
            return r;
        }
    }
    return "nil";
}

// ==========================================
// Utilitários
// ==========================================

/// Timestamp ISO-8601 UTC atual (para metadados)
inline std::string current_timestamp() {
    auto now    = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

/// Cria nó de status padrão
inline Node make_status(std::string_view status, std::string_view msg = "") {
    auto n = form("scout:status");
    n.kv("status", string(status));
    if (!msg.empty()) n.kv("message", string(msg));
    return n;
}

/// Cria nó de erro
inline Node make_error(std::string_view msg) {
    auto n = form("scout:error");
    n.kv("status",  string("error"));
    n.kv("message", string(msg));
    return n;
}

} // namespace sexpr
