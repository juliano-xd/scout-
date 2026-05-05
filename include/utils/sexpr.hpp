#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <format>
#include <concepts>
#include <initializer_list>
#include <utility>
#include <iterator>

/**
 * @file sexpr.hpp
 * @brief AST nativa e motor de serialização S-Expression para o Scout++.
 * @details Arquitetura otimizada para C++26. Utiliza avaliação transient constexpr,
 * Concepts para SFINAE e a biblioteca <format> para evitar locks e alocações excessivas na heap.
 */
namespace sexpr {

// ==========================================
// S-Expression AST Node
// ==========================================

struct Node {
    /**
     * @brief Tipo base restrito a uint8_t para forçar alinhamento de 1 byte.
     * @details Em arquiteturas de 64 bits, isso reduz o tamanho total do struct `Node` ao
     * minimizar o padding gerado pelo compilador antes dos ponteiros do `std::string` e `std::vector`.
     */
    enum class Kind : uint8_t { Nil, Bool, Int, Float, String, Symbol, Keyword, List };

    Kind kind = Kind::Nil;
    bool bool_val = false;
    long long int_val = 0;
    double float_val = 0.0;
    std::string str_val;
    std::vector<Node> items;

    /**
     * @brief Construtor padrão constexpr.
     * @details C++20 introduziu suporte a alocações dinâmicas transitórias. Isso permite que
     * instâncias de `std::string` e `std::vector` dentro de `Node` sejam computadas em
     * tempo de compilação (compile-time AST generation).
     */
    constexpr Node() noexcept = default;

    [[nodiscard]] constexpr bool is_nil()     const noexcept { return kind == Kind::Nil; }
    [[nodiscard]] constexpr bool is_list()    const noexcept { return kind == Kind::List; }
    [[nodiscard]] constexpr bool is_string()  const noexcept { return kind == Kind::String; }
    [[nodiscard]] constexpr bool is_keyword() const noexcept { return kind == Kind::Keyword; }

    constexpr Node& push(Node n) {
        if (kind != Kind::List) [[unlikely]]
            throw std::logic_error("sexpr::Node::push() requer Kind::List");
        items.push_back(std::move(n));
        return *this;
    }

    constexpr Node& kv(std::string_view key, Node val);

    [[nodiscard]] std::string to_string(bool pretty = false, int depth = 0) const;

private:
    /**
     * @brief Motor de serialização O(N).
     * @param out Buffer mutável passado por referência para prevenir fragmentação da heap
     * e realocações O(N^2) causadas por cópias de string em retornos recursivos.
     */
    void to_string_impl(std::string& out, bool pretty, int depth) const;
};

// ==========================================
// Zero-Cost Abstraction Factories
// ==========================================

constexpr Node nil() noexcept { return Node{}; }

constexpr Node boolean(bool b) {
    Node n; n.kind = Node::Kind::Bool; n.bool_val = b; return n;
}

/**
 * @brief Construtor de nós inteiros com restrição estrita de tipo.
 * @details Utiliza o Concept `std::integral` do C++20 no lugar de `std::enable_if_t`,
 * acelerando a resolução de sobrecargas e prevenindo coerção implícita de floats/ponteiros.
 */
template<std::integral T>
constexpr Node integer(T v) {
    Node n; n.kind = Node::Kind::Int; n.int_val = static_cast<long long>(v); return n;
}

constexpr Node real(double v) {
    Node n; n.kind = Node::Kind::Float; n.float_val = v; return n;
}

constexpr Node string(std::string_view s) {
    Node n; n.kind = Node::Kind::String; n.str_val = std::string(s); return n;
}

constexpr Node symbol(std::string_view s) {
    Node n; n.kind = Node::Kind::Symbol; n.str_val = std::string(s); return n;
}

constexpr Node keyword(std::string_view s) {
    Node n; n.kind = Node::Kind::Keyword;
    if (!s.empty() && s.front() == ':') {
        s.remove_prefix(1); // Modificação O(1) do view, não causa mutação de memória
    }
    n.str_val = std::string(s);
    return n;
}

constexpr Node list(std::vector<Node> children = {}) {
    Node n; n.kind = Node::Kind::List; n.items = std::move(children); return n;
}

constexpr Node list(std::initializer_list<Node> children) {
    Node n; n.kind = Node::Kind::List;
    n.items.reserve(children.size()); // Pre-alloc para evitar amortização de crescimento do vetor
    for (auto& c : children) n.items.push_back(std::move(c));
    return n;
}

constexpr Node form(std::string_view name) {
    Node n; n.kind = Node::Kind::List;
    n.items.push_back(keyword(name));
    return n;
}

// ==========================================
// Implementações da Serialização
// ==========================================

constexpr Node& Node::kv(std::string_view key, Node val) {
    if (kind != Kind::List) [[unlikely]]
        throw std::logic_error("sexpr::Node::kv() requer Kind::List");
    items.push_back(keyword(key));
    items.push_back(std::move(val));
    return *this;
}

inline std::string escape_string(std::string_view s) {
    std::string r;
    r.reserve(s.size() + 2); // Capacidade base garantida
    r += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:
                if (c < 32) [[unlikely]] {
                    // C++20: Formatação type-safe direto no buffer do iterador
                    // Elimina as vulnerabilidades e o overhead do antigo `snprintf`
                    std::format_to(std::back_inserter(r), "\\x{:02x}", c);
                } else {
                    r += static_cast<char>(c);
                }
        }
    }
    r += '"';
    return r;
}

