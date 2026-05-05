#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <format>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cctype>

/**
 * @file string_utils.hpp
 * @brief Utilitários de manipulação de texto, criptografia e parsing para o Scout++.
 * @details Implementações zero-allocation (onde aplicável) e time-complexity otimizado (C++26).
 */
namespace utils {

    /**
     * @brief Remove whitespace das extremidades numa complexidade O(N) sem alocar memória.
     */
    [[nodiscard]] constexpr std::string_view trim(std::string_view s) noexcept {
        constexpr std::string_view whitespace = " \t\r\n";
        const auto first = s.find_first_not_of(whitespace);
        if (first == std::string_view::npos) return {};
        const auto last = s.find_last_not_of(whitespace);
        return s.substr(first, (last - first + 1));
    }

    /**
     * @brief Formatação Hexadecimal Fast-Path.
     * @details std::format_to nativo elimina o overhead crônico do std::stringstream.
     */
    [[nodiscard]] inline std::string to_hex(uint32_t val) {
        return std::format("0x{:08x}", val);
    }

    /**
     * @brief Validação Base64.
     */
    [[nodiscard]] constexpr bool is_base64(std::string_view s) noexcept {
        if (s.length() < 12) return false;
        for (const char c : s) {
            // UB mitigation: static_cast para unsigned char é obrigatório no isalnum
            const auto uc = static_cast<unsigned char>(c);
            if (!std::isalnum(uc) && c != '+' && c != '/' && c != '=') [[unlikely]] return false;
        }
        return true;
    }

    namespace detail {
        // Geração da Tabela de Lookup Base64 em tempo de compilação.
        // Evita a alocação de `std::vector` repetida a cada invocação do decode_base64.
        [[nodiscard]] constexpr auto build_b64_table() noexcept {
            std::array<int8_t, 256> table{};
            table.fill(-1);
            constexpr std::string_view b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            for (size_t i = 0; i < b64.size(); ++i) {
                table[static_cast<uint8_t>(b64[i])] = static_cast<int8_t>(i);
            }
            return table;
        }
        constexpr auto b64_lookup = build_b64_table();
    }

    /**
     * @brief Descodificador Base64 iterativo com Lookup Table O(1).
     */
    [[nodiscard]] inline std::string decode_base64(std::string_view in) {
        std::string out;
        out.reserve((in.size() * 3) / 4); // Pre-alloc heurístico mitigando realocações

        int val = 0;
        int valb = -8;

        for (const unsigned char c : in) {
            const int8_t tbl_val = detail::b64_lookup[c];
            if (tbl_val == -1) break; // Padding ou char inválido detectado

            val = (val << 6) + tbl_val;
            valb += 6;

            if (valb >= 0) {
                out.push_back(static_cast<char>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return out;
    }

    /**
     * @brief Computação da Entropia de Shannon (O(N) CPU, O(1) Memória).
     * @details Migrado de std::map (Red-Black tree) para std::array continuo,
     * evitando page-faults e latência de pointer-chasing no heap.
     */
    [[nodiscard]] inline double calculate_entropy(std::string_view s) noexcept {
        if (s.empty()) [[unlikely]] return 0.0;

        std::array<uint32_t, 256> freq{};
        for (const unsigned char c : s) {
            freq[c]++;
        }

        double entropy = 0.0;
        const double len = static_cast<double>(s.length());

        for (const uint32_t f : freq) {
            if (f > 0) {
                const double p = f / len;
                entropy -= p * std::log2(p);
            }
        }
        return entropy;
    }

    /**
     * @brief Busca de substrings Case-Insensitive O(N*M).
     * @details Utiliza C++20 Ranges.
     */
    [[nodiscard]] inline bool contains_insensitive(std::string_view haystack, std::string_view needle) noexcept {
        if (needle.empty()) return true;

        auto it = std::ranges::search(
            haystack, needle,
            [](unsigned char ch1, unsigned char ch2) {
                return std::tolower(ch1) == std::tolower(ch2);
            }
        );
        return !it.empty();
    }

    /**
     * @brief Iterador Non-Allocating para processar textos multiline sem instanciar std::string.
     */
    struct LineIterator {
        std::string_view data;
        size_t pos = 0;

        constexpr explicit LineIterator(std::string_view d) noexcept : data(d) {}

        constexpr bool next(std::string_view& line) noexcept {
            if (pos >= data.size()) return false;

            const size_t end = data.find('\n', pos);
            if (end == std::string_view::npos) {
                line = data.substr(pos);
                pos = data.size();
            } else {
                line = data.substr(pos, end - pos);
                pos = end + 1;
            }

            // Suporte para quebras CRLF do Windows
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            return true;
        }
    };

    /**
     * @brief Extrai blocos `.method` de ficheiros Smali de Android.
     * @param path Localização do ficheiro.
     * @param method_sig Assinatura alvo (Ex: "onCreate(Landroid/os/Bundle;)V").
     */
    [[nodiscard]] inline std::vector<std::string> extract_smali_method(
        const std::filesystem::path& path,
        std::string_view method_sig)
    {
        std::ifstream file(path);
        if (!file.is_open()) [[unlikely]] return {};

        std::string line;
        std::vector<std::string> body;
        body.reserve(128); // Minimiza amortização de vetores pequenos

        bool in_method = false;

        // std::getline reusa a capacidade interna de `line`, mantendo as alocações reduzidas.
        while (std::getline(file, line)) {
            const std::string_view trimmed = trim(line);

            if (trimmed.starts_with(".method ") && trimmed.find(method_sig) != std::string_view::npos) {
                in_method = true;
                continue;
            }

            if (in_method) {
                if (trimmed.starts_with(".end method")) break;
                body.push_back(line);
            }
        }

        return body;
    }

} // namespace utils