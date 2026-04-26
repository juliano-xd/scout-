#include "engines/deobf_engine.hpp"
#include "core/scanner.hpp"
#include "utils/mmap_file.hpp"
#include "utils/string_utils.hpp"
#include <regex>
#include <chrono>
#include <mutex>

namespace engines {

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
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<SearchResult> results;

        // Se a query for uma classe, fazemos análise estrutural de obfuscação
        if (config.query.starts_with('L') && config.query.ends_with(';')) {
            std::string_view content = ctx.get_class_content(config.query);
            if (!content.empty()) {
                size_t short_names = 0;
                size_t total_methods = 0;
                utils::LineIterator it(content);
                std::string_view line;
                while (it.next(line)) {
                    std::string_view trimmed = utils::trim(line);
                    if (trimmed.starts_with(".method ")) {
                        total_methods++;
                        size_t paren = trimmed.find('(');
                        if (paren != std::string_view::npos) {
                            size_t last_space = trimmed.rfind(' ', paren);
                            if (last_space != std::string_view::npos) {
                                std::string_view name = trimmed.substr(last_space + 1, paren - last_space - 1);
                                if (name.length() < 3 && name != "<init>" && name != "<clinit>") {
                                    short_names++;
                                }
                            }
                        }
                    }
                }

                bool is_obfuscated = false;
                if (total_methods > 0 && (static_cast<double>(short_names) / total_methods) > 0.5) {
                    is_obfuscated = true;
                }
                // Se o nome da classe em si for muito curto (descontando pacotes)
                size_t last_slash = config.query.find_last_of('/');
                std::string_view class_short_name = (last_slash != std::string_view::npos) ? 
                    config.query.substr(last_slash + 1) : config.query;
                if (class_short_name.length() < 5) is_obfuscated = true;

                SearchResult res;
                res.file_path = std::string(config.query);
                res.engine_name = name();
                auto node = sexpr::form("obfuscation-report");
                node.kv("class", sexpr::string(config.query));
                node.kv("obfuscated", sexpr::symbol(is_obfuscated ? "true" : "false"));
                node.kv("short_method_ratio", sexpr::string(std::to_string(total_methods > 0 ? (double)short_names/total_methods : 0)));
                res.line_content = node.to_string();
                res.context = res.line_content;
                results.push_back(std::move(res));
            }
        }

        // Continua com a busca por strings Base64 (comportamento original)
        auto& root_dir = ctx.root_dir();
        std::regex b64_reg("\"([A-Za-z0-9+/]{20,}=*)\"");
        // ... rest of implementation (reinserting original string scan)

        core::scan_files(root_dir, [&](const std::filesystem::path& path) {
            utils::MappedFile mmap(path.string());
            if (!mmap.is_open()) return;

            utils::LineIterator it(mmap.view());
            std::string_view line;
            size_t line_num = 0;
            
            while (it.next(line)) {
                line_num++;
                std::smatch match;
                std::string s_line(line);
                if (std::regex_search(s_line, match, b64_reg)) {
                    std::string match_str = match[1].str();
                    if (is_suspicious(match_str)) {
                        SearchResult res;
                        res.file_path = std::filesystem::relative(path, root_dir);
                        res.line_number = line_num;
                        res.line_content = std::string(line);
                        
                        std::string decoded = decode_simple(match_str);
                        std::string preview;
                        for (char c : decoded) {
                            if (isprint(c)) preview += c;
                            else preview += '.';
                        }
                        
                        double entropy = utils::calculate_entropy(match_str);
                        auto node = sexpr::form("deobf-result");
                        node.kv("preview", sexpr::string(preview));
                        node.kv("entropy", sexpr::string(std::to_string(entropy)));
                        if (entropy > 4.5) node.kv("alert", sexpr::string("high-entropy-potential-encryption"));
                        
                        res.context = node.to_string();
                        res.engine_name = this->name();
                        
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        if (results.size() < config.max_results) {
                            results.push_back(std::move(res));
                        }
                    }
                }
            }
        }, config.include_dirs, config.exclude_dirs);

        auto end = std::chrono::high_resolution_clock::now();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        stats_.matches_found = results.size();

        return results;
    }

    std::unique_ptr<ISearchEngine> create_deobf_engine() {
        return std::make_unique<DeobfEngine>();
    }

} // namespace engines
