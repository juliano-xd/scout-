#pragma once

#include "i_search_engine.hpp"
#include "core/scanner.hpp"
#include <string>
#include <vector>
#include <filesystem>
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
            const std::filesystem::path& root_dir,
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
        static bool is_dalvik_notation(const std::string& query) {
            return query.starts_with('L') && query.ends_with(';');
        }

        /**
         * @brief Normaliza a query removendo L e ; se presentes.
         */
        static std::string normalize_dalvik(const std::string& query) {
            std::string result = query;
            if (result.starts_with('L') && result.ends_with(';')) {
                return result.substr(1, result.size() - 2);
            }
            if (result.starts_with('L')) {
                result = result.substr(1);
            }
            if (result.ends_with(';')) {
                result.pop_back();
            }
            return result;
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