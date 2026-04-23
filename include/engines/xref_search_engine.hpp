#pragma once

#include "i_search_engine.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <mutex>
#include <execution>

namespace engines {

    /**
     * @brief Estrutura específica para resultados de XREF.
     * Extende SearchResult com informações de contexto de chamada.
     */
    struct XrefResult : public SearchResult {
        std::string caller_class;       // Classe que faz a chamada
        std::string caller_method;      // Método que faz a chamada
        std::string target_class;       // Classe do alvo (se for método/field)
        std::string target_member;      // Nome do método/field alvo
        std::string xref_type;          // "caller", "callee", "reference"
        std::string instruction;        // Instrução Smali que contém a referência
    };

    /**
     * @brief Motor de busca especializado para Cross-References (XREF).
     * 
     * Encontra todas as referências a uma classe, método ou campo específico.
     * Suporta:
     * - Busca por classe completa (Lcom/example/A;)
     * - Busca por método específico (Lcom/example/A;->method()V)
     * - Busca por campo (Lcom/example/A;->field:I)
     * 
     * Direções suportadas:
     * - "callers": quem chama este método/classe
     * - "callees": o que este método/classe chama
     * - "both": ambas as direções
     * 
     * Recursos:
     * - Paralelização agressiva em múltiplos arquivos
     * - Contexto estrutural (rastreia .class e .method atuais)
     * - Suporte a profundidade recursiva (XREF de XREF)
     * - Filtragem de classes do sistema Android
     */
    class XrefSearchEngine : public ISearchEngine {
    public:
        XrefSearchEngine() = default;
        ~XrefSearchEngine() override = default;

        std::vector<SearchResult> search(
            const std::filesystem::path& root_dir,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "xref"; }

        std::string description() const override {
            return "Busca especializada por cross-references. Encontra quem chama ou é chamado por uma classe/método/campo específico.";
        }

        EngineStats get_stats() const override { return stats_; }

        bool supports_config(const SearchConfig& config) const override {
            // XREF suporta todas as configurações básicas
            return true;
        }

        /**
         * @brief Define a direção da busca XREF.
         * @param direction "callers", "callees" ou "both"
         */
        void set_direction(const std::string& direction) {
            direction_ = direction;
        }

        /**
         * @brief Define se deve incluir classes do sistema Android.
         * @param include true para incluir, false para omitir
         */
        void set_include_system(bool include) {
            include_system_ = include;
        }

        /**
         * @brief Define a profundidade máxima para XREF recursivo.
         * @param depth Profundidade (1 = direto, 2 = indireto, etc.)
         */
        void set_depth(int depth) {
            depth_ = depth;
        }

    private:
        EngineStats stats_;
        std::string direction_ = "both";
        bool include_system_ = false;
        int depth_ = 1;

        /**
         * @brief Estrutura interna para rastrear contexto durante parsing.
         */
        struct ParseContext {
            std::string current_class;
            std::string current_method;
            std::string current_method_signature;  // Assinatura completa (inclui parênteses e retorno)
            size_t line_number = 0;
        };

        /**
         * @brief Processa uma linha Smali atualizando o contexto.
         */
        static void update_context(const std::string& trimmed_line, ParseContext& ctx);

        /**
         * @brief Verifica se a linha contém uma referência ao alvo.
         */
        static bool contains_target(const std::string& line, const std::string& target);

        /**
         * @brief Extrai o tipo de referência (invoke, iget, sput, etc.)
         */
        static std::string extract_instruction_type(const std::string& line);

        /**
         * @brief Normaliza uma assinatura de método para comparação.
         */
        static std::string normalize_method_signature(const std::string& signature);

        /**
         * @brief Verifica se uma classe é do sistema Android.
         */
        static bool is_system_class(const std::string& class_name);

        /**
         * @brief Executa XREF recursivo até a profundidade especificada.
         */
        std::vector<XrefResult> search_recursive(
            const std::filesystem::path& root_dir,
            const std::string& target,
            int current_depth,
            const SearchConfig& config
        );
    };

    /**
     * @brief Factory function para criar XrefSearchEngine.
     */
    std::unique_ptr<ISearchEngine> create_xref_search_engine();

} // namespace engines