#include "engines/xref_search_engine.hpp"
#include "core/scanner.hpp"
#include "utils/filesystem.hpp"
#include "utils/string_utils.hpp"
#include "utils/mmap_file.hpp"
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
    bool XrefSearchEngine::contains_target(std::string_view line, std::string_view target) {
        return line.find(target) != std::string_view::npos;
    }

    // Verifica se uma classe é do sistema Android
    bool XrefSearchEngine::is_system_class(std::string_view class_name) {
        static const std::string_view system_prefixes[] = {
            "Landroid/", "Ljava/", "Ljavax/", "Lsun/",
            "Lcom/android/", "Lorg/", "Ldalvik/", "Llibcore/"
        };
        for (const auto& prefix : system_prefixes) {
            if (class_name.starts_with(prefix)) return true;
        }
        return false;
    }

    void XrefSearchEngine::update_context(std::string_view trimmed_line, ParseContext& ctx) {
        if (trimmed_line.find(".class ") != std::string_view::npos) {
            size_t pos = trimmed_line.find_last_of(' ');
            if (pos != std::string_view::npos) {
                ctx.current_class = std::string(trimmed_line.substr(pos + 1));
            }
        } else if (trimmed_line.find(".method ") != std::string_view::npos) {
            size_t paren = trimmed_line.find('(');
            if (paren != std::string_view::npos) {
                size_t method_start = trimmed_line.find(".method ");
                std::string_view sig = trimmed_line.substr(method_start + 8);
                size_t space = sig.find_last_of(' ', paren - (method_start + 8));
                if (space != std::string_view::npos) {
                    ctx.current_method = std::string(sig.substr(space + 1));
                } else {
                    ctx.current_method = std::string(sig);
                }
            }
        } else if (trimmed_line.find(".end method") != std::string_view::npos) {
            ctx.current_method.clear();
        }
        ctx.line_number++;
    }

    // Extrai o opcode de uma instrução (ex: invoke-virtual)
    std::string_view XrefSearchEngine::extract_opcode(std::string_view line) {
        std::string_view trimmed = utils::trim(line);
        size_t space = trimmed.find(' ');
        if (space != std::string_view::npos) return trimmed.substr(0, space);
        return trimmed;
    }

    // Extrai registradores de uma instrução (ex: {p0, v0})
    std::string_view extract_registers(std::string_view line) {
        size_t start = line.find('{');
        size_t end = line.find('}', start);
        if (start != std::string_view::npos && end != std::string_view::npos) {
            return line.substr(start, end - start + 1);
        }
        return "";
    }

    // Classifica o tipo de acesso baseado no opcode
    std::string classify_access(std::string_view opcode) {
        if (opcode.starts_with("invoke-")) return "invoke";
        if (opcode.starts_with("sget") || opcode.starts_with("iget")) return "read";
        if (opcode.starts_with("sput") || opcode.starts_with("iput")) return "write";
        return "other";
    }

    // Tenta rastrear o valor de um registrador voltando algumas linhas (taint lite)
    std::string trace_register_value(std::string_view reg, const std::vector<std::string>& history) {
        // Procurar de trás para frente no histórico
        for (auto it = history.rbegin(); it != history.rend(); ++it) {
            std::string_view line = *it;
            if (line.find("const-string") != std::string_view::npos && line.find(reg) != std::string_view::npos) {
                size_t quote_start = line.find('"');
                size_t quote_end = line.find('"', quote_start + 1);
                if (quote_start != std::string::npos && quote_end != std::string::npos) {
                    return std::string(line.substr(quote_start + 1, quote_end - quote_start - 1));
                }
            }
            if (line.find("const/") != std::string_view::npos && line.find(reg) != std::string_view::npos) {
                 size_t comma = line.find(',');
                 if (comma != std::string_view::npos) {
                     return "const:" + std::string(utils::trim(line.substr(comma + 1)));
                 }
            }
        }
        return "";
    }

    std::vector<SearchResult> XrefSearchEngine::perform_search(
        const std::vector<std::filesystem::path>& files,
        const std::string& target,
        const SearchConfig& config
    ) {
        std::vector<SearchResult> results;
        std::mutex mtx;

        std::for_each(std::execution::par, files.begin(), files.end(),
            [&](const std::filesystem::path& file_path) {
                utils::MappedFile mfile(file_path);
                if (!mfile.is_open()) return;

                std::string_view data = mfile.view();
                if (data.find(target) == std::string_view::npos) return;

                utils::LineIterator it(data);
                std::string_view line;
                ParseContext ctx;
                std::vector<SearchResult> local_results;

                while (it.next(line)) {
                    std::string_view trimmed = utils::trim(line);
                    update_context(trimmed, ctx);

                    // Manter histórico para taint
                    if (enable_taint_) {
                        if (trimmed.starts_with(".method ")) ctx.recent_lines.clear();
                        if (!trimmed.empty() && !trimmed.starts_with(".")) {
                            ctx.recent_lines.push_back(std::string(trimmed));
                            if (ctx.recent_lines.size() > 20) ctx.recent_lines.erase(ctx.recent_lines.begin());
                        }
                    }

                    if (contains_target(line, target)) {
                        if (!include_system_ && is_system_class(ctx.current_class)) continue;

                        std::string_view opcode = extract_opcode(trimmed);

                        // Filtro de opcode
                        if (!filter_opcodes_.empty()) {
                            bool found = false;
                            for (const auto& f : filter_opcodes_) {
                                if (opcode.find(f) != std::string_view::npos) {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) continue;
                        }

                        SearchResult res;
                        res.file_path = file_path;
                        res.line_number = ctx.line_number;
                        res.line_content = std::string(trimmed);
                        
                        std::string xref_type = "reference";
                        if (!ctx.current_method.empty()) {
                            if (direction_ == "callers" || direction_ == "both") xref_type = "caller";
                            else if (direction_ == "callees") xref_type = "callee";
                        }

                        std::string regs_str = std::string(extract_registers(trimmed));
                        std::string access = classify_access(opcode);

                        res.context = ctx.current_class + "->" + ctx.current_method;
                        if (!regs_str.empty()) res.context += " regs:" + regs_str;
                        res.context += " type:" + access;
                        if (!xref_type.empty()) res.context += " [" + xref_type + "]";

                        // Taint analysis
                        if (enable_taint_ && !regs_str.empty()) {
                            // Simplificado: pega o primeiro registrador ou todos em {v0, v1}
                            std::string_view inner_regs = regs_str.substr(1, regs_str.size() - 2);
                            size_t comma = 0, last = 0;
                            while ((comma = inner_regs.find(',', last)) != std::string_view::npos) {
                                std::string val = trace_register_value(utils::trim(inner_regs.substr(last, comma - last)), ctx.recent_lines);
                                if (!val.empty()) res.context += " taint:" + val;
                                last = comma + 1;
                            }
                            std::string val = trace_register_value(utils::trim(inner_regs.substr(last)), ctx.recent_lines);
                            if (!val.empty()) res.context += " taint:" + val;
                        }
                        
                        res.engine_name = this->name();
                        local_results.push_back(std::move(res));
                    }
                }

                if (!local_results.empty()) {
                    std::lock_guard<std::mutex> lock(mtx);
                    for (auto& r : local_results) {
                        if (results.size() < config.max_results) {
                            results.push_back(std::move(r));
                        } else break;
                    }
                }
            });

        return results;
    }


    std::vector<SearchResult> XrefSearchEngine::search_callees(
        const std::filesystem::path& root_dir,
        const std::string& target,
        const SearchConfig& config
    ) {
        std::vector<SearchResult> results;
        
        // 1. Identificar a classe alvo
        std::string class_target;
        std::string method_target;
        size_t arrow = target.find("->");
        if (arrow != std::string::npos) {
            class_target = target.substr(0, arrow);
            method_target = target.substr(arrow + 2);
        } else {
            class_target = target;
        }

        // 2. Encontrar o arquivo da classe
        auto class_file = core::find_class_file(root_dir, class_target);
        if (!class_file) return results;

        // 3. Abrir o arquivo e procurar callees
        utils::MappedFile mfile(*class_file);
        if (!mfile.is_open()) return results;

        utils::LineIterator it(mfile.view());
        std::string_view line;
        bool in_target_block = false;
        if (method_target.empty()) in_target_block = true; // Se for classe, procura no arquivo todo

        size_t line_num = 0;
        while (it.next(line)) {
            line_num++;
            std::string_view trimmed = utils::trim(line);
            if (trimmed.empty()) continue;

            if (!method_target.empty()) {
                if (trimmed.starts_with(".method ") && trimmed.find(method_target) != std::string_view::npos) {
                    in_target_block = true;
                    continue;
                }
                if (trimmed == ".end method") {
                    in_target_block = false;
                    continue;
                }
            }

            if (in_target_block) {
                // Procurar por instruções que chamam outros (callees)
                std::string_view opcode = extract_opcode(trimmed);
                if (opcode.starts_with("invoke-") || opcode.starts_with("sget") || 
                    opcode.starts_with("sput") || opcode.starts_with("iget") || opcode.starts_with("iput")) {
                    
                    // Extrair o alvo da instrução (costuma ser o último token)
                    size_t last_space = trimmed.find_last_of(' ');
                    if (last_space != std::string_view::npos) {
                        std::string_view callee = trimmed.substr(last_space + 1);
                        
                        SearchResult res;
                        res.file_path = *class_file;
                        res.line_number = line_num;
                        res.line_content = std::string(trimmed);
                        res.context = target + " calls " + std::string(callee);
                        res.engine_name = this->name();
                        results.push_back(std::move(res));
                    }
                }
            }
        }

        return results;
    }

    std::vector<SearchResult> XrefSearchEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        auto root_dir = ctx.root_dir();
        // Sincronizar configurações internas com o config recebido (apenas se fornecidos)
        if (config.direction != "both") direction_ = config.direction;
        if (config.include_system) include_system_ = config.include_system;
        if (config.search_depth > 1) depth_ = config.search_depth;
        if (!config.filter_opcodes.empty()) {
            filter_opcodes_ = config.filter_opcodes;
        }

        if (direction_ == "callees") {
            return search_callees(root_dir, config.query, config);
        }

        std::vector<SearchResult> final_results;
        if (config.query.empty() || !std::filesystem::exists(root_dir)) return final_results;

        std::vector<std::filesystem::path> all_files;
        auto options = std::filesystem::directory_options::skip_permission_denied;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root_dir, options)) {
            if (entry.is_regular_file() && entry.path().extension() == ".smali") {
                all_files.push_back(entry.path());
            }
        }

        std::set<std::string> targets_to_search;
        targets_to_search.insert(config.query);
        std::set<std::string> processed_targets;
        std::set<std::string> found_contexts; // Para evitar duplicatas em profundidades diferentes

        auto [search_dummy, elapsed] = measure_execution([&]() {
            for (int d = 0; d < depth_; ++d) {
                std::vector<SearchResult> depth_results;
                std::set<std::string> next_targets;

                for (const auto& current_target : targets_to_search) {
                    if (processed_targets.count(current_target)) continue;
                    processed_targets.insert(current_target);

                    auto results = perform_search(all_files, current_target, config);
                    for (auto& r : results) {
                        // Se o resultado já foi encontrado em uma profundidade anterior, ignora
                        std::string context_id = r.file_path.string() + ":" + std::to_string(r.line_number);
                        if (found_contexts.count(context_id)) continue;
                        found_contexts.insert(context_id);

                        // Identifica o novo alvo para a próxima profundidade (o caller atual)
                        if (d < depth_ - 1) {
                            // Extrai a classe e método atual como alvo
                            // O contexto é: "Class->Method regs:{...} [caller]"
                            // Queremos apenas "Class->Method"
                            size_t arrow = r.context.find("->");
                            if (arrow != std::string::npos) {
                                size_t first_space = r.context.find(' ', arrow);
                                std::string caller_sig;
                                if (first_space != std::string::npos) {
                                    caller_sig = r.context.substr(0, first_space);
                                } else {
                                    caller_sig = r.context;
                                }
                                if (!caller_sig.empty()) {
                                    next_targets.insert(caller_sig);
                                }
                            }
                        }
                        depth_results.push_back(std::move(r));
                    }
                }

                final_results.insert(final_results.end(), 
                                   std::make_move_iterator(depth_results.begin()), 
                                   std::make_move_iterator(depth_results.end()));
                
                targets_to_search = std::move(next_targets);
                if (targets_to_search.empty()) break;
            }
            return 0;
        });

        stats_.files_scanned = all_files.size();
        stats_.matches_found = final_results.size();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

        return final_results;
    }

    std::unique_ptr<ISearchEngine> create_xref_search_engine() {
        return std::make_unique<XrefSearchEngine>();
    }

} // namespace engines