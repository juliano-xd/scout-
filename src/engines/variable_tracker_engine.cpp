#include "engines/variable_tracker_engine.hpp"
#include "engines/cfg_engine.hpp"
#include "core/analysis_context.hpp"
#include "utils/string_utils.hpp"
#include "utils/mmap_file.hpp"
#include "utils/sexpr.hpp"
#include <algorithm>
#include <queue>
#include <unordered_set>

namespace engines {

    static const std::string_view REG_NAMES[] = {
        "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
        "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
        "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9", "p10", "p11", "p12", "p13", "p14", "p15"
    };

    static const std::unordered_set<std::string_view> SINKS = {
        "Landroid/util/Log;", "Ljava/io/OutputStream;", 
        "Ljava/net/HttpURLConnection;", "Landroid/database/sqlite/SQLiteDatabase;",
        "Ljava/lang/Runtime;->exec", "Landroid/telephony/SmsManager;->sendTextMessage",
        "Landroid/content/SharedPreferences$Editor;", "Ljava/io/FileOutputStream;"
    };

    static const std::unordered_set<std::string_view> SANITIZERS = {
        "Ljava/security/MessageDigest;->digest", "Ljavax/crypto/Cipher;->doFinal",
        "Ljava/util/zip/CRC32;->update", "Ljava/lang/String;->hashCode"
    };

    static const std::unordered_set<std::string_view> TRANSFORMS = {
        "Landroid/util/Base64;->encode", "Ljava/net/URLEncoder;->encode",
        "Ljava/lang/Integer;->toHexString", "Ljava/lang/String;->getBytes"
    };

    static bool is_PEI_local(std::string_view line) {
        if (line.starts_with("invoke-")) return true;
        if (line.starts_with("iget") || line.starts_with("iput")) return true;
        if (line.starts_with("aget") || line.starts_with("aput")) return true;
        if (line.starts_with("sget") || line.starts_with("sput")) return true;
        if (line.starts_with("div-") || line.starts_with("rem-")) return true;
        if (line.starts_with("check-cast") || line.starts_with("new-")) return true;
        if (line.starts_with("throw") || line.starts_with("monitor-")) return true;
        return false;
    }

    VariableTrackerEngine::VariableTrackerEngine() {
        stats_.total_time = std::chrono::milliseconds(0);
    }

    std::string_view VariableTrackerEngine::bit_to_reg_sv(int bit) {
        if (bit >= 0 && bit < 48) return REG_NAMES[bit];
        return "unk";
    }

    std::string_view VariableTrackerEngine::pool_string(std::string_view s) {
        auto it = string_pool_.find(std::string(s));
        if (it != string_pool_.end()) return *it;
        return *string_pool_.insert(std::string(s)).first;
    }

    bool VariableTrackerEngine::is_sanitizer(std::string_view target) {
        for (auto s : SANITIZERS) if (target.find(s) != std::string_view::npos) return true;
        return false;
    }

    bool VariableTrackerEngine::is_transform(std::string_view target) {
        for (auto t : TRANSFORMS) if (target.find(t) != std::string_view::npos) return true;
        return false;
    }

    bool VariableTrackerEngine::merge_states(VariableTrackerEngine::TrackingState& target, const VariableTrackerEngine::TrackingState& incoming) {
        bool changed = false;
        
        uint64_t old_regs = target.active_regs;
        target.active_regs |= incoming.active_regs;
        if (target.active_regs != old_regs) changed = true;

        for (const auto& [reg, fields] : incoming.obj_taint_map) {
            auto& t_fields = target.obj_taint_map[reg];
            size_t old_sz = t_fields.size();
            t_fields.insert(fields.begin(), fields.end());
            if (t_fields.size() > old_sz) changed = true;
        }

        if (target.control_taint_stack.size() < incoming.control_taint_stack.size()) {
            target.control_taint_stack = incoming.control_taint_stack;
            changed = true;
        }
        
        return changed;
    }

