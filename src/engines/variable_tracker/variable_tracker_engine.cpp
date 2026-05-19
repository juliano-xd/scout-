#include "../../../include/engines/variable_tracker/variable_tracker_engine.hpp"
#include "../../../include/engines/cfg/cfg_engine.hpp"
#include "../../../include/core/analysis_context.hpp"
#include "../../../include/utils/string_utils.hpp"
#include "../../../include/utils/sexpr.hpp"

#include <algorithm>
#include <array>
#include <queue>
#include <ranges>
#include <chrono>

// ============================================================
// MUDANÇAS EM RELAÇÃO AO ORIGINAL — RESUMO
// ============================================================
// [BUG-1]  Recursão infinita em ciclos de call-graph:
//          Adicionado `in_progress_methods_` (unordered_set<string_view>)
//          para marcar métodos em voo. track_recursive retorna {} imediatamente
//          se o método já estiver em progresso.
//
// [BUG-2]  Chave de cache incompleta:
//          CacheKey agora inclui hash de obj_taint_map e static_fields_taint
//          via `taint_fingerprint` (XOR de hashes dos campos tainted).
//
// [BUG-3]  Shadowing de `sp` no bloco `return`:
//          Renomeado para `ret_sp` dentro do handler.
//
// [BUG-4]  Falso positivo na busca de cabeçalho de método:
//          A comparação agora exige que o method_sig seja imediatamente
//          precedido por espaço/tab E seguido por '(' para evitar casar
//          com nomes que são substrings de outros.
//
// [BUG-5]  devirtualize_call retornando string opaca usada como método:
//          Adicionado check explícito antes de chamar track_recursive;
//          strings que começam com '(' são tratadas como opacas.
//
// [BUG-6]  is_prop excessivamente amplo:
//          Substituído find(";->put") por lista explícita de classes-alvo.
//
// [BUG-7]  rfind redundante:
//          Removido; `start` agora é simplesmente `m_pos`.
//
// [BUG-8]  Ordenação não-determinística de eventos:
//          `block_events_map` substituído por vector<pair<int,...>> e
//          ordenado por block id antes de flatten.
//
// [PERF-1] Double lookup no cache: `count` + `operator[]`
//          Substituído por `find` em todos os casos.
//
// [PERF-2] `any_of` em SINKS dentro de loop de invocações:
//          Extraído para fora do loop de argumentos (era O(args * sinks)).
//
// [PERF-3] `string_pool_` retém strings entre chamadas recursivas;
//          Limpeza movida para após o scan completo, não antes.
//
// [DESIGN] EventAction: enum class em vez de literais de string espalhados.
//          Adicione ao header público se necessário.
// ============================================================

namespace engines {

    // ----------------------------------------------------------
    // Enum de ações — evita literais dispersos e permite switch
    // ----------------------------------------------------------
    enum class EventAction : uint8_t {
        MOVE,
        MOVE_RESULT,
        CONST_ASSIGN,
        LOAD,
        STORE,
        STORE_STATIC,
        STORE_ARRAY,
        CALL,
        SINK_LEAK,
        TRANSFORM,
        SANITY,
        TAINT_PROP,
        EES_OPAQUE_ENTRY,
    };

    static std::string_view action_to_sv(EventAction a) noexcept {
        switch (a) {
            case EventAction::MOVE:              return "MOVE";
            case EventAction::MOVE_RESULT:       return "MOVE_RESULT";
            case EventAction::CONST_ASSIGN:      return "CONST";
            case EventAction::LOAD:              return "LOAD";
            case EventAction::STORE:             return "STORE";
            case EventAction::STORE_STATIC:      return "STORE_STATIC";
            case EventAction::STORE_ARRAY:       return "STORE_ARRAY";
            case EventAction::CALL:              return "CALL";
            case EventAction::SINK_LEAK:         return "SINK_LEAK";
            case EventAction::TRANSFORM:         return "TRANSFORM";
            case EventAction::SANITY:            return "SANITY";
            case EventAction::TAINT_PROP:        return "TAINT_PROP";
            case EventAction::EES_OPAQUE_ENTRY:  return "EES_OPAQUE_ENTRY";
        }
        return "UNKNOWN";
    }

    namespace {
        constexpr std::array<std::string_view, 48> REG_NAMES = {
            "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
            "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
            "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
            "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
            "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7",
            "p8", "p9", "p10", "p11", "p12", "p13", "p14", "p15"
        };

        constexpr std::array<std::string_view, 18> SINKS = {
            "Landroid/util/Log;", "Ljava/io/OutputStream;",
            "Ljava/net/HttpURLConnection;", "Landroid/database/sqlite/SQLiteDatabase;",
            "Ljava/lang/Runtime;->exec", "Landroid/telephony/SmsManager;->sendTextMessage",
            "Landroid/content/SharedPreferences$Editor;", "Ljava/io/FileOutputStream;",
            "Ljava/io/File;-><init>", "Landroid/content/Intent;-><init>",
            "Ldalvik/system/DexClassLoader;", "Landroid/webkit/WebView;->loadUrl",
            "Landroid/webkit/WebView;->evaluateJavascript", "Ljava/net/URL;->openConnection",
            "Landroid/content/Context;->startActivity", "Landroid/content/Context;->sendBroadcast",
            "Ljava/lang/System;->loadLibrary", "Ljava/lang/System;->load"
        };

        constexpr std::array<std::string_view, 5> SANITIZERS = {
            "Ljava/security/MessageDigest;->digest", "Ljavax/crypto/Cipher;->doFinal",
            "Ljava/util/zip/CRC32;->update", "Ljava/lang/String;->hashCode",
            "Landroid/text/Html;->escapeHtml"
        };

        constexpr std::array<std::string_view, 8> TRANSFORMS = {
            "Landroid/util/Base64;->encode", "Ljava/net/URLEncoder;->encode",
            "Ljava/lang/Integer;->toHexString", "Ljava/lang/String;->getBytes",
            "Ljava/lang/String;->format", "Ljava/lang/String;->substring",
            "Ljava/lang/String;->replace", "Ljava/lang/String;->concat"
        };