constexpr bool needs_symbol_escape(std::string_view s) {
    if (s.empty()) return true;
    constexpr std::string_view invalid_chars = " ()\":\\|";
    for (char c : s) {
        if (invalid_chars.find(c) != std::string_view::npos || static_cast<unsigned char>(c) < 32)
            return true;
    }
    return false;
}

inline std::string escape_symbol(std::string_view s) {
    if (!needs_symbol_escape(s))
        return std::string(s);
    std::string r;
    r.reserve(s.size() + 4);
    r += '|';
    for (char c : s) {
        if (c == '|' || c == '\\') r += '\\';
        r += c;
    }
    r += '|';
    return r;
}

inline void Node::to_string_impl(std::string& out, bool pretty, int depth) const {
    constexpr int W = 2;

    switch (kind) {
        case Kind::Nil:     out += "nil"; break;
        case Kind::Bool:    out += (bool_val ? "true" : "false"); break;

        // Delegação de formatação numérica para o motor `std::to_chars` subjacente via `format_to`.
        // Impede degeneração para notação científica imprevista associada a ostringstream.
        case Kind::Int:     std::format_to(std::back_inserter(out), "{}", int_val); break;
        case Kind::Float:   std::format_to(std::back_inserter(out), "{}", float_val); break;

        case Kind::String:  out += escape_string(str_val); break;
        case Kind::Symbol:  out += escape_symbol(str_val); break;
        case Kind::Keyword: out += ':'; out += escape_symbol(str_val); break;

        case Kind::List: {
            if (items.empty()) {
                out += "()";
                break;
            }
            out += '(';

            std::string ci;
            if (pretty) ci = std::string((depth + 1) * W, ' ');

            for (std::size_t i = 0; i < items.size(); ++i) {
                if (i > 0) {
                    if (pretty) { out += '\n'; out += ci; }
                    else { out += ' '; }
                }
                items[i].to_string_impl(out, pretty, depth + 1);
            }

            if (pretty && items.size() > 1) {
                out += '\n';
                // Mutação in-place O(M) ao invés de alocação temporal de string
                out.append(depth * W, ' ');
            }
            out += ')';
            break;
        }
    }
}

inline std::string Node::to_string(bool pretty, int depth) const {
    std::string r;
    r.reserve(256); // Capacidade heurística para evitar realocação no early-stage do AST
    to_string_impl(r, pretty, depth);
    return r;
}

// ==========================================
// Subsistemas Auxiliares
// ==========================================

/**
 * @brief Geração de Timestamp ISO-8601 UTC.
 * @details Utiliza a infraestrutura de chrono de C++20. Lock-free (thread-safe inerente),
 * contornando a contenção de mutexes que ocorria ao utilizar ctime/gmtime em cenários multi-thread.
 */
inline std::string current_timestamp() {
    return std::format("{:%Y-%m-%dT%H:%M:%SZ}",
        std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()));
}

inline Node make_status(std::string_view status, std::string_view msg = "") {
    auto n = form("scout:status");
    n.kv("status", string(status));
    if (!msg.empty()) n.kv("message", string(msg));
    return n;
}

inline Node make_error(std::string_view msg) {
    auto n = form("scout:error");
    n.kv("status",  string("error"));
    n.kv("message", string(msg));
    return n;
}

} // namespace sexpr