    std::vector<SearchResult> VariableTrackerEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<SearchResult> results;
        std::vector<VariableEvent> events;

        std::string target_query = config.query;
        std::string initial_reg_str = config.var_name;

        // Suporte a METHOD?0xVALUE ou METHOD:reg
        if (target_query.find('?') != std::string::npos) {
            size_t qmark = target_query.find('?');
            std::string value = target_query.substr(qmark + 1);
            target_query = target_query.substr(0, qmark);
            // Sobrescrevemos a query interna para o motor usar na busca de constantes
            const_cast<engines::SearchConfig&>(config).query = value; 
        }
        else if (initial_reg_str.empty() && target_query.find(':') != std::string::npos) {
            size_t colon = target_query.find_last_of(':');
            initial_reg_str = target_query.substr(colon + 1);
            target_query = target_query.substr(0, colon);
        }

        // Suporte a rastreio de constante (hex ou string)
        bool is_const_search = !config.query.empty();
        if (target_query.empty() || (initial_reg_str.empty() && !is_const_search)) return results;

        analysis_cache_.clear();
        string_pool_.clear();

        TrackingState state;
        state.current_method = pool_string(target_query);
        int bit = reg_to_bit(initial_reg_str);
        if (bit != -1 && !initial_reg_str.empty()) {
            state.active_regs |= (1ULL << bit);
        }
        // Se a query for hex, não precisamos de registrador inicial, o motor vai achar o const
        
        track_recursive(ctx, state, events, config);

        SearchResult res;
        res.engine_name = name();
        res.file_path = "aero_taint_analysis";
        
        auto root = sexpr::form("aero-taint-report");
        root.kv("start", sexpr::string(std::string(state.current_method)));
        
        auto timeline = sexpr::list();
        for (const auto& ev : events) {
            auto node = sexpr::form("ev");
            node.kv("m", sexpr::string(std::string(ev.method)));
            node.kv("l", sexpr::integer(ev.line));
            node.kv("r", sexpr::string(std::string(ev.reg)));
            node.kv("a", sexpr::string(std::string(ev.action)));
            node.kv("t", sexpr::string(std::string(ev.target)));
            if (!ev.extra.empty()) node.kv("e", sexpr::string(std::string(ev.extra)));
            timeline.push(node);
        }
        root.kv("flow", timeline);
        res.context = root.to_string();
        results.push_back(std::move(res));

