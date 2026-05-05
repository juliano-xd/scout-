#include "../../../include/engines/class_search/class_search_engine.hpp"
#include "../../../include/core/scanner.hpp"

#include <chrono>
#include <system_error>
#include <format>       // C++20/26: Substitui concatenação O(N^2)
#include <algorithm>    // Para std::min

namespace engines {

    namespace {
        // Helper utilitário para medir tempo de execução com deduce type
        template<typename F>
        auto measure_execution(F&& func) {
            const auto start = std::chrono::high_resolution_clock::now();
            auto result = func();
            const auto end = std::chrono::high_resolution_clock::now();
            return std::make_pair(
                std::move(result),
                std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            );
        }
    } // anonymous namespace

    std::vector<SearchResult> ClassSearchEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        const auto& root_dir = ctx.root_dir();
        std::vector<SearchResult> results;

        if (config.query.empty()) {
            return results;
        }

        // Validação Estrita RAII/no-throw guarantee (C++17)
        // Impede Crash-to-Desktop se root_dir não tiver permissões de acesso
        std::error_code ec;
        if (!std::filesystem::is_directory(root_dir, ec) || ec) {
            return results;
        }

        // Executar busca medindo tempo
        auto [search_results, elapsed] = measure_execution([&]() -> std::vector<std::filesystem::path> {
            std::vector<std::filesystem::path> found_files;

            // Promoção passiva a string_view para evitar cópias não-intencionais no lookup
            const std::string_view query = config.query;

            if (is_dalvik_notation(query)) {
                // Modo exato: busca por nome Dalvik completo
                const std::string normalized = normalize_dalvik(query);
                auto file_opt = core::find_class_file(root_dir, normalized);

                if (file_opt && core::is_path_filtered(*file_opt, config.include_dirs, config.exclude_dirs)) {
                    // Mover o valor diretamente do optional contornando a cópia
                    found_files.push_back(std::move(*file_opt));
                }
            } else {
                // Modo substring: varredura paralela por parte do nome
                found_files = core::find_classes_containing(root_dir, query, config.include_dirs, config.exclude_dirs);
            }

            return found_files;
        });

        // ==========================================
        // Conversão Otimizada de Resultados
        // ==========================================
        // Pré-alocação controlada: Evita alocar milhares de espaços se max_results for baixo
        const size_t max_items = static_cast<size_t>(config.max_results);
        results.reserve(std::min(search_results.size(), max_items));

        for (auto& file_path : search_results) {
            if (results.size() >= max_items) {
                break;
            }

            SearchResult result;

            // OPTIMIZAÇÃO: stem() extrai instantaneamente o nome ignorando a extensão (.smali)
            // Substitui completamente a lógica frágil e lenta baseada em string.compare()
            const std::string class_name = file_path.stem().string();

            result.file_path = std::move(file_path); // Transferência O(1) de owner
            result.line_number = 0;

            // C++20: Formatação in-place direta
            result.context = std::format("class:{}", class_name);

            result.engine_name = this->name();
            result.search_time = elapsed;

            results.push_back(std::move(result));
        }

        // Atualização de Estatísticas do Motor
        stats_.files_scanned = 0; // O scanning nativo é gerido pelo FS Core
        stats_.matches_found = results.size();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

        return results;
    }

    std::unique_ptr<ISearchEngine> create_class_search_engine() {
        return std::make_unique<ClassSearchEngine>();
    }

} // namespace engines
