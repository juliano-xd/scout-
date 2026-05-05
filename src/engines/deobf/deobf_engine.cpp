#include "../../../include/engines/deobf/deobf_engine.hpp"
#include "../../../include/core/scanner.hpp"
#include "../../../include/utils/mmap_file.hpp" // Nome corrigido moderno
#include "../../../include/utils/string_utils.hpp"
#include "../../../include/utils/sexpr.hpp"

#include <chrono>
#include <mutex>
#include <system_error>
#include <format>
#include <cctype>

namespace engines {

    // Removido o 'noexcept' para coincidir com a declaração no cabeçalho (.hpp)
    bool DeobfEngine::is_suspicious(std::string_view str) {
        if (str.length() < 20) return false;
        return utils::is_base64(str);
    }

    std::string DeobfEngine::decode_simple(std::string_view str) {
        return utils::decode_base64(str);
    }

    std::vector<SearchResult> DeobfEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        const auto start = std::chrono::high_resolution_clock::now();
        std::vector<SearchResult> results;

        // ==========================================
        // Análise Estrutural (Class DNA / Obfuscation Ratio)
        // ==========================================
        if (config.query.starts_with('L') && config.query.ends_with(';')) {
            const std::string_view content = ctx.get_class_content(config.query);

            if (!content.empty()) {
                size_t short_names = 0;
                size_t total_methods = 0;

                utils::LineIterator it(content);
                std::string_view line;

                while (it.next(line)) {
                    const std::string_view trimmed = utils::trim(line);

                    if (trimmed.starts_with(".method ")) {
                        total_methods++;
                        const size_t paren = trimmed.find('(');

                        if (paren != std::string_view::npos) {
                            const size_t last_space = trimmed.rfind(' ', paren);
                            if (last_space != std::string_view::npos) {
                                const std::string_view name = trimmed.substr(last_space + 1, paren - last_space - 1);
                                if (name.length() < 3 && name != "<init>" && name != "<clinit>") {
                                    short_names++;
                                }
                            }
                        }
                    }
                }

                bool is_obfuscated = false;
                const double short_ratio = total_methods > 0 ? (static_cast<double>(short_names) / total_methods) : 0.0;

                if (total_methods > 0 && short_ratio > 0.5) {
                    is_obfuscated = true;
                }

                // Correção do -Wdangling-gsl:
                // Promovemos config.query a string_view PRIMEIRO.
                // O .substr() de um string_view não aloca memória temporária na Heap,
                // prevenindo a criação de ponteiros para dados destruídos.
                std::string_view class_short_name = config.query;
                const size_t last_slash = class_short_name.find_last_of('/');
                if (last_slash != std::string_view::npos) {
                    class_short_name = class_short_name.substr(last_slash + 1);
                }

                if (class_short_name.length() < 5) is_obfuscated = true;

                SearchResult res;
                res.file_path = std::string(config.query);
                res.engine_name = name();

                auto node = sexpr::form("obfuscation-report");
                node.kv("class", sexpr::string(config.query));
                node.kv("obfuscated", sexpr::symbol(is_obfuscated ? "true" : "false"));

                // C++20: Formatação consistente independente do Locale
                node.kv("short_method_ratio", sexpr::string(std::format("{:.4f}", short_ratio)));

                res.line_content = node.to_string();
                res.context = res.line_content;
                results.push_back(std::move(res));
            }
        }

        // ==========================================
        // Scan Contínuo de Strings Suspeitas (Deobf B64)
        // ==========================================
        const auto& root_dir = ctx.root_dir();
        std::mutex mtx; // Mutex local para a recolha de resultados

        // Scan assíncrono gerido pelo core do Scout++
        core::scan_files(root_dir, [&](const std::filesystem::path& path) {
            try {
                // RAII Mapped File: Captura erros OS nativamente
                utils::MappedFile mfile(path);
                if (mfile.is_empty()) return;

                utils::LineIterator it(mfile.view());
                std::string_view line;
                size_t line_num = 0;

                std::vector<SearchResult> local_results;
                local_results.reserve(8); // Batching thread-local para evitar contenção de mutex

                while (it.next(line)) {
                    line_num++;

                    // OPTIMIZAÇÃO EXTREMA: Parser Heurístico de Strings
                    // Erradicámos std::regex_search. Procuramos apenas fragmentos delimitados
                    // por aspas duplas, limitando o consumo a operações O(N) atómicas na View.
                    size_t pos = 0;
                    while ((pos = line.find('"', pos)) != std::string_view::npos) {
                        const size_t end_pos = line.find('"', pos + 1);
                        if (end_pos == std::string_view::npos) break; // Aspas não fechadas

                        const std::string_view match_str = line.substr(pos + 1, end_pos - pos - 1);

                        if (is_suspicious(match_str)) {
                            SearchResult res;
                            res.file_path = std::filesystem::relative(path, root_dir);
                            res.line_number = line_num;
                            res.line_content = std::string(line);

                            const std::string decoded = decode_simple(match_str);
                            std::string preview;
                            preview.reserve(decoded.size());

                            // Mitigação UB: static_cast para unsigned char em std::isprint
                            for (const char c : decoded) {
                                const auto uc = static_cast<unsigned char>(c);
                                if (std::isprint(uc)) {
                                    preview += static_cast<char>(uc);
                                } else {
                                    preview += '.';
                                }
                            }

                            const double entropy = utils::calculate_entropy(match_str);
                            auto node = sexpr::form("deobf-result");

                            node.kv("preview", sexpr::string(preview));
                            node.kv("entropy", sexpr::string(std::format("{:.4f}", entropy)));

                            if (entropy > 4.5) {
                                node.kv("alert", sexpr::string("high-entropy-potential-encryption"));
                            }

                            res.context = node.to_string();
                            res.engine_name = this->name();

                            local_results.push_back(std::move(res));
                        }
                        pos = end_pos + 1; // Avança para depois do término da string actual
                    }
                }

                // Fusão em lote (Thread-Safe)
                if (!local_results.empty()) {
                    std::lock_guard<std::mutex> lock(mtx);
                    for (auto& r : local_results) {
                        if (results.size() < static_cast<size_t>(config.max_results)) {
                            results.push_back(std::move(r));
                        } else break;
                    }
                }

            } catch (const std::system_error&) {
                // Ficheiro bloqueado ou inacessível no sistema de ficheiros
            }
        }, config.include_dirs, config.exclude_dirs);

        const auto end = std::chrono::high_resolution_clock::now();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        stats_.matches_found = results.size();

        // A contagem de files scanned estaria contida no core::scan_files, se aplicável.

        return results;
    }

    std::unique_ptr<ISearchEngine> create_deobf_engine() {
        return std::make_unique<DeobfEngine>();
    }

} // namespace engines
