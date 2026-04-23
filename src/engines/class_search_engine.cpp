#include "engines/class_search_engine.hpp"
#include "utils/filesystem.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <algorithm>
#include <mutex>

namespace engines {

    namespace {
        // Helper para medir tempo de execução
        template<typename F>
        auto measure_execution(F&& func) {
            auto start = std::chrono::high_resolution_clock::now();
            auto result = func();
            auto end = std::chrono::high_resolution_clock::now();
            return std::make_pair(
                result,
                std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            );
        }
    }

    std::vector<SearchResult> ClassSearchEngine::search(
        const std::filesystem::path& root_dir,
        const SearchConfig& config
    ) {
        std::vector<SearchResult> results;
        
        if (config.query.empty()) {
            return results;
        }

        // Validar diretório raiz
        if (!std::filesystem::exists(root_dir) || !std::filesystem::is_directory(root_dir)) {
            return results;
        }

        // Executar busca medindo tempo
        auto [search_results, elapsed] = measure_execution([&]() -> std::vector<std::filesystem::path> {
            std::vector<std::filesystem::path> found_files;
            
            const std::string& query = config.query;
            
            if (is_dalvik_notation(query)) {
                // Modo exato: busca por nome Dalvik completo
                std::string normalized = normalize_dalvik(query);
                auto file_opt = core::find_class_file(root_dir, normalized);
                if (file_opt) {
                    found_files.push_back(*file_opt);
                }
            } else {
                // Modo substring: busca por parte do nome da classe
                found_files = core::find_classes_containing(root_dir, query);
            }
            
            return found_files;
        });

        // Converter para SearchResult padronizado
        results.reserve(search_results.size());
        for (const auto& file_path : search_results) {
            if (results.size() >= config.max_results) {
                break;
            }
            
            // Extrair nome da classe do arquivo (para contexto)
            std::string filename = file_path.filename().string();
            std::string class_name = filename;
            if (class_name.size() > 6 && class_name.compare(class_name.size() - 6, 6, ".smali") == 0) {
                class_name = class_name.substr(0, class_name.size() - 6);
            }
            
            SearchResult result;
            result.file_path = file_path;
            result.line_number = 0; // Busca por arquivo, não por linha
            result.line_content = ""; // Conteúdo completo não é carregado aqui
            result.context = "class:" + class_name;
            result.engine_name = this->name();
            result.search_time = elapsed;
            
            results.push_back(std::move(result));
        }

        // Atualizar estatísticas
        stats_.files_scanned = 0; // Não escaneamos arquivos individualmente aqui
        stats_.matches_found = results.size();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        
        return results;
    }


    std::unique_ptr<ISearchEngine> create_class_search_engine() {
        return std::make_unique<ClassSearchEngine>();
    }

} // namespace engines