#include "../../../include/engines/cfg/cfg_engine.hpp"
#include "../../../include/core/analysis_context.hpp"
#include "../../../include/utils/string_utils.hpp"
#include "../../../include/utils/sexpr.hpp"
#include <vector>
#include <unordered_map>
#include <chrono>

namespace engines {

    CFGEngine::CFGEngine() {
        stats_.total_time = std::chrono::milliseconds(0);
    }

    std::vector<SearchResult> CFGEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<SearchResult> results;

        std::string_view query_view = config.query;
        size_t arrow = query_view.find("->");
        if (arrow == std::string_view::npos) return results;

        std::string_view class_name = query_view.substr(0, arrow);
        std::string_view method_sig = query_view.substr(arrow + 2);

        std::string_view content = ctx.get_class_content(class_name);
        if (content.empty()) return results;

        size_t m_pos = content.find(method_sig);
        if (m_pos == std::string_view::npos) return results;

        size_t m_start = content.rfind(".method ", m_pos);
        size_t m_end = content.find(".end method", m_pos);
        if (m_start == std::string_view::npos || m_end == std::string_view::npos) return results;

        std::string_view body = content.substr(m_start, m_end - m_start);
        CFG cfg = build_cfg(body);

        SearchResult res;
        res.engine_name = name();
        res.file_path = std::string(class_name);

        auto root = sexpr::list();
        root.push(sexpr::symbol("cfg-report"));
        root.kv("method", sexpr::string(std::string(config.query)));

        auto blocks_node = sexpr::list();
        for (const auto& b : cfg.blocks) {
            auto b_node = sexpr::list();
            b_node.push(sexpr::symbol("BB" + std::to_string(b.id)));

            // Extrair instruções relevantes (opcodes)
            auto ops_node = sexpr::list();
            utils::LineIterator it(b.code_content);
            std::string_view line;
            while (it.next(line)) {
                std::string_view trimmed = utils::trim(line);
                if (trimmed.empty() || trimmed[0] == '.' || trimmed[0] == ':') continue;
                ops_node.push(sexpr::string(std::string(trimmed)));
            }
            b_node.kv("ops", ops_node);

            auto succs = sexpr::list();
            for (int s : b.successors) succs.push(sexpr::symbol("BB" + std::to_string(s)));
            b_node.kv("succ", succs);
            blocks_node.push(b_node);
        }
        root.kv("blocks", blocks_node);
        res.line_content = root.to_string();
        res.context = res.line_content;
        results.push_back(std::move(res));

        auto end = std::chrono::high_resolution_clock::now();
        stats_.total_time += std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        stats_.matches_found = 1;

