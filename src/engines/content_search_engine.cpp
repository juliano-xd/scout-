#include "engines/content_search_engine.hpp"
#include "utils/filesystem.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <fstream>
#include <sstream>

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

    void ContentSearchEngine::update_context(const std::string& line, ParseContext& ctx) {
        // Trim leading whitespace
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return;
        std::string trimmed = line.substr(first, line.find_last_not_of(" \t\r\n") - first + 1);

        if (trimmed.find(".class ") == 0) {
            size_t class_start = trimmed.find_last_of(' ');
            if (class_start != std::string::npos) {
                ctx.current_class = trimmed.substr(class_start + 1);
            }
        } else if (trimmed.find(".method ") == 0) {
            ctx.current_method = trimmed.substr(8);
        } else if (trimmed == ".end method") {
            ctx.current_method.clear();
        }
        ctx.line_number++;
    }

    bool ContentSearchEngine::matches_string(const std::string& line, const std::string& query, bool case_sensitive) {
            if (case_sensitive) {
                return line.find(query) != std::string::npos;
            } else {
                std::string line_lower = line;
                std::string query_lower = query;
                std::transform(line_lower.begin(), line_lower.end(), line_lower.begin(), ::tolower);
                std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
                return line_lower.find(query_lower) != std::string::npos;
            }
        }

    bool ContentSearchEngine::matches_regex(const std::string& line, const std::regex& pattern) {
        return std::regex_search(line, pattern);
    }

    bool ContentSearchEngine::matches_integer(const std::string& line, const std::string& query) {
            // Busca por inteiros em diferentes formatos:
            // - const/16 v0, 0x1
            // - const v0, 42
            // - const-wide v0, 0x1 (para longs)
            
            // Normaliza query para string sem prefixo
            std::string normalized_query = query;
            if (normalized_query.starts_with("0x")) {
                normalized_query = normalized_query.substr(2);
            }
            
            // Tokeniza a linha procurando números
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) {
                // Remove vírgulas e outros caracteres
                token.erase(std::remove_if(token.begin(), token.end(), 
                    [](char c) { return c == ',' || c == ';' || c == '(' || c == ')'; }), 
                    token.end());
                
                // Verifica se é um número (hex ou decimal)
                if (token.empty()) continue;
                
                // Testa formato hexadecimal
                if (token.starts_with("0x") || token.starts_with("0X")) {
                    std::string hex_part = token.substr(2);
                    // Compara hexadecimal ignorando case
                    std::string hex_lower = hex_part;
                    std::transform(hex_lower.begin(), hex_lower.end(), hex_lower.begin(), ::tolower);
                    std::string query_lower = normalized_query;
                    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
                    if (hex_lower == query_lower) return true;
                } 
                // Testa formato decimal
                else if (std::all_of(token.begin(), token.end(), ::isdigit)) {
                    if (token == normalized_query) return true;
                }
            }
            return false;
    }

    std::vector<SearchResult> ContentSearchEngine::search(
        const std::filesystem::path& root_dir,
        const SearchConfig& config
    ) {
        std::vector<SearchResult> results;
        
        if (config.query.empty()) {
            return results;
        }

        if (!std::filesystem::exists(root_dir) || !std::filesystem::is_directory(root_dir)) {
            return results;
        }

        // Preparar padrão de regex se necessário
        std::optional<std::regex> regex_pattern;
        if (config.search_type == "regex" || config.search_type == "string") {
            try {
                regex_pattern = std::regex(config.query);
            } catch (...) {
                // Regex inválida, retorna vazio
                return results;
            }
        }

        auto options = std::filesystem::directory_options::skip_permission_denied;
        std::vector<std::filesystem::path> all_files;
        
        // Coletar todos os arquivos .smali
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root_dir, options)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.size() > 6 && filename.compare(filename.size() - 6, 6, ".smali") == 0) {
                    all_files.push_back(entry.path());
                }
            }
        }

        std::mutex mtx;
        size_t total_files = all_files.size();

        // Executar busca paralela
        auto [search_time, elapsed] = measure_execution([&]() {
            std::for_each(std::execution::par_unseq, all_files.begin(), all_files.end(),
                [&](const std::filesystem::path& file_path) {
                    std::ifstream file(file_path);
                    if (!file.is_open()) return;

                    std::string line;
                    ParseContext ctx;
                    std::vector<SearchResult> local_results;

                    while (std::getline(file, line)) {
                        // Atualizar contexto (class/method)
                        update_context(line, ctx);

                        // Verificar match
                        bool matched = false;
                        if (config.search_type == "regex" && regex_pattern) {
                            matched = matches_regex(line, *regex_pattern);
                        } else if (config.search_type == "integer") {
                            matched = matches_integer(line, config.query);
                        } else {
                            // Default: string literal search
                            matched = matches_string(line, config.query, config.case_sensitive);
                        }

                        if (matched) {
                            // Trim da linha
                            size_t first = line.find_first_not_of(" \t\r\n");
                            size_t last = line.find_last_not_of(" \t\r\n");
                            std::string trimmed_line = (first != std::string::npos && last != std::string::npos) 
                                ? line.substr(first, last - first + 1) 
                                : line;

                            SearchResult result;
                            result.file_path = file_path;
                            result.line_number = ctx.line_number;
                            result.line_content = trimmed_line;
                            result.context = ctx.current_method.empty() 
                                ? "class:" + ctx.current_class 
                                : "method:" + ctx.current_method;
                            result.engine_name = this->name();
                            result.search_time = std::chrono::microseconds(0); // Será preenchido depois
                            
                            local_results.push_back(std::move(result));
                        }
                    }

                    if (!local_results.empty()) {
                        std::lock_guard<std::mutex> lock(mtx);
                        // Adicionar respeitando limite max_results
                        for (auto& r : local_results) {
                            if (results.size() < config.max_results) {
                                results.push_back(std::move(r));
                            } else {
                                break;
                            }
                        }
                    }
                });
            
            return 0; // Valor dummy para measure_execution
        });

        // Atualizar estatísticas
        stats_.files_scanned = total_files;
        stats_.matches_found = results.size();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

        return results;
    }


    std::unique_ptr<ISearchEngine> create_content_search_engine() {
        return std::make_unique<ContentSearchEngine>();
    }

} // namespace engines