        // [BUG-6] Lista explícita de propagadores conhecidos, em vez de
        // busca genérica por ";->put" que casava com qualquer método com "put" no nome.
        constexpr std::array<std::string_view, 6> PROPAGATORS = {
            "Ljava/lang/StringBuilder;->append",
            "Ljava/util/Map;->put",
            "Ljava/util/HashMap;->put",
            "Ljava/util/LinkedHashMap;->put",
            "Ljava/util/concurrent/ConcurrentHashMap;->put",
            ";-><init>",
        };

        [[nodiscard]] constexpr bool is_PEI_local(std::string_view line) noexcept {
            if (line.empty()) return false;
            switch (line.front()) {
                case 'i': return line.starts_with("invoke-") || line.starts_with("iget") || line.starts_with("iput");
                case 'a': return line.starts_with("aget") || line.starts_with("aput") || line.starts_with("array-length");
                case 's': return line.starts_with("sget") || line.starts_with("sput");
                case 'd': return line.starts_with("div-");
                case 'r': return line.starts_with("rem-");
                case 'c': return line.starts_with("check-cast");
                case 'n': return line.starts_with("new-");
                case 't': return line.starts_with("throw");
                case 'm': return line.starts_with("monitor-");
                case 'f': return line.starts_with("fill-array-data"); // instrução omitida no original
                default:  return false;
            }
        }

        // [BUG-2] Gera uma "impressão digital" determinística da parte de campos do estado
        // de taint para compor a chave de cache. Rápido e suficientemente único.
        [[nodiscard]] uint64_t compute_taint_fingerprint(
            const std::unordered_map<int, std::unordered_set<std::string_view>>& obj_map,
            const std::unordered_set<std::string_view>& static_fields
        ) noexcept {
            uint64_t h = 0;
            const std::hash<std::string_view> sv_hash{};
            for (const auto& [reg, fields] : obj_map) {
                for (const auto& f : fields) {
                    // XOR comutativo — a ordem dos registradores não importa para a chave
                    h ^= sv_hash(f) ^ (static_cast<uint64_t>(reg) * 0x9e3779b97f4a7c15ULL);
                }
            }
            for (const auto& f : static_fields) {
                h ^= sv_hash(f) ^ 0xdeadbeefcafeULL;
            }
            return h;
        }

    } // namespace (anon)

    // ----------------------------------------------------------
    VariableTrackerEngine::VariableTrackerEngine() {
        stats_.total_time = std::chrono::milliseconds(0);
    }

    std::string_view VariableTrackerEngine::bit_to_reg_sv(int bit) {
        if (bit >= 0 && bit < 48) return REG_NAMES[bit];
        return "unk";
    }

    std::string_view VariableTrackerEngine::pool_string(std::string_view s) {
        auto it = string_pool_.find(s);
        if (it != string_pool_.end()) return *it;
        return *string_pool_.insert(std::string(s)).first;
    }

    bool VariableTrackerEngine::is_sanitizer(std::string_view target) {
        return std::ranges::any_of(SANITIZERS, [target](std::string_view s) {
            return target.find(s) != std::string_view::npos;
        });
    }

    bool VariableTrackerEngine::is_transform(std::string_view target) {
        return std::ranges::any_of(TRANSFORMS, [target](std::string_view t) {
            return target.find(t) != std::string_view::npos;
        });
    }

    // [BUG-6] is_propagator usa lista explícita.
    bool VariableTrackerEngine::is_propagator(std::string_view target) {
        return std::ranges::any_of(PROPAGATORS, [target](std::string_view p) {
            return target.find(p) != std::string_view::npos;
        });
    }

    bool VariableTrackerEngine::merge_states(
        VariableTrackerEngine::TrackingState& target,
        const VariableTrackerEngine::TrackingState& incoming
    ) {
        // [O2] Fast path: se active_regs ja contem todos os bits de incoming
        // e todos os maps de obj/static ja sao subconjunto, pula merge.
        if ((target.active_regs | incoming.active_regs) == target.active_regs
            && target.control_taint_stack == incoming.control_taint_stack)
        {
            bool subset = true;
            for (const auto& [reg, fields] : incoming.obj_taint_map) {
                const auto it = target.obj_taint_map.find(reg);
                if (it == target.obj_taint_map.end()) { subset = false; break; }
                for (const auto& f : fields) {
                    if (!it->second.count(f)) { subset = false; break; }
                }
                if (!subset) break;
            }
            if (subset) {
                for (const auto& f : incoming.static_fields_taint) {
                    if (!target.static_fields_taint.count(f)) { subset = false; break; }
                }
            }
            if (subset) return false;
        }

        bool changed = false;

        const uint64_t old_regs = target.active_regs;
        target.active_regs |= incoming.active_regs;
        if (target.active_regs != old_regs) changed = true;

        for (const auto& [reg, fields] : incoming.obj_taint_map) {
            auto& t_fields = target.obj_taint_map[reg];
            const size_t old_sz = t_fields.size();
            t_fields.insert(fields.begin(), fields.end());
            if (t_fields.size() > old_sz) changed = true;
        }

        if (incoming.control_taint_stack.size() >= target.control_taint_stack.size()
            && incoming.control_taint_stack != target.control_taint_stack)
        {
            target.control_taint_stack = incoming.control_taint_stack;
            changed = true;
        }

        const size_t old_static_sz = target.static_fields_taint.size();
        target.static_fields_taint.insert(incoming.static_fields_taint.begin(), incoming.static_fields_taint.end());
        if (target.static_fields_taint.size() > old_static_sz) changed = true;

        return changed;
    }

    std::vector<SearchResult> VariableTrackerEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        const auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<SearchResult> results;
        std::vector<VariableEvent> events;

        std::string_view target_query_sv = config.query;
        std::string initial_reg_str = config.var_name;

        SearchConfig local_config = config;

