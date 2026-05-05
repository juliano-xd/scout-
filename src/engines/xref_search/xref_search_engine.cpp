#include "../../../include/engines/xref_search/xref_search_engine.hpp"
#include "../../../include/core/scanner.hpp"
#include "../../../include/utils/string_utils.hpp"
#include "../../../include/utils/mmap_file.hpp" // Corrigido para mapped_file.hpp moderno

#include <chrono>
#include <algorithm>
#include <mutex>
#include <unordered_set> // C++11+: Substitui o lento std::set O(log N)
#include <format>        // C++20/26: Formatação in-place O(N)
#include <execution>     // C++17/20: std::execution::par_unseq
#include <system_error>  // C++11: Para capturar falhas de I/O no RAII

namespace engines {

    namespace {
        // Helper para medir tempo de execução com deduce type (C++14+)
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

        // Helper Non-Allocating para extrair a assinatura.
        // O(N) com retorno de std::string_view sobre a memória original (MMap).
        std::string_view extract_method_signature(std::string_view line) noexcept {
            constexpr std::string_view method_prefix = ".method ";
            const size_t pos = line.find(method_prefix);

            if (pos != std::string_view::npos) {
                std::string_view signature = line.substr(pos + method_prefix.length());
                const size_t paren_pos = signature.find('(');

                if (paren_pos != std::string_view::npos) {
                    const size_t last_space = signature.find_last_of(' ', paren_pos);
                    if (last_space != std::string_view::npos) {
                        signature = signature.substr(last_space + 1);
                    }
                }
                return signature;
            }
            return {};
        }
    } // anonymous namespace

    bool XrefSearchEngine::contains_target(std::string_view line, std::string_view target) noexcept {
        return line.find(target) != std::string_view::npos;
    }

    bool XrefSearchEngine::is_system_class(std::string_view class_name) noexcept {
        // Array constexpr instanciado em tempo de compilação
        static constexpr std::string_view system_prefixes[] = {
            "Landroid/", "Ljava/", "Ljavax/", "Lsun/",
            "Lcom/android/", "Lorg/", "Ldalvik/", "Llibcore/"
        };

        // Avaliação vectorizada C++20
        return std::ranges::any_of(system_prefixes, [class_name](std::string_view prefix) {
            return class_name.starts_with(prefix);
        });
    }

    void XrefSearchEngine::update_context(std::string_view trimmed_line, ParseContext& ctx) noexcept {
        if (trimmed_line.starts_with(".class ")) {
            const size_t pos = trimmed_line.find_last_of(' ');
            if (pos != std::string_view::npos) {
                ctx.current_class = trimmed_line.substr(pos + 1);
            }
        }
        else if (trimmed_line.starts_with(".method ")) {
            ctx.reset_method(); // Reseta o Ring Buffer de Taint Analysis

            const size_t paren = trimmed_line.find('(');
            if (paren != std::string_view::npos) {
                std::string_view sig = trimmed_line.substr(8); // Salta ".method "
                const size_t space = sig.find_last_of(' ', paren - 8);

                if (space != std::string_view::npos) {
                    ctx.current_method = sig.substr(space + 1);
                } else {
                    ctx.current_method = sig;
                }
            }
        }
        else if (trimmed_line == ".end method") {
            ctx.reset_method();
        }

        ctx.line_number++;
    }

    std::string_view XrefSearchEngine::extract_opcode(std::string_view line) noexcept {
        const std::string_view trimmed = utils::trim(line);
        const size_t space = trimmed.find(' ');
        if (space != std::string_view::npos) return trimmed.substr(0, space);
        return trimmed;
    }

    std::string_view XrefSearchEngine::extract_registers(std::string_view line) noexcept {
        const size_t start = line.find('{');
        const size_t end = line.find('}', start);
        if (start != std::string_view::npos && end != std::string_view::npos) {
            return line.substr(start, end - start + 1);
        }
        return {};
    }

    std::string_view XrefSearchEngine::extract_instruction_type(std::string_view opcode) noexcept {
        if (opcode.starts_with("invoke-")) return "invoke";
        if (opcode.starts_with("sget") || opcode.starts_with("iget")) return "read";
        if (opcode.starts_with("sput") || opcode.starts_with("iput")) return "write";
        return "other";
    }

    std::string XrefSearchEngine::normalize_method_signature(std::string_view signature) {
        return std::string(utils::trim(signature)); // Fallback simples
    }

