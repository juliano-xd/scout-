#include "../../../include/engines/class_search/class_search_engine.hpp"
#include "../../../include/core/scanner.hpp"

#include <chrono>
#include <system_error>
#include <format>       // C++20/26: Substitui concatenação O(N^2) eficiente
#include <algorithm>    // Para std::min

namespace engines {

    namespace {
        /**
         * @brief Utilitário para medir tempo de execução.
         * 
         * Encapsula a lógica de timing para evitar repetição de código boilerplate.
         */
        template<typename F>
        auto measure_execution(F&& func) {
            const auto start = std::chrono::high_resolution_clock::now();
            auto result = func();
            const auto end = std::chrono::high_resolution_clock::now();
            return std::make_pair(
                std::move(result),
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            );
        }
    } // namespace anônimo

    std::vector<SearchResult> ClassSearchEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        const auto& root_dir = ctx.root_dir();
        std::vector<SearchResult> results;

        // Early return para consultas vazias
        if (config.query.empty()) {
            return results;
        }

        // Validação de segurança do diretório raiz utilizando a API filesystem do C++17
        std::error_code ec;
        if (!std::filesystem::is_directory(root_dir, ec) || ec) {
            return results;
        }

        // Captura o limite de resultados antecipadamente para otimizar a varredura
        const size_t max_items = static_cast<size_t>(config.max_results);

        // Fase 1: Localização de Arquivos
        // A busca é realizada dentro do wrapper de medição de tempo
        auto [search_results, elapsed] = measure_execution([&]() -> std::vector<std::filesystem::path> {
            std::vector<std::filesystem::path> found_files;
            const std::string_view query = config.query;

            // Decisão Algorítmica:
            // Se a query parece uma assinatura Dalvik, tentamos uma resolução direta (muito mais rápida).
            // Caso contrário, fazemos uma varredura por substring em todo o projeto.
            if (is_dalvik_notation(query)) {
                const std::string normalized = normalize_dalvik(query);
                
                // core::find_class_file utiliza heurísticas de pacotes comuns para O(1) em muitos casos
                auto file_opt = core::find_class_file(root_dir, normalized);

                if (file_opt && core::is_path_filtered(*file_opt, config.include_dirs, config.exclude_dirs)) {
                    found_files.push_back(std::move(*file_opt));
                }
            } else {
                // Modo Substring: Varredura paralela otimizada com suporte a interrupção precoce (Short-Circuit)
                found_files = core::find_classes_containing(root_dir, query, config.include_dirs, config.exclude_dirs, max_items);
            }

            return found_files;
        });

        // Fase 2: Conversão para Formato de Saída (SearchResult)
        // Pré-alocação do vetor para evitar realocações dinâmicas custosas
        results.reserve(std::min(search_results.size(), max_items));

        for (auto& file_path : search_results) {
            // Garante o respeito estrito ao limite solicitado
            if (results.size() >= max_items) {
                break;
            }

            SearchResult result;

            // OTIMIZAÇÃO: file_path.stem() extrai apenas o nome do arquivo (sem extensão) de forma eficiente.
            // Isso evita operações de substring manuais sobre o path completo.
            const std::string class_name = file_path.stem().string();

            // Transferência de ownership do path para evitar cópia desnecessária (std::move)
            result.file_path = std::move(file_path);
            result.line_number = 0; // Busca de classe não referencia uma linha específica de código

            // Gera metadados de contexto utilizando std::format (C++20) para máxima velocidade
            result.context = std::format("class:{}", class_name);

            result.engine_name = this->name();
            result.search_time = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);

            results.push_back(std::move(result));
        }

        // Atualização de Estatísticas para relatório final
        stats_.files_scanned = 0; // O scanning é abstraído pelo módulo core
        stats_.matches_found = results.size();
        stats_.total_time = elapsed;

        return results;
    }

    std::unique_ptr<ISearchEngine> create_class_search_engine() {
        return std::make_unique<ClassSearchEngine>();
    }

} // namespace engines
