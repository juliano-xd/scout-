#pragma once

#include "../i_search_engine.hpp"
#include "../../core/analysis_context.hpp"
#include <string>
#include <regex>
#include <memory>

namespace engines {

    /**
     * @brief Motor de busca especializado para conteúdo interno de arquivos Smali.
     *
     * O ContentSearchEngine realiza buscas textuais profundas dentro do código Smali descompilado.
     * Ele é projetado para máxima performance, utilizando mapeamento de memória para acesso a arquivos
     * e execução paralela em múltiplos núcleos.
     * 
     * Suporta três modos principais de busca definidos via SearchConfig:
     * - "string": Busca literal simples (opcionalmente case-sensitive).
     * - "regex": Busca avançada utilizando expressões regulares ECMAScript.
     * - "integer": Busca inteligente por valores numéricos (decimal ou hexadecimal), 
     *              garantindo que o match ocorra em limites de palavras (boundaries).
     *
     * Principais Otimizações:
     * - Memory-mapped files (mmap) via AnalysisContext para evitar cópias de buffer.
     * - Paralelização agressiva via std::execution::par_unseq (Vetorização + Multi-threading).
     * - Mecanismo de Short-Circuit atômico para interrupção imediata ao atingir max_results.
     * - Extração automática de contexto (classe e método) para cada ocorrência encontrada.
     */
    class ContentSearchEngine : public ISearchEngine {
    public:
        /**
         * @brief Construtor padrão do motor de busca de conteúdo.
         */
        ContentSearchEngine() = default;

        /**
         * @brief Destrutor virtual para garantir limpeza correta de classes derivadas.
         */
        ~ContentSearchEngine() override = default;

        /**
         * @brief Executa a busca de conteúdo com base na configuração fornecida.
         * 
         * Este é o método principal que orquestra a varredura de arquivos, a aplicação dos filtros
         * de inclusão/exclusão e a execução da busca paralela.
         * 
         * @param ctx Referência ao AnalysisContext que provê acesso aos arquivos mmap'd.
         * @param config Objeto de configuração contendo a query, tipo de busca e limites.
         * @return std::vector<SearchResult> Lista de resultados encontrados respeitando o limite.
         */
        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        /**
         * @brief Retorna o identificador interno do motor.
         * @return std::string "content"
         */
        std::string name() const override { return "content"; }

        /**
         * @brief Retorna uma descrição amigável das capacidades do motor.
         * @return std::string Descrição resumida.
         */
        std::string description() const override {
            return "Busca especializada por conteúdo em arquivos Smali. Suporta string literal, regex e busca por inteiros.";
        }

        /**
         * @brief Recupera as estatísticas da última execução (arquivos lidos, tempo, etc).
         * @return EngineStats Estrutura com dados de performance.
         */
        EngineStats get_stats() const override { return stats_; }

        /**
         * @brief Verifica se a configuração solicitada é compatível com este motor.
         * @param config Configuração a ser validada.
         * @return true Sempre retorna true para ContentSearchEngine.
         */
        bool supports_config(const SearchConfig& config) const override {
            return true;
        }

    public:
        /**
         * @brief Estrutura interna para rastrear o estado do parser durante a leitura de um arquivo.
         * 
         * Mantém informações sobre a classe e o método atual para fornecer contexto rico 
         * aos resultados da busca sem precisar re-escanear o arquivo.
         */
        struct ParseContext {
            std::string current_class;  ///< Nome da classe atual (.class)
            std::string current_method; ///< Assinatura do método atual (.method)
            size_t line_number = 0;     ///< Contador de linhas absoluto no arquivo
        };

        /**
         * @brief Analisa uma linha Smali para atualizar o contexto (classe/método).
         * 
         * Identifica diretivas como .class, .method e .end method para rastrear em qual
         * escopo as instruções seguintes pertencem.
         * 
         * @param line View da linha atual.
         * @param ctx Referência ao contexto a ser atualizado.
         */
        static void update_context(std::string_view line, ParseContext& ctx);

        /**
         * @brief Verifica se uma linha contém a string de busca.
         * 
         * @param line Linha a ser verificada.
         * @param query Texto a procurar.
         * @param case_sensitive Se a busca deve diferenciar maiúsculas/minúsculas.
         * @return true Se houver correspondência.
         */
        static bool matches_string(std::string_view line, std::string_view query, bool case_sensitive);

        /**
         * @brief Executa busca por expressão regular na linha.
         * 
         * @param line Linha a ser verificada.
         * @param pattern Objeto regex já compilado.
         * @return true Se a linha satisfizer a regex.
         */
        static bool matches_regex(std::string_view line, const std::regex& pattern);

        /**
         * @brief Realiza busca inteligente de valores numéricos.
         * 
         * Garante que o número encontrado seja uma entidade isolada (usando checagem de limites
         * alfanuméricos) e suporta normalização de prefixos hexadecimais (0x).
         * 
         * @param line Linha a ser verificada.
         * @param query Valor numérico em formato string.
         * @return true Se o número for encontrado como um token isolado.
         */
        static bool matches_integer(std::string_view line, std::string_view query);

    private:
        EngineStats stats_; ///< Estatísticas acumuladas de execução.
    };

    /**
     * @brief Função de fábrica para instanciar o ContentSearchEngine.
     * @return std::unique_ptr<ISearchEngine> Instância do motor.
     */
    std::unique_ptr<ISearchEngine> create_content_search_engine();

} // namespace engines
