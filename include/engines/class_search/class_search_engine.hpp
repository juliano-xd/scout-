#pragma once

#include "../i_search_engine.hpp"
#include "../../../include/core/analysis_context.hpp"
#include <string>
#include <vector>
#include <memory>

namespace engines {

    /**
     * @brief Motor de busca especializado para localizar classes Smali.
     *
     * Suporta dois modos:
     * 1. Busca exata por nome Dalvik (Lcom/example/A;) → retorna arquivo único
     * 2. Busca por substring no nome da classe → retorna múltiplos arquivos
     *
     *   query = "Lcom/example/LoginActivity" (sem ; final) → trata como substring
     */
    class ClassSearchEngine : public ISearchEngine {
    public:
        ClassSearchEngine() = default;
        ~ClassSearchEngine() override = default;

        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "class"; }

        std::string description() const override {
            return "Busca especializada por classes Smali. Aceita notação Dalvik completa (L...;) ou substring do nome da classe.";
        }

        EngineStats get_stats() const override { return stats_; }

        bool supports_config(const SearchConfig& config) const override {
            return true;
        }

        /**
         * @brief Detecta se a query parece ser uma notação Dalvik completa.
         */
        static bool is_dalvik_notation(std::string_view query) {
            return query.starts_with('L') && query.ends_with(';');
        }

        /**
         * @brief Normaliza a query removendo L e ; se presentes.
         */
        static std::string normalize_dalvik(std::string_view query) {
            std::string_view result = query;
            if (result.starts_with('L') && result.ends_with(';')) {
                result = result.substr(1, result.size() - 2);
            } else {
                if (result.starts_with('L')) result.remove_prefix(1);
                if (result.ends_with(';')) result.remove_suffix(1);
            }
            return std::string(result);
        }

    private:
        EngineStats stats_;
    };

    /**
     * @brief Factory function para criar ClassSearchEngine.
     * Usado pelo EngineRegistry.
     */
    std::unique_ptr<ISearchEngine> create_class_search_engine();

} // namespace engines
