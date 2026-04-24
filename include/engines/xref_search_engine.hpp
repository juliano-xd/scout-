#pragma once

#include "i_search_engine.hpp"
#include "core/analysis_context.hpp"
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
        std::string access_type;        // "read", "write", "invoke", "other"
        std::string instruction;        // Instrução Smali que contém a referência
        std::vector<std::string> tainted_values; // Valores rastreados (ex: strings literais em regs)
    };

    /**
     * @brief Motor de busca especializado para Cross-References (XREF).
     * ... (comments truncated)
     */
    class XrefSearchEngine : public ISearchEngine {
    public:
        XrefSearchEngine() = default;
        ~XrefSearchEngine() override = default;

        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "xref"; }

        std::string description() const override {
            return "Busca especializada por cross-references. Encontra quem chama ou é chamado por uma classe/método/campo específico.";
        }

        EngineStats get_stats() const override { return stats_; }

        bool supports_config(const SearchConfig& config) const override {
            return true;
        }

        void set_direction(const std::string& direction) { direction_ = direction; }
        void set_include_system(bool include) { include_system_ = include; }
        void set_depth(int depth) { depth_ = depth; }
        void set_opcodes(const std::vector<std::string>& opcodes) { filter_opcodes_ = opcodes; }
        
        /**
         * @brief Habilita análise de fluxo de dados local (taint lite).
         */
        void set_enable_taint(bool enable) { enable_taint_ = enable; }

    private:
        EngineStats stats_;
        std::string direction_ = "both";
        bool include_system_ = false;
        int depth_ = 1;
        bool enable_taint_ = false;
        std::vector<std::string> filter_opcodes_;

        /**
         * @brief Estrutura interna para rastrear contexto durante parsing.
         */
        struct ParseContext {
            std::string current_class;
            std::string current_method;
            std::string current_method_signature;
            size_t line_number = 0;
            // Buffer circular para análise de taint (últimas 10-20 linhas do método)
            std::vector<std::string> recent_lines;
        };


        /**
         * @brief Processa uma linha Smali atualizando o contexto.
         */
        static void update_context(std::string_view trimmed_line, ParseContext& ctx);

        /**
         * @brief Verifica se a linha contém uma referência ao alvo.
         */
        static bool contains_target(std::string_view line, std::string_view target);

        /**
         * @brief Extrai o opcode de uma instrução (ex: invoke-virtual).
         */
        static std::string_view extract_opcode(std::string_view line);

        /**
         * @brief Extrai o tipo de referência (invoke, iget, sput, etc.)
         */
        static std::string extract_instruction_type(std::string_view line);

        /**
         * @brief Normaliza uma assinatura de método para comparação.
         */
        static std::string normalize_method_signature(std::string_view signature);

        /**
         * @brief Verifica se uma classe é do sistema Android.
         */
        static bool is_system_class(std::string_view class_name);

        /**
         * @brief Executa busca XREF em um conjunto de arquivos.
         */
        std::vector<SearchResult> perform_search(
            const std::vector<std::filesystem::path>& files,
            const std::string& target,
            const SearchConfig& config
        );

        /**
         * @brief Encontra o que este método/classe chama (callees).
         */
        std::vector<SearchResult> search_callees(
            const std::filesystem::path& root_dir,
            const std::string& target,
            const SearchConfig& config
        );


    };

    /**
     * @brief Factory function para criar XrefSearchEngine.
     */
    std::unique_ptr<ISearchEngine> create_xref_search_engine();

} // namespace engines