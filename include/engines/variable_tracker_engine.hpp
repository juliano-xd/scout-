#pragma once

#include "engines/i_search_engine.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <optional>
#include <string_view>
#include <algorithm>

namespace engines {

    /**
     * @brief LocusID: Identificador compacto de um sítio de alocação (Allocation Site).
     * Empacotado em 32 bits para maximizar cache locality.
     * [ClassID: 16 | MethodID: 8 | Offset: 8]
     */
    using LocusID = uint32_t;

    /**
     * @brief PointsToSet: Conjunto de possíveis instâncias (LocusIDs) para um registrador.
     * Otimizado via Small Vector Optimization (SVO) para evitar alocações na heap em 90% dos casos.
     */
    struct PointsToSet {
        static constexpr size_t SmallSize = 2;
        
        union {
            LocusID small[SmallSize];
            std::vector<LocusID>* large;
        } storage;

        uint8_t count = 0;
        bool is_large = false;

        PointsToSet() : count(0), is_large(false) {}
        
        ~PointsToSet() {
            if (is_large) delete storage.large;
        }

        PointsToSet(const PointsToSet& other) {
            copy_from(other);
        }
        
        PointsToSet& operator=(const PointsToSet& other) {
            if (this != &other) {
                if (is_large) delete storage.large;
                copy_from(other);
            }
            return *this;
        }

        void add(LocusID id) {
            if (contains(id)) return;
            if (!is_large) {
                if (count < SmallSize) {
                    storage.small[count++] = id;
                } else {
                    auto* v = new std::vector<LocusID>();
                    v->push_back(storage.small[0]);
                    v->push_back(storage.small[1]);
                    v->push_back(id);
                    storage.large = v;
                    is_large = true;
                    count = 3;
                }
            } else {
                storage.large->push_back(id);
                count = static_cast<uint8_t>(std::min<size_t>(255, storage.large->size()));
            }
        }

        bool contains(LocusID id) const {
            if (!is_large) {
                for (int i = 0; i < count; ++i) if (storage.small[i] == id) return true;
                return false;
            }
            return std::find(storage.large->begin(), storage.large->end(), id) != storage.large->end();
        }

        bool empty() const { return count == 0; }
        void clear() {
            if (is_large) delete storage.large;
            is_large = false;
            count = 0;
        }

    private:
        void copy_from(const PointsToSet& other) {
            is_large = other.is_large;
            count = other.count;
            if (is_large) {
                storage.large = new std::vector<LocusID>(*other.storage.large);
            } else {
                for (int i = 0; i < count; ++i) storage.small[i] = other.storage.small[i];
            }
        }
    };

    struct VariableEvent {
        std::string_view method;
        int line;
        std::string_view reg;
        std::string_view action;
        std::string_view target;
        std::string_view extra;
    };

    struct MethodSummary {
        bool return_tainted = false;
        std::unordered_set<std::string_view> return_obj_fields;
        PointsToSet return_aliases;
    };

    struct CacheKey {
        std::string_view method_sig;
        uint64_t reg_mask;

        bool operator==(const CacheKey& other) const {
            return method_sig == other.method_sig && reg_mask == other.reg_mask;
        }
    };

    struct CacheKeyHash {
        std::size_t operator()(const CacheKey& k) const {
            return std::hash<std::string_view>{}(k.method_sig) ^ (std::hash<uint64_t>{}(k.reg_mask) << 1);
        }
    };

    class VariableTrackerEngine : public ISearchEngine {
    public:
        struct TrackingState {
            std::string_view current_method;
            uint64_t active_regs = 0;
            std::unordered_map<int, std::unordered_set<std::string_view>> obj_taint_map;
            std::unordered_map<int, PointsToSet> alias_map; // Alias Identity
            std::vector<int> control_taint_stack;
            int depth = 0;
            MethodSummary last_call_summary;
        };

        VariableTrackerEngine();

        std::vector<SearchResult> search(
            core::AnalysisContext& ctx,
            const SearchConfig& config
        ) override;

        std::string name() const override { return "track_var"; }
        std::string description() const override { return "Scout-Alias + Aero-Taint Nível 15: Context-Sensitive Identity Bridge."; }
        EngineStats get_stats() const override { return stats_; }

        bool supports_config(const SearchConfig& config) const override;

        // Alias & CHA Interface
        PointsToSet get_points_to_set(core::AnalysisContext& ctx, 
                                     std::string_view method_sig, 
                                     uint32_t reg_id, 
                                     uint32_t instruction_offset);

        std::vector<std::string> devirtualize_call(core::AnalysisContext& ctx,
                                                   const PointsToSet& receiver_aliases,
                                                   std::string_view virtual_method_sig);

    private:
        MethodSummary track_recursive(
            core::AnalysisContext& ctx,
            TrackingState& state,
            std::vector<VariableEvent>& events,
            const SearchConfig& config
        );

        void process_method_internal(
            std::string_view body,
            TrackingState& state,
            TrackingState& ex_out,
            std::vector<VariableEvent>& events,
            core::AnalysisContext& ctx,
            const SearchConfig& config,
            std::string_view method_name,
            int depth
        );

        static int reg_to_bit(std::string_view reg) {
            if (reg.empty()) return -1;
            char prefix = reg[0];
            try {
                // Handle v0, v1... and p0, p1...
                std::string num_part = std::string(reg.substr(1));
                if (num_part.empty()) return -1;
                int val = std::stoi(num_part);
                if (prefix == 'v') return (val >= 0 && val < 32) ? val : -1;
                if (prefix == 'p') return (val >= 0 && val < 16) ? 32 + val : -1;
            } catch (...) {}
            return -1;
        }

        static std::string_view bit_to_reg_sv(int bit);
        std::string_view pool_string(std::string_view s);
        bool is_sanitizer(std::string_view target);
        bool is_transform(std::string_view target);

        std::unordered_set<std::string> string_pool_;
        std::unordered_map<CacheKey, std::pair<std::vector<VariableEvent>, MethodSummary>, CacheKeyHash> analysis_cache_;
        EngineStats stats_;
    };

} // namespace engines
