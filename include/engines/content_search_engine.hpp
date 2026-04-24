#pragma once

#include "i_search_engine.hpp"
#include "core/analysis_context.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <regex>
#include <memory>
#include <fstream>
#include <mutex>
#include <execution>

namespace engines {

    /**
     * @brief Motor de busca especializado para conteúdo interno de arquivos Smali.
     * 
     * Suporta múltiplos modos de busca:
     * - "string": busca literal por texto (com opção case_sensitive)
     * - "regex": busca por expressão regular (PCRE-like)
     * - "integer": busca por valores inteiros/hexadecimais
     * 
     * Otimizações:
     * - Memory-mapped files para zero-copy
     * - Paralelização agressiva com std::execution::par_unseq
     * - Early termination se max_results for atingido
     * - Contexto: extrai nome do método atual para cada match
     */
    class ContentSearchEngine : public ISearchEngine {
    public:
        ContentSearchEngine() = default;
        ~ContentSearchEngine() override = default;

        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "content"; }

        std::string description() const override {
            return "Busca especializada por conteúdo em arquivos Smali. Suporta string literal, regex e busca por inteiros.";
        }

        EngineStats get_stats() const override { return stats_; }

        bool supports_config(const SearchConfig& config) const override {
            // Motor de conteúdo suporta todas as configurações básicas
            return true;
        }

    private:
        EngineStats stats_;

        /**
         * @brief Estrutura interna para rastrear contexto durante parsing de arquivo.
         */
        struct ParseContext {
            std::string current_class;
            std::string current_method;
            size_t line_number = 0;
        };

        /**
         * @brief Processa uma linha Smali extraindo contexto.
         */
        static void update_context(std::string_view line, ParseContext& ctx);

        /**
         * @brief Verifica se a linha contém a query (modo string literal).
         */
        static bool matches_string(std::string_view line, std::string_view query, bool case_sensitive);

        /**
         * @brief Verifica se a linha corresponde ao padrão regex.
         */
        static bool matches_regex(std::string_view line, const std::regex& pattern);

        /**
         * @brief Verifica se a linha contém o inteiro/hex especificado.
         */
        static bool matches_integer(std::string_view line, std::string_view query);
    };

    /**
     * @brief Factory function para criar ContentSearchEngine.
     */
    std::unique_ptr<ISearchEngine> create_content_search_engine();

} // namespace engines