        stats_.total_time += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time);
        stats_.matches_found = (int)events.size();
        return results;
    }

    MethodSummary VariableTrackerEngine::track_recursive(
        core::AnalysisContext& ctx,
        TrackingState& state,
        std::vector<VariableEvent>& events,
        const SearchConfig& config
    ) {
        if (state.depth > config.search_depth) return {};
        if (state.active_regs == 0 && state.obj_taint_map.empty() && state.control_taint_stack.empty() && config.query.empty()) return {};

        CacheKey key{state.current_method, state.active_regs};
        if (analysis_cache_.count(key)) {
            auto& cached = analysis_cache_[key];
            events.insert(events.end(), cached.first.begin(), cached.first.end());
            return cached.second;
        }

        size_t arrow = state.current_method.find("->");
        if (arrow == std::string_view::npos) return {};
        std::string_view class_name = state.current_method.substr(0, arrow);
        std::string_view method_sig = state.current_method.substr(arrow + 2);

        std::string_view content = ctx.get_class_content(class_name);
        if (content.empty()) return {};

        size_t m_pos = content.find(".method ");
        while (m_pos != std::string_view::npos) {
            size_t next_line = content.find('\n', m_pos);
            std::string_view header = content.substr(m_pos, next_line - m_pos);
            
            // Busca precisa da assinatura: deve ser precedida por espaço e 
            // opcionalmente conter modificadores, terminando exatamente na assinatura.
            // Para ser robusto: o nome do método deve estar lá e ser seguido por '('
            size_t sig_pos = header.find(method_sig);
            if (sig_pos != std::string_view::npos) {
                // Verifica se é um match exato (evita a() matching abc())
                // No Smali, a assinatura no cabeçalho é: [access] name(args)ret
                // Procuramos por: " name(args)ret"
                bool preceded_by_space = (sig_pos > 0 && (header[sig_pos-1] == ' ' || header[sig_pos-1] == '\t'));
                if (preceded_by_space) break;
            }
            m_pos = content.find(".method ", next_line);
        }
        if (m_pos == std::string_view::npos) return {};

        size_t start = content.rfind(".method ", m_pos);
        if (start == std::string_view::npos) return {};
        
        size_t end = content.find(".end method", m_pos);
        if (end == std::string_view::npos) return {};

        std::string_view body = content.substr(start, end - start);
        
        CFG cfg = CFGEngine::build_cfg(body);
        DominatorAnalyzer::compute_ipds(cfg);
        
        std::unordered_map<int, TrackingState> block_in_states;
        std::unordered_map<int, std::vector<VariableEvent>> block_events_map;
        std::unordered_set<int> visited_blocks;
        std::queue<int> worklist;
        
        block_in_states[cfg.entry_block_id] = state;
        worklist.push(cfg.entry_block_id);
        visited_blocks.insert(cfg.entry_block_id);
        
        MethodSummary final_summary;

        while (!worklist.empty()) {
            int bid = worklist.front(); worklist.pop();
            const auto& block = cfg.blocks[bid];

            TrackingState current_in = block_in_states[bid];

            while (!current_in.control_taint_stack.empty() && current_in.control_taint_stack.back() == bid) {
                current_in.control_taint_stack.pop_back();
            }

            TrackingState current_out = current_in;
            TrackingState exception_out; 
            std::vector<VariableEvent> block_events;

            process_method_internal(block.code_content, current_out, exception_out, block_events, ctx, config, state.current_method, state.depth);

            // Level 15: Check for tainted branch
            std::string_view trimmed_body = utils::trim(block.code_content);
            size_t last_nl = trimmed_body.find_last_of('\n');
            std::string_view last_line = (last_nl == std::string_view::npos) ? trimmed_body : utils::trim(trimmed_body.substr(last_nl + 1));
            if (last_line.starts_with("if-")) {
                bool branch_tainted = false;
                if (current_in.active_regs != 0) branch_tainted = true; 

                if (branch_tainted && block.ipd != -1) {
                    current_out.control_taint_stack.push_back(block.ipd);
                }
            }

            for (int sid : block.successors) {
                bool state_changed = merge_states(block_in_states[sid], current_out);
                if (state_changed || visited_blocks.find(sid) == visited_blocks.end()) {
                    visited_blocks.insert(sid);
                    worklist.push(sid);
                }
            }

            for (const auto& handler : block.handlers) {
                bool state_changed = merge_states(block_in_states[handler.target_id], exception_out);
                if (state_changed || visited_blocks.find(handler.target_id) == visited_blocks.end()) {
                    visited_blocks.insert(handler.target_id);
                    worklist.push(handler.target_id);
                }
            }

            block_events_map[bid] = std::move(block_events);
            if (block.successors.empty()) {
                final_summary.return_tainted |= current_out.last_call_summary.return_tainted;
                for (auto& f : current_out.last_call_summary.return_obj_fields) final_summary.return_obj_fields.insert(f);
            }
        }

        std::vector<VariableEvent> all_method_events;
        for (auto& [bid, b_events] : block_events_map) {
            all_method_events.insert(all_method_events.end(), b_events.begin(), b_events.end());
        }

        analysis_cache_[key] = {all_method_events, final_summary};
        events.insert(events.end(), all_method_events.begin(), all_method_events.end());
        return final_summary;
    }

    void VariableTrackerEngine::process_method_internal(
        std::string_view body,
        TrackingState& state,
        TrackingState& ex_out,
        std::vector<VariableEvent>& events,
        core::AnalysisContext& ctx,
        const SearchConfig& config,
        std::string_view method_name,
        int depth
    ) {
        size_t pos = 0; int line_idx = 0;
        state.last_call_summary = {};

        while (pos < body.size()) {
            size_t next = body.find('\n', pos);
            if (next == std::string_view::npos) next = body.size();
            std::string_view line = utils::trim(body.substr(pos, next - pos));
            pos = next + 1; line_idx++;

            // Ignorar comentários e diretivas
            if (line.empty() || line[0] == '.' || line[0] == '#') continue;

            // Remover comentários em linha (inline comments)
            size_t hash_pos = line.find('#');
            if (hash_pos != std::string_view::npos) {
                line = utils::trim(line.substr(0, hash_pos));
                if (line.empty()) continue;
            }

            if (is_PEI_local(line)) merge_states(ex_out, state);

            bool pc_tainted = !state.control_taint_stack.empty();

            if (line.starts_with("move") || line.starts_with("iget") || line.starts_with("aget") || line.starts_with("const")) {
                size_t sp = line.find(' '); if (sp == std::string_view::npos) continue;
                size_t comma = line.find(',', sp);
                std::string_view dst_r = utils::trim(line.substr(sp + 1, (comma == std::string_view::npos) ? std::string_view::npos : (comma - sp - 1)));
                int d_bit = reg_to_bit(dst_r);
                if (d_bit == -1) continue;

                bool data_tainted = false;
                std::string_view target = "const";
                std::string_view extra = "";
                
                // Suporte a rastreio de constante específica (Nível 16)
                if (line.starts_with("const")) {
                    size_t last_comma = line.find_last_of(',');
                    if (last_comma != std::string_view::npos) {
                        std::string_view val = utils::trim(line.substr(last_comma + 1));
                        std::string_view normalized_val = val;
                        if (normalized_val.starts_with('"') && normalized_val.ends_with('"')) {
                            normalized_val = normalized_val.substr(1, normalized_val.size() - 2);
                        }
                        
                        if (normalized_val == config.query || (config.query.starts_with("0x") && val == config.query)) {
                            data_tainted = true;
                            target = normalized_val;
                            extra = "CONST_SOURCE";
                        }
                    }
                }

                const char* act = "MOVE";
                if (line.starts_with("i")) act = "LOAD";
                else if (line.starts_with("a")) act = "LOAD";
                else if (line.starts_with("const")) act = "CONST";

                if (!line.starts_with("const") && comma != std::string_view::npos) {
                    std::string_view src_r = utils::trim(line.substr(comma + 1));
                    target = src_r;
                    int s_bit = reg_to_bit(src_r);
                    if (s_bit != -1 && (state.active_regs & (1ULL << s_bit))) {
                        data_tainted = true;
                        extra = src_r;
                    }
                    else if (line.starts_with("iget")) {
                        size_t first_comma = line.find(','); size_t last_comma = line.find_last_of(',');
                        if (first_comma != std::string_view::npos && last_comma != std::string_view::npos) {
                            std::string_view obj_r = utils::trim(line.substr(first_comma + 1, last_comma - first_comma - 1));
                            std::string_view field = utils::trim(line.substr(last_comma + 1));
                            int o_bit = reg_to_bit(obj_r);
                            if (o_bit != -1 && state.obj_taint_map.count(o_bit) && state.obj_taint_map[o_bit].count(field)) {
                                data_tainted = true; target = field; extra = obj_r;
                            }
                        }
                    }
                }

                if (data_tainted || pc_tainted) {
                    state.active_regs |= (1ULL << d_bit);
                    events.push_back({method_name, line_idx, bit_to_reg_sv(d_bit), act, target, pc_tainted ? "IMPLICIT" : extra});
                } else {
                    state.active_regs &= ~(1ULL << d_bit);
                    state.obj_taint_map.erase(d_bit);
                }
            }
            else if (line.starts_with("iput") || line.starts_with("sput") || line.starts_with("aput")) {
                size_t sp = line.find(' '); size_t comma = line.find(','); size_t l_comma = line.find_last_of(',');
                if (sp != std::string_view::npos && comma != std::string_view::npos && l_comma != std::string_view::npos) {
                    std::string_view src_r = utils::trim(line.substr(sp + 1, comma - sp - 1));
                    std::string_view obj_r = utils::trim(line.substr(comma + 1, l_comma - comma - 1));
                    std::string_view field = utils::trim(line.substr(l_comma + 1));
                    int s_bit = reg_to_bit(src_r), o_bit = reg_to_bit(obj_r);
                    if ((s_bit != -1 && (state.active_regs & (1ULL << s_bit))) || pc_tainted) {
                        if (o_bit != -1) {
                            state.obj_taint_map[o_bit].insert(field);
                            events.push_back({method_name, line_idx, bit_to_reg_sv(s_bit != -1 ? s_bit : 0), "STORE", field, obj_r});
                        }
                    }
                }
            }
            else if (line.starts_with("invoke-")) {
                size_t ob = line.find('{'), cb = line.find('}', ob);
                if (ob != std::string_view::npos && cb != std::string_view::npos) {
                    std::string_view target;
                    size_t last_space = line.find_last_of(' ');
                    if (last_space != std::string_view::npos) {
                        target = utils::trim(line.substr(last_space + 1));
                    }
                    // Se o target comecar com {, eh porque pegamos os registradores. Precisamos do token depois da virgula.
                    if (target.starts_with('{')) {
                        size_t comma = line.find('}', last_space);
                        if (comma != std::string_view::npos) {
                            size_t next_token = line.find_first_not_of(", ", comma + 1);
                            if (next_token != std::string_view::npos) {
                                target = utils::trim(line.substr(next_token));
                            }
                        }
                    }
                    std::string_view regs_sv = line.substr(ob + 1, cb - ob - 1);
                    bool is_sink = false; for (auto s : SINKS) if (target.find(s) != std::string_view::npos) { is_sink = true; break; }
                    bool is_san = is_sanitizer(target);
                    bool is_tr = is_transform(target);
                    bool is_prop = target.find("StringBuilder;->append") != std::string_view::npos || target.find(";-><init>") != std::string_view::npos || target.find(";->put") != std::string_view::npos;
                    
                    int call_bits[16]; int call_count = 0;
                    if (regs_sv.find(" .. ") != std::string_view::npos) {
                        size_t dd = regs_sv.find(" .. ");
                        int s_idx = reg_to_bit(utils::trim(regs_sv.substr(0, dd))), e_idx = reg_to_bit(utils::trim(regs_sv.substr(dd + 4)));
                        if (s_idx != -1 && e_idx != -1) for (int i = s_idx; i <= e_idx && call_count < 16; ++i) call_bits[call_count++] = i;
                    } else {
                        size_t r_pos = 0;
                        while (r_pos < regs_sv.size()) {
                            size_t r_comma = regs_sv.find(',', r_pos); if (r_comma == std::string_view::npos) r_comma = regs_sv.size();
                            int b = reg_to_bit(utils::trim(regs_sv.substr(r_pos, r_comma - r_pos)));
                            if (b != -1 && call_count < 16) call_bits[call_count++] = b;
                            r_pos = r_comma + 1;
                        }
                    }

                    for (int i = 0; i < call_count; ++i) {
                        bool tainted = (state.active_regs & (1ULL << call_bits[i])) || (state.obj_taint_map.count(call_bits[i]) && !state.obj_taint_map[call_bits[i]].empty()) || pc_tainted;
                        if (tainted) {
                            if (is_san && !pc_tainted) {
                                state.active_regs &= ~(1ULL << call_bits[i]); state.obj_taint_map.erase(call_bits[i]);
                                events.push_back({method_name, line_idx, bit_to_reg_sv(call_bits[i]), "SANITY", target, ""});
                                continue;
                            }
                            const char* act = is_sink ? "SINK_LEAK" : (is_tr ? "TRANSFORM" : "CALL");
                            events.push_back({method_name, line_idx, bit_to_reg_sv(call_bits[i]), act, target, pc_tainted ? "IMPLICIT" : ""});
                            
                            if (is_prop && i > 0) {
                                state.active_regs |= (1ULL << call_bits[0]);
                                if (state.obj_taint_map.count(call_bits[i])) { for (auto& f : state.obj_taint_map[call_bits[i]]) state.obj_taint_map[call_bits[0]].insert(f); }
                                events.push_back({method_name, line_idx, bit_to_reg_sv(call_bits[0]), "TAINT_PROP", target, ""});
                            }

                            TrackingState next_state; next_state.current_method = pool_string(target); next_state.depth = depth + 1; 
                            
                            // Nível 16: Se for reflexão ou código externo, aplicamos EPS/EES
                            if (target.find("java/lang/reflect/Method;->invoke") != std::string::npos || 
                                target.find("ClassLoader;->loadClass") != std::string::npos ||
                                target.find("DexClassLoader") != std::string::npos ||
                                target.find("PathClassLoader") != std::string::npos) {
                                state.last_call_summary.return_tainted = true; // Assumimos incerteza (EIT)
                                // Opaque Taint: Marcamos como influência externa
                                events.push_back({method_name, line_idx, bit_to_reg_sv(call_bits[i]), "EES_OPAQUE_ENTRY", target, "EXTERNAL_PAYLOAD_SHADOWING"});
                            } else {
                                next_state.active_regs = ((state.active_regs & (1ULL << call_bits[i])) || pc_tainted) ? (1ULL << (32 + i)) : 0;
                                if (state.obj_taint_map.count(call_bits[i])) next_state.obj_taint_map[32 + i] = state.obj_taint_map[call_bits[i]];
                                state.last_call_summary = track_recursive(ctx, next_state, events, config);
                            }
                        }
                    }
                }
            }
            else if (line.starts_with("move-result")) {
                int d_bit = reg_to_bit(utils::trim(line.substr(line.find(' ') + 1)));
                if (d_bit != -1) {
                    if (state.last_call_summary.return_tainted || pc_tainted) state.active_regs |= (1ULL << d_bit);
                    if (!state.last_call_summary.return_obj_fields.empty()) state.obj_taint_map[d_bit] = state.last_call_summary.return_obj_fields;
                }
                state.last_call_summary = {};
            }
            else if (line.starts_with("return")) {
                size_t sp = line.find(' ');
                if (sp != std::string_view::npos) {
                    std::string_view ret_r = utils::trim(line.substr(sp + 1));
                    int r_bit = reg_to_bit(ret_r);
                    if (r_bit != -1) {
                        state.last_call_summary.return_tainted = (state.active_regs & (1ULL << r_bit)) || pc_tainted;
                        if (state.obj_taint_map.count(r_bit)) state.last_call_summary.return_obj_fields = state.obj_taint_map[r_bit];
                    }
                }
            }
        }
    }

    bool VariableTrackerEngine::supports_config(const SearchConfig& config) const {
        return !config.var_name.empty() || config.query.find(':') != std::string::npos;
    }

    PointsToSet VariableTrackerEngine::get_points_to_set(core::AnalysisContext&, std::string_view, uint32_t, uint32_t) {
        return {};
    }

    std::vector<std::string> VariableTrackerEngine::devirtualize_call(core::AnalysisContext& ctx, const PointsToSet& receiver_aliases, std::string_view virtual_method_sig) {
        // Nível 16: ReflectionResolver
        if (virtual_method_sig.find("java/lang/reflect/Method;->invoke") != std::string_view::npos) {
            // Se o receptor do invoke (v0) tem um alias que aponta para um nome de método resolvido
            // Injetamos a aresta sintética (Reflect-Bridge)
            // Por enquanto, retorna um marcador de reflexão opaca
            return {"(REFLECTIVE_INVOKE_OPAQUE)"};
        }

        // Caso padrão: CHA guidada por Alias
        return {std::string(virtual_method_sig)};
    }

} // namespace engines
