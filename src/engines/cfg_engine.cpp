#include "engines/cfg_engine.hpp"
#include "core/scanner.hpp"
#include "utils/filesystem.hpp"
#include "utils/string_utils.hpp"
#include "utils/mmap_file.hpp"
#include <chrono>
#include <sstream>
#include <set>

namespace engines {

    std::vector<BasicBlock> CFGEngine::analyze_method(const std::filesystem::path& file, const std::string& method_sig) {
        std::vector<BasicBlock> blocks;
        utils::MappedFile mfile(file);
        if (!mfile.is_open()) return blocks;

        utils::LineIterator it(mfile.view());
        std::string_view line;
        bool in_method = false;
        std::vector<std::string> method_lines;
        size_t start_ln = 0;
        size_t current_ln = 0;

        while (it.next(line)) {
            current_ln++;
            std::string_view trimmed = utils::trim(line);
            if (trimmed.starts_with(".method ") && trimmed.find(method_sig) != std::string_view::npos) {
                in_method = true;
                start_ln = current_ln;
                continue;
            }
            if (in_method) {
                if (trimmed == ".end method") {
                    in_method = false;
                    break;
                }
                method_lines.push_back(std::string(trimmed));
            }
        }

        if (method_lines.empty()) return blocks;

        // Identificar líderes (início de blocos básicos)
        std::set<size_t> leaders;
        leaders.insert(0); // Primeira instrução é líder

        for (size_t i = 0; i < method_lines.size(); ++i) {
            const std::string& l = method_lines[i];
            if (l.empty() || l[0] == '.') continue;

            if (l.starts_with("goto") || l.starts_with("if-") || l.starts_with("return") || l.starts_with("throw")) {
                // Instrução após um jump/return é líder
                if (i + 1 < method_lines.size()) leaders.insert(i + 1);
                
                // Alvo de um jump é líder (precisamos encontrar a label)
                if (l.find(':') != std::string::npos) {
                    size_t colon = l.find_last_of(':');
                    std::string label = l.substr(colon);
                    for (size_t j = 0; j < method_lines.size(); ++j) {
                        if (method_lines[j] == label) {
                            leaders.insert(j);
                            break;
                        }
                    }
                }
            }
            // Labels são líderes
            if (l.starts_with(":")) leaders.insert(i);
        }

        // Criar blocos
        std::vector<size_t> sorted_leaders(leaders.begin(), leaders.end());
        for (size_t i = 0; i < sorted_leaders.size(); ++i) {
            BasicBlock bb;
            bb.id = "BB" + std::to_string(i);
            bb.start_line = sorted_leaders[i];
            size_t end = (i + 1 < sorted_leaders.size()) ? sorted_leaders[i + 1] : method_lines.size();
            bb.end_line = end;

            for (size_t j = bb.start_line; j < end; ++j) {
                bb.instructions.push_back(method_lines[j]);
            }
            blocks.push_back(std::move(bb));
        }

        // Resolver sucessores
        for (size_t i = 0; i < blocks.size(); ++i) {
            if (blocks[i].instructions.empty()) continue;
            const std::string& last = blocks[i].instructions.back();

            if (last.starts_with("return") || last.starts_with("throw")) {
                continue; // Sem sucessores
            }

            if (last.starts_with("goto")) {
                // Apenas sucessor alvo
                size_t colon = last.find_last_of(':');
                std::string label = last.substr(colon);
                for (size_t j = 0; j < blocks.size(); ++j) {
                    if (!blocks[j].instructions.empty() && blocks[j].instructions[0] == label) {
                        blocks[i].successors.push_back(blocks[j].id);
                        break;
                    }
                }
            } else if (last.starts_with("if-")) {
                // Sucessor alvo + sucessor imediato
                size_t colon = last.find_last_of(':');
                std::string label = last.substr(colon);
                for (size_t j = 0; j < blocks.size(); ++j) {
                    if (!blocks[j].instructions.empty() && blocks[j].instructions[0] == label) {
                        blocks[i].successors.push_back(blocks[j].id);
                        break;
                    }
                }
                if (i + 1 < blocks.size()) blocks[i].successors.push_back(blocks[i + 1].id);
            } else {
                // Sucessor imediato
                if (i + 1 < blocks.size()) blocks[i].successors.push_back(blocks[i + 1].id);
            }
        }

        return blocks;
    }

    std::string CFGEngine::blocks_to_sexpr(const std::vector<BasicBlock>& blocks) {
        std::stringstream ss;
        ss << "(cfg (blocks ";
        for (const auto& b : blocks) {
            ss << "(block (id \"" << b.id << "\") (instructions ";
            for (const auto& ins : b.instructions) {
                if (ins.empty() || ins[0] == '.') continue;
                ss << "\"" << ins << "\" ";
            }
            ss << ") (successors ";
            for (const auto& s : b.successors) ss << "\"" << s << "\" ";
            ss << "))";
        }
        ss << "))";
        return ss.str();
    }

    std::vector<SearchResult> CFGEngine::search(
        const std::filesystem::path& root_dir,
        const SearchConfig& config
    ) {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<SearchResult> results;

        // 1. Identificar classe e método
        size_t arrow = config.query.find("->");
        if (arrow == std::string::npos) return results;

        std::string class_target = config.query.substr(0, arrow);
        std::string method_target = config.query.substr(arrow + 2);

        auto file = core::find_class_file(root_dir, class_target);
        if (!file) return results;

        auto blocks = analyze_method(*file, method_target);
        
        SearchResult res;
        res.file_path = *file;
        res.line_content = blocks_to_sexpr(blocks);
        res.context = config.query;
        res.engine_name = this->name();
        results.push_back(std::move(res));

        auto end = std::chrono::high_resolution_clock::now();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        stats_.matches_found = results.size();

        return results;
    }

    std::unique_ptr<ISearchEngine> create_cfg_engine() {
        return std::make_unique<CFGEngine>();
    }

} // namespace engines