        return results;
    }

    CFG CFGEngine::build_cfg(std::string_view body) {
        CFG cfg;
        if (body.empty()) return cfg;

        std::vector<std::string_view> lines;
        std::vector<size_t> line_start_pos;
        std::unordered_map<std::string_view, int> label_to_line;

        // [C22] Catch clauses extraídas antes de construir o CFG.
        struct CatchClause {
            std::string_view type;
            std::string_view from_label;
            std::string_view to_label;
            std::string_view handler_label;
        };
        std::vector<CatchClause> catch_clauses;

        size_t pos = 0;
        while (pos < body.size()) {
            size_t next = body.find('\n', pos);
            if (next == std::string_view::npos) next = body.size();
            std::string_view raw_line = body.substr(pos, next - pos);
            std::string_view line = utils::trim(raw_line);
            if (!line.empty()) {
                if (line[0] == ':') {
                    label_to_line[line] = lines.size();
                    lines.push_back(line);
                    line_start_pos.push_back(pos);
                } else if (line.starts_with(".catch ") || line.starts_with(".catchall ")) {
                    // Extrai cláusula catch: .catch Type {from .. to} handler
                    const size_t brace_open = line.find('{');
                    const size_t brace_close = line.find('}', brace_open);
                    const size_t ddot = line.find(" .. ", brace_open);
                    if (brace_open != std::string_view::npos && brace_close != std::string_view::npos
                        && ddot != std::string_view::npos)
                    {
                        CatchClause cc;
                        if (line.starts_with(".catch ")) {
                            const size_t t_start = 7; // after ".catch "
                            const size_t t_end = line.find(" {", t_start);
                            cc.type = utils::trim(line.substr(t_start, t_end - t_start));
                        } else {
                            cc.type = ""; // catchall
                        }
                        cc.from_label = utils::trim(line.substr(brace_open + 1, ddot - brace_open - 1));
                        cc.to_label   = utils::trim(line.substr(ddot + 4, brace_close - ddot - 4));
                        cc.handler_label = utils::trim(line.substr(brace_close + 1));
                        catch_clauses.push_back(cc);
                    }
                } else {
                    lines.push_back(line);
                    line_start_pos.push_back(pos);
                }
            }
            pos = next + 1;
        }

        if (lines.empty()) return cfg;

        std::vector<bool> is_leader(lines.size(), false);
        is_leader[0] = true;

        for (size_t i = 0; i < lines.size(); ++i) {
            std::string_view line = lines[i];
            if (line.starts_with("if-") || line.starts_with("goto") || line.starts_with("return") || line.starts_with("throw")
                || line.starts_with("packed-switch") || line.starts_with("sparse-switch")) {
                if (i + 1 < lines.size()) is_leader[i + 1] = true;
                size_t colon = line.find_last_of(':');
                if (colon != std::string_view::npos
                    && !line.starts_with("packed-switch") && !line.starts_with("sparse-switch")) {
                    std::string_view target = line.substr(colon);
                    if (label_to_line.count(target)) is_leader[label_to_line[target]] = true;
                }
            }
        }

        std::vector<int> line_to_block(lines.size(), -1);
        std::vector<size_t> block_start_line;

        for (size_t i = 0; i < lines.size(); ++i) {
            if (is_leader[i]) {
                block_start_line.push_back(i);
                BasicBlock bb;
                bb.id = cfg.blocks.size();
                cfg.blocks.push_back(bb);
            }
            line_to_block[i] = cfg.blocks.size() - 1;
        }

        for (size_t i = 0; i < cfg.blocks.size(); ++i) {
            size_t start_line = block_start_line[i];
            size_t end_line = (i + 1 < cfg.blocks.size()) ? block_start_line[i + 1] : lines.size();
            size_t b_start = line_start_pos[start_line];
            size_t b_end = (end_line < lines.size()) ? line_start_pos[end_line] : body.size();
            cfg.blocks[i].code_content = body.substr(b_start, b_end - b_start);

            std::string_view last_line = lines[end_line - 1];
            if (last_line.starts_with("goto")) {
                size_t colon = last_line.find(':');
                if (colon != std::string_view::npos) {
                    std::string_view target = utils::trim(last_line.substr(colon));
                    if (label_to_line.count(target)) {
                        int succ_bid = line_to_block[label_to_line[target]];
                        cfg.blocks[i].successors.push_back(succ_bid);
                        cfg.blocks[succ_bid].predecessors.push_back(i);
                    }
                }
            } else if (last_line.starts_with("if-")) {
                if (i + 1 < cfg.blocks.size()) {
                    cfg.blocks[i].successors.push_back(i + 1);
                    cfg.blocks[i + 1].predecessors.push_back(i);
                }
                size_t colon = last_line.find(':');
                if (colon != std::string_view::npos) {
                    std::string_view target = utils::trim(last_line.substr(colon));
                    if (label_to_line.count(target)) {
                        int succ_bid = line_to_block[label_to_line[target]];
                        cfg.blocks[i].successors.push_back(succ_bid);
                        cfg.blocks[succ_bid].predecessors.push_back(i);
                    }
                }
            } else if (!last_line.starts_with("return") && !last_line.starts_with("throw") && !last_line.starts_with(".end method")) {
                if (i + 1 < cfg.blocks.size()) {
                    cfg.blocks[i].successors.push_back(i + 1);
                    cfg.blocks[i + 1].predecessors.push_back(i);
                }
            }
        }

        // [C22] Popula exception edges nos blocos dentro de cada cláusula catch.
        // Para cada bloco cujas linhas caem dentro do intervalo [from, to),
        // adiciona a exceção como handler. A prioridade é crescente por ordem
        // de aparecimento (primeiro catch listado = maior prioridade).
        int catch_priority = 0;
        for (const auto& cc : catch_clauses) {
            const auto from_it = label_to_line.find(cc.from_label);
            const auto to_it = label_to_line.find(cc.to_label);
            const auto handler_it = label_to_line.find(cc.handler_label);
            if (from_it == label_to_line.end() || to_it == label_to_line.end()
                || handler_it == label_to_line.end())
            {
                ++catch_priority;
                continue;
            }
            const int from_line = from_it->second;
            const int to_line = to_it->second;
            const int handler_bid = line_to_block[handler_it->second];
            for (size_t i = 0; i < cfg.blocks.size(); ++i) {
                const size_t bs = block_start_line[i];
                const size_t be = (i + 1 < cfg.blocks.size()) ? block_start_line[i + 1] : lines.size();
                // Bloco está dentro do try-range se começa em [from_line, to_line)
                if (static_cast<int>(bs) >= from_line && static_cast<int>(be) <= to_line) {
                    cfg.blocks[i].handlers.push_back(
                        {cc.type, handler_bid, catch_priority});
                    cfg.blocks[i].contains_peis = true;
                    cfg.blocks[i].is_in_try_scope = true;
                }
            }
            ++catch_priority;
        }

        return cfg;
    }

} // namespace engines
