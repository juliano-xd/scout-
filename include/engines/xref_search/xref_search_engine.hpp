#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <memory>
#include <array>

#include "../i_search_engine.hpp"
#include "../../core/analysis_context.hpp"


namespace engines {

    /**
     * @brief Estrutura específica para resultados de XREF.
     * @details Extende SearchResult com informações de contexto de chamada.
     */
    struct XrefResult final : public SearchResult {
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
     * @file xref_search_engine.hpp
     * @brief Motor de busca especializado para Cross-References (XREF).
     * @details Arquitetura refatorizada para C++26. O motor utiliza um parser
     * Zero-Allocation baseado em Ring Buffers para Taint Analysis e String Views
     * para mapeamento contínuo em RAM, mitigando estrangulamento de I/O e Heap.
     */
    class XrefSearchEngine final : public ISearchEngine {
    public:
        XrefSearchEngine() = default;
        ~XrefSearchEngine() override = default;

        // ==========================================
        // Gestão de Memória Segura (Regra dos 5)
        // ==========================================
        XrefSearchEngine(const XrefSearchEngine&) = delete;
        XrefSearchEngine& operator=(const XrefSearchEngine&) = delete;
        XrefSearchEngine(XrefSearchEngine&&) noexcept = default;
        XrefSearchEngine& operator=(XrefSearchEngine&&) noexcept = default;

        // ==========================================
        // Interface ISearchEngine Overrides
        // ==========================================
        [[nodiscard]] std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        [[nodiscard]] std::string name() const override { return "xref"; }

        [[nodiscard]] std::string description() const override {
            return "Busca especializada por cross-references. Encontra quem chama ou é chamado por uma classe/método/campo específico.";
        }

        [[nodiscard]] EngineStats get_stats() const override { return stats_; }

        [[nodiscard]] bool supports_config(const SearchConfig& config) const override {
            return true;
        }

        // ==========================================
        // Configurações do Motor
        // ==========================================
        void set_direction(std::string_view direction) { direction_ = std::string(direction); }
        void set_include_system(bool include) noexcept { include_system_ = include; }
        void set_depth(int depth) noexcept { depth_ = depth; }

        // Passagem por valor + move: Abordagem moderna para zero-copy se o chamador usar std::move
        void set_opcodes(std::vector<std::string> opcodes) noexcept { filter_opcodes_ = std::move(opcodes); }

        /**
         * @brief Habilita análise de fluxo de dados local (taint lite).
         */
        void set_enable_taint(bool enable) noexcept { enable_taint_ = enable; }

    public:
        // ==========================================
        // Subsistema de Parsing State (Zero Allocation)
        // ==========================================

        /**
         * @brief Estrutura interna para rastrear contexto durante parsing.
         * @details Otimizada para referenciar a memória mmap via std::string_view.
         */
        struct ParseContext {
            std::string_view current_class;
            std::string_view current_method;
            std::string_view current_method_signature;
            size_t line_number = 0;

            // OPTIMIZAÇÃO CRÍTICA: Buffer Circular de Tamanho Fixo na Stack.
            // Elimina milhares de alocações na Heap por ficheiro face ao antigo std::vector<std::string>.
            static constexpr size_t k_history_size = 32;
            std::array<std::string_view, k_history_size> recent_lines{};
            size_t history_head = 0;

            /**
             * @brief Insere uma linha no histórico em O(1) puro.
             */
            constexpr void push_line(std::string_view line) noexcept {
                recent_lines[history_head % k_history_size] = line;
                ++history_head;
            }

            /**
             * @brief Limpa o contexto ao transitar entre métodos.
             */
            constexpr void reset_method() noexcept {
                current_method = {};
                current_method_signature = {};
                history_head = 0; // Invalida o buffer logicamente (Zero Overhead)
            }
        };

        // ==========================================
        // Utilitários Estáticos de Parsing
        // ==========================================

        /**
         * @brief Processa uma linha Smali atualizando o contexto.
         */
        static void update_context(std::string_view trimmed_line, ParseContext& ctx) noexcept;

        /**
         * @brief Verifica se a linha contém uma referência ao alvo.
         */
        [[nodiscard]] static bool contains_target(std::string_view line, std::string_view target) noexcept;

        /**
         * @brief Extrai o opcode de uma instrução (ex: invoke-virtual).
         */
        [[nodiscard]] static std::string_view extract_opcode(std::string_view line) noexcept;

        /**
         * @brief Extrai o tipo de referência (invoke, iget, sput, etc.)
         */
        [[nodiscard]] static std::string_view extract_instruction_type(std::string_view line) noexcept;

        /**
         * @brief Normaliza uma assinatura de método para comparação.
         */
        [[nodiscard]] static std::string normalize_method_signature(std::string_view signature);

        /**
         * @brief Verifica se uma classe é do sistema Android.
         */
        [[nodiscard]] static bool is_system_class(std::string_view class_name) noexcept;

        /**
         * @brief Extrai a lista de registradores (ex: {v0, v1, p0}).
         */
        [[nodiscard]] static std::string_view extract_registers(std::string_view line) noexcept;

        /**
         * @brief Rastreia o valor de um registrador utilizando o Ring Buffer do contexto.
         * @details O `ParseContext` carrega a view de todo o estado recente sem fragmentar memória.
         */
        [[nodiscard]] static std::string trace_register_value(std::string_view reg, const ParseContext& ctx);

    private:
        EngineStats stats_;
        std::string direction_ = "both";
        std::vector<std::string> filter_opcodes_;
        bool include_system_ = false;
        int depth_ = 1;
        bool enable_taint_ = false;

        /**
         * @brief Executa busca XREF em um conjunto de arquivos.
         */
        [[nodiscard]] std::vector<SearchResult> perform_search(
            const std::vector<std::filesystem::path>& files,
            const std::string& target,
            const SearchConfig& config,
            const std::filesystem::path& root_dir
        );

        /**
         * @brief Encontra o que este método/classe chama (callees).
         */
        [[nodiscard]] std::vector<SearchResult> search_callees(
            const std::filesystem::path& root_dir,
            const std::string& target,
            const SearchConfig& config
        );
    };

    /**
     * @brief Factory function para criar XrefSearchEngine.
     */
    [[nodiscard]] std::unique_ptr<ISearchEngine> create_xref_search_engine();

} // namespace engines
