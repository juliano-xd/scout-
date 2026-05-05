#include "../../../include/engines/cfg/cfg_engine.hpp"
#include <vector>
#include <unordered_map>
#include <bit>
#include <queue>

namespace engines {

    typedef std::vector<uint64_t> BitMask;

    static void set_bit(BitMask& mask, int bit) {
        mask[bit / 64] |= (1ULL << (bit % 64));
    }

    static bool get_bit(const BitMask& mask, int bit) {
        return (mask[bit / 64] & (1ULL << (bit % 64))) != 0;
    }

    std::unordered_map<int, int> DominatorAnalyzer::compute_ipds(CFG& cfg) {
        if (cfg.blocks.empty()) return {};

        int num_real_blocks = (int)cfg.blocks.size();
        int virtual_exit_id = num_real_blocks; // N-th node
        int total_nodes = num_real_blocks + 1;
        int words = (total_nodes + 63) / 64;

        // 1. Cirurgia do Buraco Negro: Garantir que todos os caminhos cheguem ao Virtual Exit
        // Primeiro, identificamos quem já chega no fim naturalmente
        std::vector<std::vector<int>> preds(total_nodes);
        std::vector<std::vector<int>> all_succs(total_nodes);

        for (auto& b : cfg.blocks) {
            std::vector<int> succs = b.successors;
            for (auto& h : b.handlers) succs.push_back(h.target_id);

            if (succs.empty()) {
                all_succs[b.id].push_back(virtual_exit_id);
                preds[virtual_exit_id].push_back(b.id);
            } else {
                for (int s : succs) {
                    all_succs[b.id].push_back(s);
                    preds[s].push_back(b.id);
                }
            }
        }

        // Backward Reachability para detectar loops infinitos isolados
        std::vector<bool> reaches_exit(total_nodes, false);
        std::queue<int> q;
        reaches_exit[virtual_exit_id] = true;
        q.push(virtual_exit_id);

        while (!q.empty()) {
            int curr = q.front(); q.pop();
            for (int p : preds[curr]) {
                if (!reaches_exit[p]) {
                    reaches_exit[p] = true;
                    q.push(p);
                }
            }
        }

        // Aterramento Universal: Conectar buracos negros ao Virtual Exit
        for (int i = 0; i < num_real_blocks; ++i) {
            if (!reaches_exit[i]) {
                all_succs[i].push_back(virtual_exit_id);
                preds[virtual_exit_id].push_back(i);
            }
        }

        // 2. Ponto Fixo de Pós-Dominância
        std::vector<BitMask> pdoms(total_nodes, BitMask(words, 0));

        // Inicialização
        set_bit(pdoms[virtual_exit_id], virtual_exit_id);
        for (int i = 0; i < virtual_exit_id; ++i) {
            for (int w = 0; w < words; ++w) pdoms[i][w] = ~0ULL;
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (int b = 0; b < virtual_exit_id; ++b) {
                BitMask current_inter(words, ~0ULL);
                for (int s : all_succs[b]) {
                    for (int w = 0; w < words; ++w) current_inter[w] &= pdoms[s][w];
                }
                set_bit(current_inter, b);

                if (current_inter != pdoms[b]) {
                    pdoms[b] = std::move(current_inter);
                    changed = true;
                }
            }
        }

        // 3. Extração de IPDs via Popcount + CTZ
        std::vector<int> popcounts(total_nodes, 0);
        for (int i = 0; i < total_nodes; ++i) {
            for (uint64_t w : pdoms[i]) popcounts[i] += (int)std::popcount(w);
        }

        std::unordered_map<int, int> ipd_map;
        for (int b = 0; b < virtual_exit_id; ++b) {
            int target_pop = popcounts[b] - 1;
            int found_ipd = -1;

            // Otimização CTZ: Varremos apenas os bits setados na máscara de dominância
            for (int w = 0; w < words; ++w) {
                uint64_t word = pdoms[b][w];
                while (word) {
                    int bit_idx = w * 64 + std::countr_zero(word);
                    if (bit_idx != b && popcounts[bit_idx] == target_pop) {
                        found_ipd = bit_idx;
                        break;
                    }
                    word &= (word - 1); // Clear lowest bit
                }
                if (found_ipd != -1) break;
            }
            // Se o IPD for o Virtual Exit, marcamos como -1 (fim da linha)
            ipd_map[b] = (found_ipd == virtual_exit_id) ? -1 : found_ipd;
            cfg.blocks[b].ipd = ipd_map[b];
        }

        return ipd_map;
    }

} // namespace engines
