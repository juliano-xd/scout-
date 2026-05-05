#include "../../../include/engines/smali_dump/smali_dump_engine.hpp"
#include "../../../include/core/analysis_context.hpp"
#include "../../../include/utils/string_utils.hpp"
#include "../../../include/utils/sexpr.hpp"
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

    static std::string translate_to_pseudo(std::string_view method_block) {
        std::stringstream ss;
        std::istringstream iss{std::string(method_block)};
        std::string line;
        while (std::getline(iss, line)) {
            std::string_view trimmed = utils::trim(line);
            if (trimmed.empty() || trimmed.starts_with(".") || trimmed.starts_with("#")) {
                if (trimmed.starts_with(".method ")) ss << "// " << trimmed << "\n";
                continue;
            }

            std::string s_line(trimmed);
            if (s_line.starts_with("const-string")) {
                size_t reg_pos = s_line.find(' ');
                size_t comma = s_line.find(',', reg_pos);
                if (reg_pos != std::string::npos && comma != std::string::npos) {
                    ss << utils::trim(s_line.substr(reg_pos + 1, comma - reg_pos - 1)) << " = " << utils::trim(s_line.substr(comma + 1)) << ";\n";
                    continue;
                }
            }
            if (s_line.starts_with("invoke-")) {
                size_t ob = s_line.find('{'), cb = s_line.find('}', ob);
                size_t arrow = s_line.find("->", cb);
                if (ob != std::string::npos && cb != std::string::npos && arrow != std::string::npos) {
                    std::string regs = s_line.substr(ob + 1, cb - ob - 1);
                    std::string method_call = s_line.substr(arrow + 2);
                    size_t last_slash = s_line.rfind('/', arrow);
                    std::string class_name = (last_slash != std::string::npos) ? s_line.substr(last_slash + 1, arrow - last_slash - 1) : "unknown";
                    ss << class_name << "." << method_call << "(" << regs << ");\n";
                    continue;
                }
            }
            if (s_line.starts_with("move-")) {
                size_t sp = s_line.find(' ');
                size_t comma = s_line.find(',', sp);
                if (sp != std::string_view::npos && comma != std::string_view::npos) {
                    ss << utils::trim(s_line.substr(sp + 1, comma - sp - 1)) << " = " << utils::trim(s_line.substr(comma + 1)) << ";\n";
                    continue;
                }
            }
            ss << s_line << ";\n";
        }
        return ss.str();
    }

    std::vector<SearchResult> SmaliDumpEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<SearchResult> results;

        std::string_view full_query = config.query;
        size_t arrow_pos = full_query.find("->");
        bool specific_method = (arrow_pos != std::string_view::npos);
        std::string_view class_name = specific_method ? full_query.substr(0, arrow_pos) : full_query;
        std::string_view method_sig = specific_method ? full_query.substr(arrow_pos + 2) : "";

        std::string_view content = ctx.get_class_content(class_name);
        if (content.empty()) return results;

        if (config.search_type == "translate") {
            std::string pseudo;
            std::istringstream iss{std::string(content)};
            std::string line;
            while (std::getline(iss, line)) {
                if (utils::trim(line).starts_with(".method ")) {
                    std::string method_block = line + "\n";
                    while (std::getline(iss, line)) {
                        method_block += line + "\n";
                        if (utils::trim(line) == ".end method") break;
                    }
                    if (!specific_method || method_block.find(method_sig) != std::string::npos) {
                        pseudo += translate_to_pseudo(method_block) + "\n";
                    }
                }
            }
            SearchResult res;
            res.engine_name = name();
            res.file_path = std::string(class_name);
            res.line_content = pseudo;
            results.push_back(std::move(res));
        } else {
            auto root = sexpr::form("smali-ast");
            root.kv("class", sexpr::string(std::string(class_name)));
            std::istringstream iss{std::string(content)};
            std::string line;
            std::vector<sexpr::Node> fields;
            std::vector<sexpr::Node> methods;

            while (std::getline(iss, line)) {
                std::string_view trimmed = utils::trim(line);
                if (trimmed.empty()) continue;
                if (trimmed.starts_with(".class ")) root.kv("definition", sexpr::string(std::string(trimmed.substr(7))));
                else if (trimmed.starts_with(".super ")) root.kv("super", sexpr::string(std::string(trimmed.substr(7))));
                else if (trimmed.starts_with(".field ")) fields.push_back(sexpr::string(std::string(trimmed.substr(7))));
                else if (trimmed.starts_with(".method ")) {
                    std::string method_block = line + "\n";
                    while (std::getline(iss, line)) {
                        method_block += line + "\n";
                        if (utils::trim(line) == ".end method") break;
                    }
                    if (!specific_method || method_block.find(method_sig) != std::string::npos) {
                        methods.push_back(parse_method(method_block));
                    }
                }
            }
            if (!fields.empty()) root.kv("fields", sexpr::list(fields));
            if (!methods.empty()) root.kv("methods", sexpr::list(methods));

            SearchResult res;
            res.engine_name = name();
            res.file_path = std::string(class_name);
            res.line_content = root.to_string(true);
            res.context = res.line_content;
            results.push_back(std::move(res));
        }

        auto end = std::chrono::high_resolution_clock::now();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        stats_.matches_found = results.size();
        return results;
    }

    std::unique_ptr<ISearchEngine> create_smali_dump_engine() {
        return std::make_unique<SmaliDumpEngine>();
    }

} // namespace engines
