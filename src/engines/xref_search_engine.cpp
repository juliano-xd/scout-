#include "engines/xref_search_engine.hpp"
#include "utils/filesystem.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <fstream>
#include <sstream>
#include <queue>
#include <set>

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

        // Helper para extrair nome da classe de uma linha .class
        std::string extract_class_name(const std::string& line) {
            size_t last_space = line.find_last_of(' ');
            if (last_space != std::string::npos && last_space + 1 < line.size()) {
                std::string class_name = line.substr(last_space + 1);
                // Remover trailing ; se existir
                if (!class_name.empty() && class_name.back() == ';') {
                    class_name.pop_back();
                }
                return class_name;
            }
            return "";
        }

        // Helper para extrair assinatura completa de método de uma linha .method
        std::string extract_method_signature(const std::string& line) {
            // .method public doLogin(Ljava/lang/String;Ljava/lang/String;)Z
            const std::string method_prefix = ".method ";
            size_t pos = line.find(method_prefix);
            if (pos != std::string::npos) {
                std::string signature = line.substr(pos + method_prefix.length());
                // Remover modificadores access (public, static, etc) do início se houver
                size_t paren_pos = signature.find('(');
                if (paren_pos != std::string::npos) {
                    size_t last_space_before_paren = signature.find_last_of(' ', paren_pos);
                    if (last_space_before_paren != std::string::npos) {
                        signature = signature.substr(last_space_before_paren + 1);
                    }
                }
                return signature;
            }
            return ""; // BUG FIX: missing return on non-matching path
        }
    } // anonymous namespace

    // Verifica se uma linha Smali contém referência ao target
    bool XrefSearchEngine::contains_target(const std::string& line, const std::string& target) {
            // O target pode ser uma classe (Lcom/example/A;) ou método (Lcom/example/A;->method()V)
            // No Smali, referências aparecem como:
            // - invoke-virtual {p0}, Lcom/example/A;->doLogin()V
            // - iget-object v0, p1, Lcom/example/A;->field:I
            // - sput-object v0, Lcom/example/A;->staticField:Ljava/lang/String;
            
            // Busca simples por substring (o target já deve estar no formato Dalvik completo)
            return line.find(target) != std::string::npos;
        }

    // Extrai o tipo de instrução (invoke, iget, sput, etc.)
    std::string XrefSearchEngine::extract_instruction_type(const std::string& line) {
            // Trim leading whitespace
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return "";
            std::string trimmed = line.substr(first, line.find_last_not_of(" \t\r\n") - first + 1);
            
            // Primeiro token é o opcode
            size_t space_pos = trimmed.find(' ');
            if (space_pos != std::string::npos) {
                return trimmed.substr(0, space_pos);
            }
            return trimmed;
        }

    // Verifica se uma classe é do sistema Android
    bool XrefSearchEngine::is_system_class(const std::string& class_name) {
            // Classes do sistema começam com "Landroid/", "Ljava/", "Ljavax/", "Lsun/", etc.
            static const std::vector<std::string> system_prefixes = {
                "Landroid/",
                "Ljava/",
                "Ljavax/",
                "Lsun/",
                "Lcom/android/",
                "Lorg/",
                "Ldalvik/",
                "Llibcore/"
            };
            
            for (const auto& prefix : system_prefixes) {
                if (class_name.starts_with(prefix)) {
                    return true;
                }
            }
            return false;
        }

    // Normaliza assinatura de método para comparação
    std::string XrefSearchEngine::normalize_method_signature(const std::string& signature) {
            // Remove espaços extras, normaliza para formato canônico
            std::string result = signature;
            // Remover espaços entre tokens
            result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
            return result;
    }

    void XrefSearchEngine::update_context(const std::string& trimmed_line, ParseContext& ctx) {
        if (trimmed_line.find(".class ") == 0) {
            std::string class_name = extract_class_name(trimmed_line);
            ctx.current_class = class_name;
        } else if (trimmed_line.find(".method ") == 0) {
            ctx.current_method = extract_method_signature(trimmed_line);
            ctx.current_method_signature = ctx.current_method;
        } else if (trimmed_line == ".end method") {
            ctx.current_method.clear();
            ctx.current_method_signature.clear();
        }
        ctx.line_number++;
    }

    std::vector<SearchResult> XrefSearchEngine::search(
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

        // Executar busca medindo tempo
        auto [search_time, elapsed] = measure_execution([&]() {
            std::vector<XrefResult> xref_results;
            
            const std::string& target = config.query;
            
            // Coletar todos os arquivos .smali
            auto options = std::filesystem::directory_options::skip_permission_denied;
            std::vector<std::filesystem::path> all_files;
            
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

            // Processar arquivos em paralelo
            std::for_each(std::execution::par_unseq, all_files.begin(), all_files.end(),
                [&](const std::filesystem::path& file_path) {
                    std::ifstream file(file_path);
                    if (!file.is_open()) return;

                    std::string line;
                    ParseContext ctx;
                    std::vector<XrefResult> local_results;

                    while (std::getline(file, line)) {
                        // Trim da linha antes de atualizar contexto (bug fix: raw line passada antes)
                        size_t first = line.find_first_not_of(" \t\r\n");
                        std::string trimmed = (first != std::string::npos)
                            ? line.substr(first, line.find_last_not_of(" \t\r\n") - first + 1)
                            : line;

                        // Atualizar contexto com linha trimada
                        update_context(trimmed, ctx);

                        // Verificar se a linha original contém o target
                        if (contains_target(line, target)) {
                            // Determinar tipo de referência baseado na direção configurada
                            std::string xref_type = "reference";
                            
                            if (direction_ == "callers" || direction_ == "both") {
                                // Se estamos buscando callers de um método, a linha atual contém uma chamada para o target
                                // O contexto atual (ctx.current_method) é o caller
                                if (!ctx.current_method.empty()) {
                                    xref_type = "caller";
                                }
                            }
                            
                            if (direction_ == "callees" || direction_ == "both") {
                                // Se estamos buscando callees de um método, a linha atual mostra o que o método atual chama
                                // O target é o callee
                                if (!ctx.current_method.empty()) {
                                    xref_type = "callee";
                                }
                            }

                            // Filtrar classes do sistema se necessário
                            if (!include_system_ && is_system_class(ctx.current_class)) {
                                continue;
                            }

                            XrefResult result;
                            result.file_path = file_path;
                            result.caller_class = ctx.current_class;
                            result.caller_method = ctx.current_method;
                            result.line_number = ctx.line_number;
                            result.instruction = line;
                            result.target_class = target;
                            result.target_member = ""; // Pode ser extraído se necessário
                            result.xref_type = xref_type;
                            result.engine_name = this->name();
                            result.search_time = std::chrono::microseconds(0);
                            
                            local_results.push_back(std::move(result));
                        }
                    }

                    if (!local_results.empty()) {
                        std::lock_guard<std::mutex> lock(mtx);
                        for (auto& r : local_results) {
                            if (xref_results.size() < config.max_results) {
                                xref_results.push_back(std::move(r));
                            } else {
                                break;
                            }
                        }
                    }
                });

            // Se profundidade > 1, realizar busca recursiva
            if (depth_ > 1) {
                // Coletar todos os targets únicos encontrados na primeira passada
                std::set<std::string> unique_targets;
                for (const auto& result : xref_results) {
                    // Extrair método ou classe referenciada
                    // Para simplificar, usamos o próprio target original
                    // Em implementação completa, extrairíamos da instrução
                }
                
                // TODO: Implementar XREF recursivo se necessário
                // Por enquanto, retornamos apenas resultados diretos
            }

            // Converter XrefResult para SearchResult base
            for (const auto& xr : xref_results) {
                SearchResult sr;
                sr.file_path = xr.file_path;
                sr.line_number = xr.line_number;
                sr.line_content = xr.instruction;
                sr.context = xr.caller_class + "->" + xr.caller_method + " [" + xr.xref_type + "]";
                sr.engine_name = xr.engine_name;
                sr.search_time = xr.search_time;
                results.push_back(sr);
            }

            stats_.files_scanned = total_files;
            // BUG FIX: stats atualizadas com base nos xref_results antes da conversão
            stats_.matches_found = xref_results.size();

            return 0;
        });

        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        return results;
    }


    std::unique_ptr<ISearchEngine> create_xref_search_engine() {
        return std::make_unique<XrefSearchEngine>();
    }

} // namespace engines