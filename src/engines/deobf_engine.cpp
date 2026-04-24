#include "engines/deobf_engine.hpp"
#include "core/scanner.hpp"
#include "utils/mmap_file.hpp"
#include "utils/string_utils.hpp"
#include <regex>
#include <chrono>
#include <mutex>

namespace engines {

    std::vector<SearchResult> DeobfEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        auto& root_dir = ctx.root_dir();
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<SearchResult> results;

        // Regexp para detectar strings que parecem Base64 longo (comum em obfuscação)
        std::regex b64_reg("\"([A-Za-z0-9+/]{20,}=*)\"");

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
                    if (utils::is_base64(match_str)) {
                        SearchResult res;
                        res.file_path = std::filesystem::relative(path, root_dir);
                        res.line_number = line_num;
                        res.line_content = std::string(line);
                        
                        std::string decoded = utils::decode_base64(match_str);
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
