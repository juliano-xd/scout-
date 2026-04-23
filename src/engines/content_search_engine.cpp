#include "engines/content_search_engine.hpp"
#include "utils/filesystem.hpp"
#include "utils/string_utils.hpp"
#include "utils/mmap_file.hpp"
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

    void ContentSearchEngine::update_context(std::string_view line, ParseContext& ctx) {
        std::string_view trimmed = utils::trim(line);
        if (trimmed.empty()) return;

        if (trimmed.starts_with(".class ")) {
            size_t class_start = trimmed.find_last_of(' ');
            if (class_start != std::string_view::npos) {
                ctx.current_class = std::string(trimmed.substr(class_start + 1));
            }
        } else if (trimmed.starts_with(".method ")) {
            ctx.current_method = std::string(trimmed.substr(8));
        } else if (trimmed == ".end method") {
            ctx.current_method.clear();
        }
        ctx.line_number++;
    }

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

    bool ContentSearchEngine::matches_integer(std::string_view line, std::string_view query) {
        std::string_view normalized_query = query;
        if (normalized_query.starts_with("0x")) {
            normalized_query = normalized_query.substr(2);
        }

        size_t pos = 0;
        while ((pos = line.find(normalized_query, pos)) != std::string_view::npos) {
            bool start_ok = (pos == 0 || !std::isalnum(static_cast<unsigned char>(line[pos-1])));
            if (start_ok) {
                size_t end = pos + normalized_query.size();
                bool end_ok = (end == line.size() || !std::isalnum(static_cast<unsigned char>(line[end])));
                if (end_ok) return true;
            }
            pos++;
        }
        return false;
    }

    std::vector<SearchResult> ContentSearchEngine::search(
        const std::filesystem::path& root_dir,
        const SearchConfig& config
    ) {
        std::vector<SearchResult> results;
        if (config.query.empty() || !std::filesystem::exists(root_dir)) return results;

        std::optional<std::regex> regex_pattern;
        if (config.search_type == "regex") {
            try {
                regex_pattern = std::regex(std::string(config.query), config.case_sensitive ? std::regex::ECMAScript : (std::regex::ECMAScript | std::regex::icase));
            } catch (...) { return results; }
        }

        std::vector<std::filesystem::path> all_files;
        auto options = std::filesystem::directory_options::skip_permission_denied;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root_dir, options)) {
            if (entry.is_regular_file() && entry.path().extension() == ".smali") {
                all_files.push_back(entry.path());
            }
        }

        std::mutex mtx;
        auto [search_dummy, elapsed] = measure_execution([&]() {
            std::for_each(std::execution::par, all_files.begin(), all_files.end(),
                [&](const std::filesystem::path& file_path) {
                    utils::MappedFile mfile(file_path);
                    if (!mfile.is_open()) return;

                    std::string_view data = mfile.view();
                    if (config.search_type == "string" && config.case_sensitive) {
                        if (data.find(config.query) == std::string_view::npos) return;
                    }

                    utils::LineIterator it(data);
                    std::string_view line;
                    ParseContext ctx;
                    std::vector<SearchResult> local_results;

                    while (it.next(line)) {
                        update_context(line, ctx);

                        bool matched = false;
                        if (config.search_type == "regex" && regex_pattern) {
                            matched = matches_regex(line, *regex_pattern);
                        } else if (config.search_type == "integer") {
                            matched = matches_integer(line, config.query);
                        } else {
                            matched = matches_string(line, config.query, config.case_sensitive);
                        }

                        if (matched) {
                            std::string_view trimmed_line = utils::trim(line);
                            SearchResult result;
                            result.file_path = file_path;
                            result.line_number = ctx.line_number;
                            result.line_content = std::string(trimmed_line);
                            result.context = ctx.current_method.empty() 
                                ? "class:" + ctx.current_class 
                                : "method:" + ctx.current_method;
                            result.engine_name = this->name();
                            local_results.push_back(std::move(result));
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
            return 0;
        });

        stats_.files_scanned = all_files.size();
        stats_.matches_found = results.size();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

        return results;
    }

    std::unique_ptr<ISearchEngine> create_content_search_engine() {
        return std::make_unique<ContentSearchEngine>();
    }

} // namespace engines