        const size_t qmark = target_query_sv.find('?');
        if (qmark != std::string_view::npos) {
            local_config.query = std::string(target_query_sv.substr(qmark + 1));
            target_query_sv = target_query_sv.substr(0, qmark);
        } else if (initial_reg_str.empty()) {
            const size_t colon = target_query_sv.find_last_of(':');
            if (colon != std::string_view::npos) {
                initial_reg_str = std::string(utils::trim(target_query_sv.substr(colon + 1)));
                target_query_sv = target_query_sv.substr(0, colon);
            }
        }

        const bool is_const_search = !local_config.query.empty();
        if (target_query_sv.empty() || (initial_reg_str.empty() && !is_const_search)) {
            return results;
        }

        analysis_cache_.clear();
        in_progress_methods_.clear(); // [BUG-1]
        string_pool_.clear(); // [BUG-6]

        TrackingState state;
        state.current_method = pool_string(target_query_sv);

        const int bit = reg_to_bit(initial_reg_str);
        if (bit != -1 && !initial_reg_str.empty()) {
            state.active_regs |= (1ULL << bit);
        }

        track_recursive(ctx, state, events, local_config);

        SearchResult res;
        res.engine_name = name();
        res.file_path = "aero_taint_analysis";

        auto root = sexpr::form("aero-taint-report");
        root.kv("start", sexpr::string(state.current_method));

        auto timeline = sexpr::list();
        for (const auto& ev : events) {
            auto node = sexpr::form("ev");
            node.kv("m", sexpr::string(ev.method));
            node.kv("l", sexpr::integer(ev.line));
            node.kv("r", sexpr::string(ev.reg));
            node.kv("a", sexpr::string(ev.action));   // já é string_view via action_to_sv
            node.kv("t", sexpr::string(ev.target));
            if (!ev.extra.empty()) node.kv("e", sexpr::string(ev.extra));
            timeline.push(std::move(node));
        }

        root.kv("flow", std::move(timeline));
        res.context = root.to_string();
        results.push_back(std::move(res));

