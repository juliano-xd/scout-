#pragma once

#include "../i_search_engine.hpp"
#include "../../../include/core/analysis_context.hpp"
#include <string>
#include <vector>
#include <memory>

namespace engines {

    /**
     * @brief Motor de busca especializado para localização de classes Smali/Dalvik.
     *
     * O ClassSearchEngine é otimizado para encontrar arquivos .smali no sistema de arquivos
     * de duas maneiras principais:
     * 
     * 1. **Modo Direto (Exact Match):** Quando a query utiliza notação Dalvik completa 
     *    (ex: Lcom/whatsapp/Main;). O motor utiliza heurísticas de caminhos comuns e 
     *    o índice do AnalysisContext para resolução O(1).
     * 
     * 2. **Modo Substring (Partial Match):** Quando a query é apenas parte de um nome ou caminho.
     *    Neste modo, o motor realiza uma varredura paralela utilizando as threads do sistema
     *    para encontrar todas as classes que satisfaçam o critério.
     *
     * Otimizações:
     * - Resolução ultrarrápida para nomes qualificados.
     * - Integração com filtros globais de diretórios (include/exclude).
     * - Interrupção precoce (Early Exit) em buscas parciais ao atingir max_results.
     */
    class ClassSearchEngine : public ISearchEngine {
    public:
        /**
         * @brief Construtor padrão.
         */
        ClassSearchEngine() = default;

        /**
         * @brief Destrutor virtual.
         */
        ~ClassSearchEngine() override = default;

        /**
         * @brief Executa a busca de classes.
         * 
         * Analisa a query para decidir entre busca exata ou por substring e coordena
         * a varredura do sistema de arquivos através do core::scanner.
         * 
         * @param ctx Contexto de análise atual.
         * @param config Configuração da busca (query, limites, filtros).
         * @return std::vector<SearchResult> Lista de classes encontradas.
         */
        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        /**
         * @brief Retorna o identificador do motor.
         * @return std::string "class"
         */
        std::string name() const override { return "class"; }

        /**
         * @brief Descrição das funcionalidades do motor.
         * @return std::string Texto explicativo.
         */
        std::string description() const override {
            return "Busca especializada por classes Smali. Aceita notação Dalvik completa (L...;) ou substring do nome da classe.";
        }

        /**
         * @brief Recupera estatísticas da última execução.
         * @return EngineStats Dados de performance.
         */
        EngineStats get_stats() const override { return stats_; }

        /**
         * @brief Valida se a configuração é suportada.
         * @param config Configuração a validar.
         * @return true Sempre compatível.
         */
        bool supports_config(const SearchConfig& config) const override {
            return true;
        }

        /**
         * @brief Detecta se uma string segue o padrão Dalvik (L...;).
         * 
         * Útil para decidir se a busca deve ser exata ou por substring.
         * 
         * @param query Texto a verificar.
         * @return true Se começar com 'L' e terminar com ';'.
         */
        static bool is_dalvik_notation(std::string_view query) {
            return query.starts_with('L') && query.ends_with(';');
        }

        /**
         * @brief Normaliza um nome Dalvik removendo prefixos e sufixos.
         * 
         * Exemplo: "Lcom/android/App;" -> "com/android/App"
         * 
         * @param query Nome Dalvik original.
         * @return std::string Nome normalizado para busca em disco.
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
        EngineStats stats_; ///< Estatísticas de execução.
    };

    /**
     * @brief Factory para criação do ClassSearchEngine.
     */
    std::unique_ptr<ISearchEngine> create_class_search_engine();

} // namespace engines
