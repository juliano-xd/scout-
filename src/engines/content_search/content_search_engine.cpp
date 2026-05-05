#include <chrono>
#include <algorithm>
#include <mutex>
#include <regex>
#include <format>       // C++20/26: Formatação in-place O(N)
#include <execution>    // C++17/20: std::execution::par_unseq
#include <system_error> // C++11: Para capturar falhas de I/O no RAII
#include <atomic>       // C++11: Atomic bool para Short-Circuit lock-free paralelo

#include "../../../include/engines/content_search/content_search_engine.hpp"
#include "../../../include/core/scanner.hpp"
#include "../../../include/utils/string_utils.hpp"
#include "../../../include/utils/sexpr.hpp" // Necessário para formatação da AST


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
    } // anonymous namespace

    // Removido 'noexcept' para coincidir com a declaração no .hpp
    void ContentSearchEngine::update_context(std::string_view line, ParseContext& ctx) {
        const std::string_view trimmed = utils::trim(line);
        if (trimmed.empty()) return;

        if (trimmed.starts_with(".class ")) {
            const size_t class_start = trimmed.find_last_of(' ');
            if (class_start != std::string_view::npos) {
                ctx.current_class = std::string(trimmed.substr(class_start + 1));
            }
        }
        else if (trimmed.starts_with(".method ")) {
            ctx.current_method = std::string(trimmed.substr(8));
        }
        else if (trimmed == ".end method") {
            ctx.current_method.clear();
        }

        ctx.line_number++;
    }

    // Removido 'noexcept' para coincidir com a declaração no .hpp
    bool ContentSearchEngine::matches_string(std::string_view line, std::string_view query, bool case_sensitive) {
        if (case_sensitive) {
            return line.find(query) != std::string_view::npos;
        } else {
            return utils::contains_insensitive(line, query);
        }
    }

    bool ContentSearchEngine::matches_regex(std::string_view line, const std::regex& pattern) {
        return std::regex_search(line.begin(), line.end(), pattern);
    }

    // Removido 'noexcept' para coincidir com a declaração no .hpp
    bool ContentSearchEngine::matches_integer(std::string_view line, std::string_view query) {
        std::string_view normalized_query = query;
        if (normalized_query.starts_with("0x")) {
            normalized_query.remove_prefix(2); // Operação O(1) Zero-Copy
        }

        size_t pos = 0;
        while ((pos = line.find(normalized_query, pos)) != std::string_view::npos) {

            // Verifica o boundary esquerdo
            bool start_ok = (pos == 0 || !std::isalnum(static_cast<unsigned char>(line[pos - 1])));

            // Caso especial: se o prefixo for 'x' e antes tiver '0', garante que "0x" está isolado
            if (!start_ok && pos >= 2 && line[pos - 1] == 'x' && line[pos - 2] == '0') {
                start_ok = (pos == 2 || !std::isalnum(static_cast<unsigned char>(line[pos - 3])));
            }

            if (start_ok) {
                // Verifica o boundary direito
                const size_t end = pos + normalized_query.size();
                const bool end_ok = (end == line.size() || !std::isalnum(static_cast<unsigned char>(line[end])));

                if (end_ok) return true;
            }
            pos++;
        }
        return false;
    }

    std::vector<SearchResult> ContentSearchEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        const auto& root_dir = ctx.root_dir();
        std::vector<SearchResult> final_results;

        if (config.query.empty() || !std::filesystem::exists(root_dir)) {
            return final_results;
        }

        // Compila o regex uma única vez (se aplicável)
        std::optional<std::regex> regex_pattern;
        if (config.search_type == "regex") {
            try {
                auto flags = std::regex::ECMAScript;
                if (!config.case_sensitive) flags |= std::regex::icase;

                regex_pattern = std::regex(std::string(config.query), flags);
            } catch (const std::regex_error&) {
                return final_results; // Query de regex inválida/malformada
            }
        }

        std::vector<std::filesystem::path> all_files;
        all_files.reserve(10000); // Mitigação proativa de realocações O(N) no vector

        std::error_code ec;
        auto options = std::filesystem::directory_options::skip_permission_denied;
        auto fs_it = std::filesystem::recursive_directory_iterator(root_dir, options, ec);
        auto fs_end = std::filesystem::recursive_directory_iterator();

        // Iterador no-throw de segurança máxima contra crashes de FS
        while (fs_it != fs_end && !ec) {
            if (fs_it->is_regular_file(ec) && !ec && fs_it->path().extension() == ".smali") {
                if (core::is_path_filtered(fs_it->path(), config.include_dirs, config.exclude_dirs)) {
                    all_files.push_back(fs_it->path());
                }
            }
            fs_it.increment(ec);
            if (ec) ec.clear();
        }

        std::mutex mtx;

        // OPTIMIZAÇÃO: Cancelamento Atómico Paralelo (Short-Circuit Lock-Free)
        std::atomic<bool> limit_reached{false};
        const size_t limit = static_cast<size_t>(config.max_results);

        auto [_, elapsed] = measure_execution([&]() {

            // C++17/20 par_unseq: Vetorização agressiva multi-thread
            std::for_each(std::execution::par_unseq, all_files.begin(), all_files.end(),
                [&](const std::filesystem::path& file_path) {

                    // EARLY OUT 1: Evita instanciar o MappedFile e alocar RAM se já acabámos
                    if (limit_reached.load(std::memory_order_relaxed)) return;

                    try {
                        // RAII Strict mode: Se o ficheiro não estiver disponível, lança system_error
                        utils::MappedFile mfile(file_path);
                        if (mfile.is_empty()) return;

                        const std::string_view data = mfile.view();

                        // Fast-path: Se for uma busca literal case-sensitive, procura diretamente no bloco de bytes
                        // O(N) ultra rápido contornando o LineIterator se não houver *hit* possível.
                        if (config.search_type == "string" && config.case_sensitive) {
                            if (data.find(config.query) == std::string_view::npos) return;
                        }

                        utils::LineIterator it(data);
                        std::string_view line;
                        ParseContext parse_ctx;

                        std::vector<SearchResult> local_results;
                        local_results.reserve(8); // Pool de reserva pequena por ficheiro

                        while (it.next(line)) {
                            // EARLY OUT 2: O ficheiro pode ter 50,000 linhas. Aborta a meio se as
                            // outras threads já preencheram a quota.
                            if (limit_reached.load(std::memory_order_relaxed)) break;

                            update_context(line, parse_ctx);

                            bool matched = false;
                            if (config.search_type == "regex" && regex_pattern) {
                                matched = matches_regex(line, *regex_pattern);
                            }
                            else if (config.search_type == "integer") {
                                matched = matches_integer(line, config.query);
                            }
                            else {
                                matched = matches_string(line, config.query, config.case_sensitive);
                            }

                            if (matched) {
                                const std::string_view trimmed_line = utils::trim(line);
                                SearchResult result;
                                result.file_path = std::filesystem::relative(file_path, root_dir);
                                result.line_number = parse_ctx.line_number;
                                result.line_content = std::string(trimmed_line);

                                // Construção Segura do Contexto em S-Expression via C++20 format
                                auto context_node = sexpr::form("match-context");

                                if (parse_ctx.current_method.empty()) {
                                    context_node.kv("scope", sexpr::string(std::format("class:{}", parse_ctx.current_class)));
                                } else {
                                    context_node.kv("scope", sexpr::string(std::format("method:{}", parse_ctx.current_method)));
                                }

                                // Parsing de constantes heurístico O(1) memory
                                if (trimmed_line.find("const") != std::string_view::npos) {
                                    const size_t reg_pos = trimmed_line.find(' ');
                                    if (reg_pos != std::string_view::npos) {
                                        const size_t comma_pos = trimmed_line.find(',', reg_pos);
                                        if (comma_pos != std::string_view::npos) {
                                            const std::string_view reg = utils::trim(trimmed_line.substr(reg_pos + 1, comma_pos - reg_pos - 1));
                                            const std::string_view val = utils::trim(trimmed_line.substr(comma_pos + 1));

                                            context_node.kv("const_reg", sexpr::string(reg));
                                            context_node.kv("const_val", sexpr::string(val));
                                        }
                                    }
                                }

                                result.context = context_node.to_string();
                                result.engine_name = this->name();
                                local_results.push_back(std::move(result));
                            }
                        }

                        // Fusão Thread-Safe dos resultados locais no vector global
                        if (!local_results.empty()) {
                            std::lock_guard<std::mutex> lock(mtx);

                            // EARLY OUT 3: Double Check Locking Pattern garantindo inserção apenas necessária
                            if (limit_reached.load(std::memory_order_relaxed)) return;

                            for (auto& r : local_results) {
                                if (final_results.size() < limit) {
                                    final_results.push_back(std::move(r));

                                    // Sinaliza as outras threads para abortarem caso o limite feche
                                    if (final_results.size() >= limit) {
                                        limit_reached.store(true, std::memory_order_relaxed);
                                        break;
                                    }
                                } else {
                                    break;
                                }
                            }
                        }

                    } catch (const std::system_error&) {
                        // Tolera graciosamente interrupções de I/O em concorrência
                        return;
                    }
                });
            return 0; // Dummy return for measure_execution
        });

        stats_.files_scanned = all_files.size();
        stats_.matches_found = final_results.size();
        stats_.total_time = elapsed;

        return final_results;
    }

    std::unique_ptr<ISearchEngine> create_content_search_engine() {
        return std::make_unique<ContentSearchEngine>();
    }

} // namespace engines