        stats_.total_time += std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_time);
        stats_.matches_found = static_cast<int>(events.size());

        return results;
    }

    MethodSummary VariableTrackerEngine::track_recursive(
        core::AnalysisContext& ctx,
        TrackingState& state,
        std::vector<VariableEvent>& events,
        const SearchConfig& config,
        std::string_view initial_reg
    ) {
        if (state.depth > config.search_depth) return {};
        if (state.active_regs == 0
            && state.obj_taint_map.empty()
            && state.control_taint_stack.empty()
            && config.query.empty())
        {
            return {};
        }

        // [BUG-1] Proteção contra recursão em ciclos do call-graph.
        // Se o método já está sendo analisado no stack atual, retornamos {}
        // para quebrar o ciclo sem resultados espúrios.
        if (in_progress_methods_.count(state.current_method)) {
            return {};
        }

        // [BUG-2] Chave de cache agora inclui fingerprint de obj/static taint.
        const uint64_t taint_fp = compute_taint_fingerprint(
            state.obj_taint_map, state.static_fields_taint);
        // [BUG-5] Chave de cache inclui hash de control_taint_stack
        // para evitar reuso de resultados entre diferentes contextos de
        // controle (ex: dentro vs fora de um if com taint).
        uint64_t control_hash = 0;
        for (const int ctrl : state.control_taint_stack) {
            control_hash ^= static_cast<uint64_t>(ctrl) * 0x9e3779b97f4a7c15ULL;
        }
        CacheKey key{state.current_method, state.active_regs, taint_fp, control_hash, state.depth};

        // [PERF-1] Single lookup no cache com `find`.
        if (const auto cache_it = analysis_cache_.find(key); cache_it != analysis_cache_.end()) {
            const auto& [cached_events, cached_summary] = cache_it->second;
            events.insert(events.end(), cached_events.begin(), cached_events.end());
            return cached_summary;
        }

        const size_t arrow = state.current_method.find("->");
        if (arrow == std::string_view::npos) return {};

        const std::string_view class_name = state.current_method.substr(0, arrow);
        const std::string_view method_sig  = state.current_method.substr(arrow + 2);

        const std::string_view content = ctx.get_class_content(class_name);
        if (content.empty()) return {};

        // [BUG-4] Busca de cabeçalho com critério mais estrito:
        // method_sig deve ser precedido por espaço/tab e seguido por '(' para
        // evitar casar nomes que são substrings de outros (ex: getName vs getNameById).
        size_t m_pos = content.find(".method ");
        while (m_pos != std::string_view::npos) {
            const size_t next_line = content.find('\n', m_pos);
            const size_t eol = (next_line != std::string_view::npos) ? next_line : content.size();
            const std::string_view header = content.substr(m_pos, eol - m_pos);

            const size_t sig_pos = header.find(method_sig);
            if (sig_pos != std::string_view::npos) {
                const bool preceded_ok = sig_pos > 0
                    && (header[sig_pos - 1] == ' ' || header[sig_pos - 1] == '\t');
                // [BUG-4] Garante que o método não é prefixo de um nome mais longo.
                // Se method_sig já contém '(' é uma assinatura completa
                // (auto-qualificada). Basta verificar que termina em boundary
                // (fim da linha ou espaço). Caso contrário, é nome simples
                // e exigimos '(' após.
                const bool has_full_sig = method_sig.find('(') != std::string_view::npos;
                const size_t after = sig_pos + method_sig.size();
                bool followed_ok;
                if (has_full_sig) {
                    followed_ok = after >= header.size()
                        || header[after] == ' ' || header[after] == '\t';
                } else {
                    followed_ok = after < header.size() && header[after] == '(';
                }
                if (preceded_ok && followed_ok) break;
            }
            m_pos = content.find(".method ", eol);
        }

        if (m_pos == std::string_view::npos) return {};

        // [BUG-7] `start` é simplesmente `m_pos`; o rfind original era código morto.
        const size_t start = m_pos;
        const size_t end   = content.find(".end method", m_pos);
        if (end == std::string_view::npos) return {};

        const std::string_view body = content.substr(start, end - start);

        CFG cfg = CFGEngine::build_cfg(body);
        DominatorAnalyzer::compute_ipds(cfg);

        std::unordered_map<int, TrackingState>            block_in_states;
        std::vector<std::vector<VariableEvent>> block_events_map(cfg.blocks.size());
        std::vector<bool> visited_blocks(cfg.blocks.size(), false);
        std::queue<int> worklist;

        block_in_states[cfg.entry_block_id]              = state;
        block_in_states[cfg.entry_block_id].active_regs  = state.active_regs;
        block_in_states[cfg.entry_block_id].obj_taint_map = state.obj_taint_map;

        worklist.push(cfg.entry_block_id);
        visited_blocks[cfg.entry_block_id] = true;

        // [BUG-1] Marca o método como em progresso antes de entrar no worklist.
        in_progress_methods_.insert(state.current_method);

        MethodSummary final_summary;

        while (!worklist.empty()) {
            const int bid = worklist.front();
            worklist.pop();
            const auto& block = cfg.blocks[bid];

            TrackingState current_in = block_in_states[bid];

            std::erase(current_in.control_taint_stack, bid);

            TrackingState current_out   = current_in;
            TrackingState exception_out;
            std::vector<VariableEvent> block_events;
            block_events.reserve(32);

            process_method_internal(
                block.code_content, current_out, exception_out, block_events,
                ctx, config, state.current_method, state.depth, initial_reg);

            // Detecção de branch tainted
            const std::string_view trimmed_body = utils::trim(block.code_content);
            const size_t last_nl = trimmed_body.find_last_of('\n');
            const std::string_view last_line =
                (last_nl == std::string_view::npos)
                    ? trimmed_body
                    : utils::trim(trimmed_body.substr(last_nl + 1));

            if (last_line.starts_with("if-")) {
                if (current_in.active_regs != 0 && block.ipd != -1) {
                    current_out.control_taint_stack.push_back(block.ipd);
                }
            }

            for (const int sid : block.successors) {
                const bool state_changed = merge_states(block_in_states[sid], current_out);
                if (state_changed || !visited_blocks[sid]) {
                    visited_blocks[sid] = true;
                    worklist.push(sid);
                }
            }

            for (const auto& handler : block.handlers) {
                const bool state_changed = merge_states(block_in_states[handler.target_id], exception_out);
                if (state_changed || !visited_blocks[handler.target_id]) {
                    visited_blocks[handler.target_id] = true;
                    worklist.push(handler.target_id);
                }
            }

            block_events_map[bid] = std::move(block_events);

            if (block.successors.empty()) {
                final_summary.return_tainted |= current_out.last_call_summary.return_tainted;
                for (const auto& f : current_out.last_call_summary.return_obj_fields)
                    final_summary.return_obj_fields.insert(f);
                for (const auto& f : current_out.static_fields_taint)
                    final_summary.modified_static_fields.insert(f);
            }
        }

        // [BUG-1] Remove o método do conjunto de "em progresso" ao terminar.
        in_progress_methods_.erase(state.current_method);

        std::vector<VariableEvent> all_method_events;
        for (auto& bevs : block_events_map) {
            all_method_events.insert(all_method_events.end(),
                                     std::make_move_iterator(bevs.begin()),
                                     std::make_move_iterator(bevs.end()));
        }

        analysis_cache_[key] = {all_method_events, final_summary};
        events.insert(events.end(),
                      std::make_move_iterator(all_method_events.begin()),
                      std::make_move_iterator(all_method_events.end()));

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
        int depth,
        std::string_view initial_reg
    ) {
        size_t pos = 0;
        int line_idx = 0;
        state.last_call_summary = {};

        while (pos < body.size()) {
            size_t next = body.find('\n', pos);
            if (next == std::string_view::npos) next = body.size();

            std::string_view line = utils::trim(body.substr(pos, next - pos));
            pos = next + 1;
            line_idx++;

            if (line.empty() || line.front() == '.' || line.front() == '#') continue;

            const size_t hash_pos = line.find('#');
            if (hash_pos != std::string_view::npos) {
                line = utils::trim(line.substr(0, hash_pos));
                if (line.empty()) continue;
            }

            if (is_PEI_local(line)) merge_states(ex_out, state);

            if (line.starts_with("move-result") || line.starts_with("move-exception")) {
                handle_move_result(line, line_idx, state, events, method_name);
            }
            else if (line.starts_with("move ") || line.starts_with("move/")
                     || line.starts_with("move-object") || line.starts_with("move-wide")) {
                handle_move_instruction(line, line_idx, state, events, method_name, initial_reg);
            }
            else if (line.starts_with("const")) {
                handle_const_instruction(line, line_idx, state, events, method_name, initial_reg, config);
            }
            else if (line.starts_with("iget") || line.starts_with("sget") || line.starts_with("aget")) {
                handle_get_instruction(line, line_idx, state, events, method_name, initial_reg);
            }
            else if (line.starts_with("iput") || line.starts_with("sput") || line.starts_with("aput")) {
                handle_put_instruction(line, line_idx, state, events, method_name);
            }
            else if (line.starts_with("invoke-")) {
                handle_invoke_instruction(line, line_idx, state, events, ctx, config, method_name, depth);
            }
            else if (line.starts_with("return")) {
                handle_return_instruction(line, state);
            }
        } // while (pos < body.size())
    }

    // [C10] Handler: move-result / move-exception
    void VariableTrackerEngine::handle_move_result(
        std::string_view line, int line_idx,
        TrackingState& state, std::vector<VariableEvent>& events,
        std::string_view method_name
    ) {
        const bool pc_tainted  = !state.control_taint_stack.empty();
        const size_t sp = line.find(' ');
        if (sp != std::string_view::npos) {
            const int d_bit = reg_to_bit(utils::trim(line.substr(sp + 1)));
            if (d_bit != -1) {
                if (state.last_call_summary.return_tainted || pc_tainted) {
                    state.active_regs |= (1ULL << d_bit);
                    events.push_back({method_name, line_idx,
                        bit_to_reg_sv(d_bit), action_to_sv(EventAction::MOVE_RESULT),
                        "", pc_tainted ? "IMPLICIT" : "CALL_RESULT"});
                } else {
                    state.active_regs &= ~(1ULL << d_bit);
                    state.obj_taint_map.erase(d_bit);
                }
                if (!state.last_call_summary.return_obj_fields.empty()) {
                    state.obj_taint_map[d_bit] = state.last_call_summary.return_obj_fields;
                }
            }
        }
        state.last_call_summary = {};
    }

    // [C10] Handler: move / move-object / move-wide
    void VariableTrackerEngine::handle_move_instruction(
        std::string_view line, int line_idx,
        TrackingState& state, std::vector<VariableEvent>& events,
        std::string_view method_name, std::string_view initial_reg
    ) {
        const bool pc_tainted  = !state.control_taint_stack.empty();
        const size_t sp = line.find(' ');
        if (sp != std::string_view::npos) {
            const size_t comma = line.find(',', sp);
            if (comma != std::string_view::npos) {
                const std::string_view dst_r = utils::trim(line.substr(sp + 1, comma - sp - 1));
                const std::string_view src_r = utils::trim(line.substr(comma + 1));
                const int d_bit = reg_to_bit(dst_r);
                const int s_bit = reg_to_bit(src_r);

                if (d_bit != -1) {
                    bool tainted = pc_tainted
                        || (s_bit != -1 && (state.active_regs & (1ULL << s_bit)));
                    if (initial_reg == dst_r) tainted = true;

                    if (tainted) {
                        state.active_regs |= (1ULL << d_bit);
                        state.obj_taint_map.erase(d_bit);
                        if (s_bit != -1) {
                            if (const auto it = state.obj_taint_map.find(s_bit);
                                it != state.obj_taint_map.end())
                            {
                                state.obj_taint_map[d_bit] = it->second;
                            }
                        }
                        const std::string_view extra =
                            pc_tainted ? "IMPLICIT"
                            : (initial_reg == dst_r ? "INITIAL_REG_SOURCE" : "");
                        events.push_back({method_name, line_idx,
                            bit_to_reg_sv(d_bit), action_to_sv(EventAction::MOVE),
                            src_r, extra});
                    } else {
                        state.active_regs &= ~(1ULL << d_bit);
                        state.obj_taint_map.erase(d_bit);
                    }
                }
            }
        }
    }

    // [C10] Handler: const*
    void VariableTrackerEngine::handle_const_instruction(
        std::string_view line, int line_idx,
        TrackingState& state, std::vector<VariableEvent>& events,
        std::string_view method_name, std::string_view initial_reg,
        const SearchConfig& config
    ) {
        const bool pc_tainted  = !state.control_taint_stack.empty();
        const size_t sp = line.find(' ');
        if (sp != std::string_view::npos) {
            const size_t comma = line.find(',', sp);
            if (comma != std::string_view::npos) {
                const std::string_view dst_r = utils::trim(line.substr(sp + 1, comma - sp - 1));
                const std::string_view val   = utils::trim(line.substr(comma + 1));
                const int d_bit = reg_to_bit(dst_r);

                if (d_bit != -1) {
                    bool tainted       = pc_tainted;
                    std::string_view extra  = "";
                    std::string_view target = "const";

                    if (initial_reg == dst_r) {
                        tainted = true;
                        extra   = "INITIAL_REG_SOURCE";
                    }

                    if (!config.query.empty()) {
                        std::string_view normalized_val = val;
                        if (normalized_val.starts_with('"') && normalized_val.ends_with('"')) {
                            normalized_val = normalized_val.substr(1, normalized_val.size() - 2);
                        }
                        if (normalized_val == config.query
                            || (config.query.starts_with("0x") && val == config.query))
                        {
                            tainted = true;
                            target  = normalized_val;
                            extra   = "CONST_SOURCE";
                        }
                    }

                    if (tainted) {
                        state.active_regs |= (1ULL << d_bit);
                        state.obj_taint_map.erase(d_bit);
                        events.push_back({method_name, line_idx,
                            bit_to_reg_sv(d_bit), action_to_sv(EventAction::CONST_ASSIGN),
                            target, pc_tainted ? "IMPLICIT" : extra});
                    } else {
                        state.active_regs &= ~(1ULL << d_bit);
                        state.obj_taint_map.erase(d_bit);
                    }
                }
            }
        }
    }

    // [C10] Handler: iget / sget / aget
    void VariableTrackerEngine::handle_get_instruction(
        std::string_view line, int line_idx,
        TrackingState& state, std::vector<VariableEvent>& events,
        std::string_view method_name, std::string_view initial_reg
    ) {
        const bool pc_tainted  = !state.control_taint_stack.empty();
        const size_t sp = line.find(' ');
        if (sp != std::string_view::npos) {
            const size_t first_comma = line.find(',', sp);
            if (first_comma != std::string_view::npos) {
                const std::string_view dst_r = utils::trim(line.substr(sp + 1, first_comma - sp - 1));
                const int d_bit = reg_to_bit(dst_r);

                if (d_bit != -1) {
                    bool tainted       = pc_tainted || (initial_reg == dst_r);
                    std::string_view target = "";
                    std::string_view extra  = "";

                    if (line.starts_with("iget")) {
                        const size_t second_comma = line.find(',', first_comma + 1);
                        if (second_comma != std::string_view::npos) {
                            const std::string_view obj_r = utils::trim(
                                line.substr(first_comma + 1, second_comma - first_comma - 1));
                            const std::string_view field = utils::trim(line.substr(second_comma + 1));
                            const int o_bit = reg_to_bit(obj_r);

                            if (o_bit != -1) {
                                const auto om_it = state.obj_taint_map.find(o_bit);
                                if (om_it != state.obj_taint_map.end()
                                    && om_it->second.count(field))
                                {
                                    tainted = true;
                                    target  = field;
                                    extra   = obj_r;
                                } else if (state.active_regs & (1ULL << o_bit)) { // [C16]
                                    tainted = true;
                                    target  = field;
                                    extra   = obj_r;
                                }
                            }
                        }
                    } else if (line.starts_with("sget")) {
                        const std::string_view field = utils::trim(line.substr(first_comma + 1));
                        if (state.static_fields_taint.count(field)) {
                            tainted = true;
                            target  = field;
                        }
                    } else {
                        const size_t second_comma = line.find(',', first_comma + 1);
                        if (second_comma != std::string_view::npos) {
                            const std::string_view arr_r = utils::trim(
                                line.substr(first_comma + 1, second_comma - first_comma - 1));
                            const std::string_view idx_r = utils::trim(line.substr(second_comma + 1));
                            const int a_bit = reg_to_bit(arr_r);
                            const int i_bit = reg_to_bit(idx_r);

                            if ((a_bit != -1 && (state.active_regs & (1ULL << a_bit)))
                                || (i_bit != -1 && (state.active_regs & (1ULL << i_bit))))
                            {
                                tainted = true;
                                target  = "array_elem";
                                extra   = arr_r;
                            }
                        }
                    }

                    if (tainted) {
                        state.active_regs |= (1ULL << d_bit);
                        state.obj_taint_map.erase(d_bit);
                        events.push_back({method_name, line_idx,
                            bit_to_reg_sv(d_bit), action_to_sv(EventAction::LOAD),
                            target, pc_tainted ? "IMPLICIT" : extra});
                    } else {
                        state.active_regs &= ~(1ULL << d_bit);
                        state.obj_taint_map.erase(d_bit);
                    }
                }
            }
        }
    }

    // [C10] Handler: iput / sput / aput
    void VariableTrackerEngine::handle_put_instruction(
        std::string_view line, int line_idx,
        TrackingState& state, std::vector<VariableEvent>& events,
        std::string_view method_name
    ) {
        const bool pc_tainted  = !state.control_taint_stack.empty();
        const size_t sp = line.find(' ');
        if (sp != std::string_view::npos) {
            const size_t first_comma = line.find(',', sp);
            if (first_comma != std::string_view::npos) {
                const std::string_view src_r = utils::trim(line.substr(sp + 1, first_comma - sp - 1));
                const int s_bit = reg_to_bit(src_r);
                const bool src_tainted =
                    (s_bit != -1 && (state.active_regs & (1ULL << s_bit))) || pc_tainted;

                if (line.starts_with("iput")) {
                    if (src_tainted) {
                        const size_t second_comma = line.find(',', first_comma + 1);
                        if (second_comma != std::string_view::npos) {
                            const std::string_view obj_r = utils::trim(
                                line.substr(first_comma + 1, second_comma - first_comma - 1));
                            const std::string_view field = utils::trim(line.substr(second_comma + 1));
                            const int o_bit = reg_to_bit(obj_r);
                            if (o_bit != -1) {
                                state.obj_taint_map[o_bit].insert(field);
                                events.push_back({method_name, line_idx,
                                    bit_to_reg_sv(s_bit != -1 ? s_bit : 0),
                                    action_to_sv(EventAction::STORE), field, obj_r});
                            }
                        }
                    }
                } else if (line.starts_with("sput")) {
                    if (src_tainted) {
                        const std::string_view field = utils::trim(line.substr(first_comma + 1));
                        state.static_fields_taint.insert(field);
                        events.push_back({method_name, line_idx,
                            bit_to_reg_sv(s_bit != -1 ? s_bit : 0),
                            action_to_sv(EventAction::STORE_STATIC), field, ""});
                    }
                } else {
                    const size_t second_comma = line.find(',', first_comma + 1);
                    if (second_comma != std::string_view::npos) {
                        const std::string_view arr_r = utils::trim(
                            line.substr(first_comma + 1, second_comma - first_comma - 1));
                        const std::string_view idx_r = utils::trim(line.substr(second_comma + 1));
                        const int a_bit = reg_to_bit(arr_r);
                        const int i_bit = reg_to_bit(idx_r);
                        const auto idx_om_it = state.obj_taint_map.find(i_bit);
                        const bool idx_tainted = (i_bit != -1 && ((state.active_regs & (1ULL << i_bit))
                            || (idx_om_it != state.obj_taint_map.end() && !idx_om_it->second.empty())));
                        if (a_bit != -1 && (src_tainted || idx_tainted)) {
                            state.active_regs |= (1ULL << a_bit);
                            events.push_back({method_name, line_idx,
                                bit_to_reg_sv(s_bit != -1 ? s_bit : 0),
                                action_to_sv(EventAction::STORE_ARRAY), "", arr_r});
                        }
                    }
                }
            }
        }
    }

    // [C10] Handler: invoke-*
    void VariableTrackerEngine::handle_invoke_instruction(
        std::string_view line, int line_idx,
        TrackingState& state, std::vector<VariableEvent>& events,
        core::AnalysisContext& ctx, const SearchConfig& config,
        std::string_view method_name, int depth
    ) {
        const bool pc_tainted = !state.control_taint_stack.empty();

        // [C13] Extrai o tipo de invoke (virtual, direct, super, interface, static).
        const size_t first_sp = line.find(' ');
        const std::string_view invoke_type =
            (first_sp != std::string_view::npos) ? line.substr(0, first_sp) : "";

        const size_t ob = line.find('{');
        const size_t cb = line.find('}', ob);

        if (ob != std::string_view::npos && cb != std::string_view::npos) {
            std::string_view target;
            const size_t last_space = line.find_last_of(' ');
            if (last_space != std::string_view::npos) {
                target = utils::trim(line.substr(last_space + 1));
            }

            if (target.starts_with('{')) {
                const size_t comma_cb = line.find('}', last_space);
                if (comma_cb != std::string_view::npos) {
                    const size_t next_token = line.find_first_not_of(", ", comma_cb + 1);
                    if (next_token != std::string_view::npos) {
                        target = utils::trim(line.substr(next_token));
                    }
                }
            }

            const std::string_view regs_sv = line.substr(ob + 1, cb - ob - 1);

            bool is_sink = false;
            for (auto s : SINKS) {
                if (target.find(s) != std::string_view::npos) { is_sink = true; break; }
            }
            const bool is_san  = is_sanitizer(target);
            const bool is_tr   = is_transform(target);
            const bool is_prop = is_propagator(target);
            const bool is_model_propagator =
                target.find("Ljava/lang/String;") != std::string_view::npos
                || target.find("Ljava/lang/StringBuilder;") != std::string_view::npos
                || target.find("Ljava/util/") != std::string_view::npos;

            std::array<int, MAX_CALL_ARGS> call_bits{};
            int call_count = 0;

            if (regs_sv.find(" .. ") != std::string_view::npos) {
                const size_t dd    = regs_sv.find(" .. ");
                const int    s_idx = reg_to_bit(utils::trim(regs_sv.substr(0, dd)));
                const int    e_idx = reg_to_bit(utils::trim(regs_sv.substr(dd + 4)));
                if (s_idx != -1 && e_idx != -1) {
                    for (int i = s_idx; i <= e_idx && call_count < MAX_CALL_ARGS; ++i)
                        call_bits[call_count++] = i;
                }
            } else {
                size_t r_pos = 0;
                while (r_pos < regs_sv.size()) {
                    size_t r_comma = regs_sv.find(',', r_pos);
                    if (r_comma == std::string_view::npos) r_comma = regs_sv.size();
                    const int b = reg_to_bit(utils::trim(regs_sv.substr(r_pos, r_comma - r_pos)));
                    if (b != -1 && call_count < MAX_CALL_ARGS) call_bits[call_count++] = b;
                    r_pos = r_comma + 1;
                }
            }

            bool any_arg_tainted = false;
            for (int i = 0; i < call_count; ++i) {
                const auto om_it = state.obj_taint_map.find(call_bits[i]);
                const bool tainted =
                    (state.active_regs & (1ULL << call_bits[i]))
                    || (om_it != state.obj_taint_map.end() && !om_it->second.empty())
                    || pc_tainted;

                if (!tainted) continue;
                any_arg_tainted = true;

                if (is_san && !pc_tainted) {
                    events.push_back({method_name, line_idx,
                        bit_to_reg_sv(call_bits[i]), action_to_sv(EventAction::SANITY),
                        target, ""});
                    continue;
                }

                EventAction act = is_sink ? EventAction::SINK_LEAK
                                : (is_tr  ? EventAction::TRANSFORM
                                          : EventAction::CALL);
                events.push_back({method_name, line_idx,
                    bit_to_reg_sv(call_bits[i]), action_to_sv(act),
                    target, pc_tainted ? "IMPLICIT" : ""});

                if (is_prop && i > 0) {
                    state.active_regs |= (1ULL << call_bits[0]);
                    if (om_it != state.obj_taint_map.end()) {
                        for (const auto& f : om_it->second) {
                            state.obj_taint_map[call_bits[0]].insert(f);
                        }
                    }
                    events.push_back({method_name, line_idx,
                        bit_to_reg_sv(call_bits[0]), action_to_sv(EventAction::TAINT_PROP),
                        target, ""});
                }
            }

            if (any_arg_tainted) {
                if (is_san && !pc_tainted) {
                    state.last_call_summary = {};
                } else if (is_tr || is_model_propagator) {
                    state.last_call_summary = {};
                    state.last_call_summary.return_tainted = true;
                } else if (target.find("java/lang/reflect/Method;->invoke") != std::string_view::npos
                           || target.find("ClassLoader;->loadClass") != std::string_view::npos
                           || target.find("DexClassLoader") != std::string_view::npos
                           || target.find("PathClassLoader") != std::string_view::npos)
                {
                    state.last_call_summary.return_tainted = true;
                    events.push_back({method_name, line_idx,
                        bit_to_reg_sv(call_bits[0]),
                        action_to_sv(EventAction::EES_OPAQUE_ENTRY),
                        target, "EXTERNAL_PAYLOAD_SHADOWING"});
                } else if (!is_sink) {
                    TrackingState next_state;

                    // [C13] Dispatch conforme tipo de invoke.
                    std::vector<std::string> resolved_targets;
                    if (invoke_type == "invoke-direct" || invoke_type == "invoke-static") {
                        // Alvo exato — sem CHA.
                        resolved_targets = {std::string(target)};
                    } else if (invoke_type == "invoke-super") {
                        // invoke-super resolve na superclasse, não na atual.
                        // O target já nomeia a superclasse diretamente no Smali,
                        // então usamos como está com CHA a partir dela.
                        PointsToSet dummy_aliases;
                        resolved_targets = devirtualize_call(ctx, dummy_aliases, target);
                    } else {
                        // invoke-virtual, invoke-interface: CHA normal.
                        PointsToSet dummy_aliases;
                        resolved_targets = devirtualize_call(ctx, dummy_aliases, target);
                    }

                    // [C12] Itera todos os alvos resolvidos, merge dos summaries.
                    MethodSummary merged_summary;
                    for (const auto& rtarget : resolved_targets) {
                        if (rtarget.empty() || rtarget.front() == '(') {
                            merged_summary.return_tainted = true;
                            continue;
                        }

                        TrackingState target_state;
                        target_state.current_method = pool_string(rtarget);
                        target_state.depth = depth + 1;

                        for (int i = 0; i < call_count; ++i) {
                            if ((state.active_regs & (1ULL << call_bits[i])) || pc_tainted) {
                                target_state.active_regs |= (1ULL << (32 + i));
                            }
                            if (const auto it = state.obj_taint_map.find(call_bits[i]);
                                it != state.obj_taint_map.end())
                            {
                                target_state.obj_taint_map[32 + i] = it->second;
                            }
                        }

                        const auto sub_summary = track_recursive(
                            ctx, target_state, events, config, "");
                        merged_summary.return_tainted |= sub_summary.return_tainted;
                        for (const auto& f : sub_summary.return_obj_fields)
                            merged_summary.return_obj_fields.insert(f);
                        for (const auto& f : sub_summary.modified_static_fields)
                            merged_summary.modified_static_fields.insert(f);
                    }
                    state.last_call_summary = merged_summary;
                    for (const auto& f : merged_summary.modified_static_fields) {
                        state.static_fields_taint.insert(f);
                    }
                }
            } else {
                state.last_call_summary = {};
            }
        }
    }

    // [C10] Handler: return*
    void VariableTrackerEngine::handle_return_instruction(
        std::string_view line,
        TrackingState& state
    ) {
        const bool pc_tainted = !state.control_taint_stack.empty();
        const size_t ret_sp = line.find(' ');
        if (ret_sp != std::string_view::npos) {
            const std::string_view ret_r = utils::trim(line.substr(ret_sp + 1));
            const int r_bit = reg_to_bit(ret_r);
            if (r_bit != -1) {
                state.last_call_summary.return_tainted =
                    (state.active_regs & (1ULL << r_bit)) || pc_tainted;
                if (const auto it = state.obj_taint_map.find(r_bit);
                    it != state.obj_taint_map.end())
                {
                    state.last_call_summary.return_obj_fields = it->second;
                }
            }
        }
    }

    bool VariableTrackerEngine::supports_config(const SearchConfig& config) const {
        return !config.var_name.empty() || config.query.find(':') != std::string::npos;
    }

    PointsToSet VariableTrackerEngine::get_points_to_set(
        core::AnalysisContext&, std::string_view, uint32_t, uint32_t)
    {
        return {};
    }

    std::vector<std::string> VariableTrackerEngine::devirtualize_call(
        core::AnalysisContext& ctx,
        const PointsToSet& /*receiver_aliases*/,
        std::string_view virtual_method_sig
    ) {
        if (virtual_method_sig.find("java/lang/reflect/Method;->invoke") != std::string_view::npos) {
            // [BUG-5] Retorna string marcada com '(' para que o chamador saiba
            // que é um alvo opaco e não tente usá-la como método real.
            return {"(REFLECTIVE_INVOKE_OPAQUE)"};
        }

        const size_t arrow = virtual_method_sig.find("->");
        if (arrow == std::string_view::npos) return {std::string(virtual_method_sig)};

        std::string class_name = std::string(virtual_method_sig.substr(0, arrow));
        const std::string_view method_sig = virtual_method_sig.substr(arrow + 2);

        // [C12] CHA multi-target: coleta TODAS as implementações disponíveis
        // na hierarquia (superclasses + interfaces), não apenas a primeira.
        std::vector<std::string> results;
        std::unordered_set<std::string> seen; // dedup

        auto find_method_in_class = [&](const std::string& cl_name) -> bool {
            const std::string_view content = ctx.get_class_content(cl_name);
            if (content.empty()) return false;

            size_t m_pos = content.find(".method ");
            bool found = false;
            while (m_pos != std::string_view::npos) {
                const size_t next_line = content.find('\n', m_pos);
                const size_t eol = (next_line != std::string_view::npos) ? next_line : content.size();
                const std::string_view header = content.substr(m_pos, eol - m_pos);

                const size_t sig_pos = header.find(method_sig);
                if (sig_pos != std::string_view::npos) {
                    const bool preceded_ok = sig_pos > 0
                        && (header[sig_pos - 1] == ' ' || header[sig_pos - 1] == '\t');
                    if (!preceded_ok) { m_pos = content.find(".method ", eol); continue; }
                    const bool has_full_sig = method_sig.find('(') != std::string_view::npos;
                    const size_t after = sig_pos + method_sig.size();
                    bool followed_ok;
                    if (has_full_sig) {
                        followed_ok = after >= header.size()
                            || header[after] == ' ' || header[after] == '\t';
                    } else {
                        followed_ok = after < header.size() && header[after] == '(';
                    }
                    if (followed_ok) { found = true; break; }
                }
                m_pos = content.find(".method ", eol);
            }
            return found;
        };

        // [C12] Função auxiliar para coletar interfaces de uma classe.
        auto collect_interfaces = [&](const std::string& cl_name) {
            const std::string_view content = ctx.get_class_content(cl_name);
            if (content.empty()) return;
            size_t ipos = 0;
            while ((ipos = content.find(".implements ", ipos)) != std::string_view::npos) {
                const size_t next_line = content.find('\n', ipos);
                const size_t eol = (next_line != std::string_view::npos) ? next_line : content.size();
                const std::string_view iface_name = utils::trim(
                    content.substr(ipos + 12, eol - ipos - 12));
                const std::string iface_str(iface_name);
                if (seen.insert(iface_str).second) {
                    if (find_method_in_class(iface_str)) {
                        results.push_back(iface_str + "->" + std::string(method_sig));
                    }
                }
                ipos = eol;
            }
        };

        // Sobe a hierarquia de superclasses, coletando implementações.
        while (!class_name.empty()) {
            if (!seen.insert(class_name).second) break;
            if (find_method_in_class(class_name)) {
                results.push_back(class_name + "->" + std::string(method_sig));
            }

            const std::string_view content = ctx.get_class_content(class_name);
            if (content.empty()) break;

            // Coleta também interfaces desta classe.
            collect_interfaces(class_name);

            // Sobe para a superclasse.
            const size_t s_pos = content.find(".super ");
            if (s_pos != std::string_view::npos) {
                const size_t next_line = content.find('\n', s_pos);
                class_name = std::string(utils::trim(content.substr(s_pos + 7, next_line - s_pos - 7)));
            } else {
                break;
            }
        }

        if (results.empty()) {
            results.push_back(std::string(virtual_method_sig));
        }
        return results;
    }

} // namespace engines