    std::string XrefSearchEngine::trace_register_value(std::string_view reg, const ParseContext& ctx) {
        // Percorre o Ring Buffer matematicamente de trás para frente (O(K))
        const size_t max_items = std::min(ctx.history_head, ParseContext::k_history_size);

        for (size_t i = 1; i <= max_items; ++i) {
            // Aritmética modular resolve a posição física no array de 32 elementos
            const std::string_view line = ctx.recent_lines[(ctx.history_head - i) % ParseContext::k_history_size];

            if (line.find("const-string") != std::string_view::npos && line.find(reg) != std::string_view::npos) {
                const size_t quote_start = line.find('"');
                if (quote_start != std::string_view::npos) {
                    const size_t quote_end = line.find('"', quote_start + 1);
                    if (quote_end != std::string_view::npos) {
                        return std::string(line.substr(quote_start + 1, quote_end - quote_start - 1));
                    }
                }
            }
            if (line.find("const/") != std::string_view::npos && line.find(reg) != std::string_view::npos) {
                const size_t comma = line.find(',');
                if (comma != std::string_view::npos) {
                    return std::format("const:{}", utils::trim(line.substr(comma + 1)));
                }
            }
        }
        return "";
    }

    std::vector<SearchResult> XrefSearchEngine::perform_search(
        const std::vector<std::filesystem::path>& files,
        const std::string& target,
        const SearchConfig& config,
        const std::filesystem::path& root_dir
    ) {
        std::vector<SearchResult> results;
        std::mutex mtx;

        // Políticas paralelas de execução do C++17/20 maximizam o débito dos múltiplos cores.
        // Utilização de par_unseq permite vectorize-instructions internas se for possível.
        std::for_each(std::execution::par_unseq, files.begin(), files.end(),
            [&](const std::filesystem::path& file_path) {

                try {
                    // RAII: Lança std::system_error se falhar ao mapear.
                    utils::MappedFile mfile(file_path);

                    // Se o ficheiro tiver 0 bytes, ignoramos pacificamente.
                    if (mfile.is_empty()) return;

                    const std::string_view data = mfile.view();
                    if (data.find(target) == std::string_view::npos) return;

                    utils::LineIterator it(data);
                    std::string_view line;
                    ParseContext ctx;

                    std::vector<SearchResult> local_results;
                    local_results.reserve(16); // Mitiga pequenas realocações

                    while (it.next(line)) {
                        const std::string_view trimmed = utils::trim(line);

                        // A actualização reseta o Ring Buffer automaticamente no `.method`
                        update_context(trimmed, ctx);

                        if (enable_taint_) {
                            if (!trimmed.empty() && !trimmed.starts_with('.')) {
                                // Operação O(1) sem alocação dinâmica.
                                ctx.push_line(trimmed);
                            }
                        }

                        if (contains_target(line, target)) {
                            if (!include_system_ && is_system_class(ctx.current_class)) continue;

                            const std::string_view opcode = extract_opcode(trimmed);

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
                            res.file_path = std::filesystem::relative(file_path, root_dir);
                            res.line_number = ctx.line_number;
                            res.line_content = std::string(trimmed);

                            std::string_view xref_type = "reference";
                            if (!ctx.current_method.empty()) {
                                if (direction_ == "callers" || direction_ == "both") xref_type = "caller";
                                else if (direction_ == "callees") xref_type = "callee";
                            }

                            const std::string_view regs_str = extract_registers(trimmed);
                            const std::string_view access = extract_instruction_type(opcode);

                            // OPTIMIZAÇÃO: Substituição de N strings secundárias pelo motor de formatação
                            // directo std::format, reduzindo drásticamente alocações consecutivas.
                            std::string context_buf;
                            context_buf.reserve(256);
                            std::format_to(std::back_inserter(context_buf), "{}->{}", ctx.current_class, ctx.current_method);

                            if (!regs_str.empty()) std::format_to(std::back_inserter(context_buf), " regs:{}", regs_str);
                            std::format_to(std::back_inserter(context_buf), " type:{}", access);
                            if (!xref_type.empty()) std::format_to(std::back_inserter(context_buf), " [{}]", xref_type);

                            res.context = std::move(context_buf);

                            // Análise local do Fluxo de Dados (Taint Analysis)
                            if (enable_taint_ && !regs_str.empty()) {
                                const std::string_view inner_regs = regs_str.substr(1, regs_str.size() - 2);
                                size_t comma = 0, last = 0;

                                while ((comma = inner_regs.find(',', last)) != std::string_view::npos) {
                                    std::string val = trace_register_value(utils::trim(inner_regs.substr(last, comma - last)), ctx);
                                    if (!val.empty()) res.context = std::format("{} taint:{}", res.context, val);
                                    last = comma + 1;
                                }
                                std::string val = trace_register_value(utils::trim(inner_regs.substr(last)), ctx);
                                if (!val.empty()) res.context = std::format("{} taint:{}", res.context, val);
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
                } catch (const std::system_error&) {
                    // Trata silenciosamente ficheiros que não puderam ser abertos/mapeados
                    // (ex: erros de I/O momentâneos, ficheiro apagado durante a iteração)
                    return;
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

        std::string_view class_target = target;
        std::string_view method_target;

        const size_t arrow = class_target.find("->");
        if (arrow != std::string_view::npos) {
            method_target = class_target.substr(arrow + 2);
            class_target = class_target.substr(0, arrow);
        }

        // Recupera localização real do ficheiro da classe
        auto class_file_opt = core::find_class_file(root_dir, std::string(class_target));
        if (!class_file_opt) return results;

        try {
            // RAII Strict mode: captura erros do SO em tempo de execução
            utils::MappedFile mfile(*class_file_opt);
            if (mfile.is_empty()) return results;

            utils::LineIterator it(mfile.view());
            std::string_view line;
            bool in_target_block = method_target.empty();

            size_t line_num = 0;
            while (it.next(line)) {
                line_num++;
                const std::string_view trimmed = utils::trim(line);
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
                    const std::string_view opcode = extract_opcode(trimmed);

                    if (opcode.starts_with("invoke-") || opcode.starts_with("sget") ||
                        opcode.starts_with("sput")    || opcode.starts_with("iget") ||
                        opcode.starts_with("iput"))
                    {
                        const size_t last_space = trimmed.find_last_of(' ');
                        if (last_space != std::string_view::npos) {
                            const std::string_view callee = trimmed.substr(last_space + 1);

                            SearchResult res;
                            res.file_path = *class_file_opt;
                            res.line_number = line_num;
                            res.line_content = std::string(trimmed);
                            res.context = std::format("{} calls {}", target, callee);
                            res.engine_name = this->name();

                            results.push_back(std::move(res));
                        }
                    }
                }
            }
            return results;

        } catch (const std::system_error&) {
            return results; // Aborta e retorna o vector vazio em caso de erro no mmap
        }
    }

    std::vector<SearchResult> XrefSearchEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        auto root_dir = ctx.root_dir();

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
        all_files.reserve(10000); // Previne múltiplas realocações no vetor central

        std::error_code ec;
        auto options = std::filesystem::directory_options::skip_permission_denied;
        auto fs_it = std::filesystem::recursive_directory_iterator(root_dir, options, ec);
        auto fs_end = std::filesystem::recursive_directory_iterator();

        while (fs_it != fs_end && !ec) {
            if (fs_it->is_regular_file(ec) && !ec && fs_it->path().extension() == ".smali") {
                if (core::is_path_filtered(fs_it->path(), config.include_dirs, config.exclude_dirs)) {
                    all_files.push_back(fs_it->path());
                }
            }
            fs_it.increment(ec);
            if (ec) ec.clear();
        }

        // Migração de std::set para std::unordered_set.
        // Consultas e Inserções passam de O(log N) para O(1)
        std::unordered_set<std::string> targets_to_search;
        targets_to_search.insert(config.query);
        std::unordered_set<std::string> processed_targets;
        std::unordered_set<std::string> found_contexts;

        auto [_, elapsed] = measure_execution([&]() {
            for (int d = 0; d < depth_; ++d) {
                std::vector<SearchResult> depth_results;
                std::unordered_set<std::string> next_targets;

                for (const auto& current_target : targets_to_search) {
                    if (!processed_targets.insert(current_target).second) continue;

                    auto results = perform_search(all_files, current_target, config, root_dir);

                    for (auto& r : results) {
                        std::string context_id = std::format("{}:{}", r.file_path.string(), r.line_number);

                        if (!found_contexts.insert(context_id).second) continue;

                        if (d < depth_ - 1) {
                            const size_t arrow = r.context.find("->");
                            if (arrow != std::string::npos) {
                                const size_t first_space = r.context.find(' ', arrow);
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

                // Append optimizado C++11/C++20
                final_results.insert(
                    final_results.end(),
                    std::make_move_iterator(depth_results.begin()),
                    std::make_move_iterator(depth_results.end())
                );

                targets_to_search = std::move(next_targets);
                if (targets_to_search.empty()) break;
            }
            return 0; // Dummy
        });

        stats_.files_scanned = all_files.size();
        stats_.matches_found = final_results.size();
        stats_.total_time = elapsed;

        return final_results;
    }

    std::unique_ptr<ISearchEngine> create_xref_search_engine() {
        return std::make_unique<XrefSearchEngine>();
    }

} // namespace engines
