#include "engines/smali_dump_engine.hpp"
#include "core/analysis_context.hpp"
#include "utils/string_utils.hpp"
#include "utils/sexpr.hpp"
#include <chrono>
#include <sstream>

namespace engines {

    SmaliDumpEngine::SmaliDumpEngine() {
        stats_.total_time = std::chrono::milliseconds(0);
    }

    static sexpr::Node parse_method(std::string_view method_block) {
        auto node = sexpr::form("method");
        std::istringstream iss{std::string(method_block)};
        std::string line;
        bool in_method = false;
        auto body = sexpr::list();

        while (std::getline(iss, line)) {
            std::string_view trimmed = utils::trim(line);
            if (trimmed.empty()) continue;

            if (trimmed.starts_with(".method ")) {
                node.kv("declaration", sexpr::string(std::string(trimmed.substr(8))));
                in_method = true;
            } else if (trimmed == ".end method") {
                break;
            } else if (in_method) {
                if (trimmed.starts_with(".")) {
                    // Directive like .registers or .param
                    size_t space = trimmed.find(' ');
                    std::string dir = std::string(trimmed.substr(1, space - 1));
                    std::string val = (space != std::string::npos) ? std::string(trimmed.substr(space + 1)) : "";
                    body.push(sexpr::form("directive").kv("type", sexpr::string(dir)).kv("value", sexpr::string(val)));
                } else {
                    body.push(sexpr::form("instruction").kv("text", sexpr::string(std::string(trimmed))));
                }
            }
        }
        node.kv("body", body);
        return node;
    }

    std::vector<SearchResult> SmaliDumpEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<SearchResult> results;

        std::string_view content = ctx.get_class_content(config.query);
        if (content.empty()) return results;

        auto root = sexpr::form("smali-ast");
        root.kv("class", sexpr::string(config.query));

        std::istringstream iss{std::string(content)};
        std::string line;
        std::vector<sexpr::Node> fields;
        std::vector<sexpr::Node> methods;

        while (std::getline(iss, line)) {
            std::string_view trimmed = utils::trim(line);
            if (trimmed.empty()) continue;

            if (trimmed.starts_with(".class ")) {
                root.kv("definition", sexpr::string(std::string(trimmed.substr(7))));
            } else if (trimmed.starts_with(".super ")) {
                root.kv("super", sexpr::string(std::string(trimmed.substr(7))));
            } else if (trimmed.starts_with(".source ")) {
                root.kv("source", sexpr::string(std::string(trimmed.substr(8))));
            } else if (trimmed.starts_with(".field ")) {
                fields.push_back(sexpr::string(std::string(trimmed.substr(7))));
            } else if (trimmed.starts_with(".method ")) {
                std::string method_block = line + "\n";
                while (std::getline(iss, line)) {
                    method_block += line + "\n";
                    if (utils::trim(line) == ".end method") break;
                }
                methods.push_back(parse_method(method_block));
            }
        }

        if (!fields.empty()) root.kv("fields", sexpr::list(fields));
        if (!methods.empty()) root.kv("methods", sexpr::list(methods));

        SearchResult res;
        res.engine_name = name();
        res.file_path = config.query;
        res.line_content = root.to_string(true); // Pretty print
        res.context = res.line_content;
        results.push_back(std::move(res));

        auto end = std::chrono::high_resolution_clock::now();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        stats_.matches_found = 1;

        return results;
    }

    std::unique_ptr<ISearchEngine> create_smali_dump_engine() {
        return std::make_unique<SmaliDumpEngine>();
    }

} // namespace engines
