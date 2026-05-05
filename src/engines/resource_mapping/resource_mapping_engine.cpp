#include <chrono>
#include <charconv>     // C++17/26: std::from_chars (Parsing numérico ultra-rápido sem excepções)
#include <format>       // C++20/26: Formatação in-place O(N)
#include <system_error> // C++11: RAII Error Handling

#include "../../../include/engines/resource_mapping/resource_mapping_engine.hpp"
#include "../../../include/utils/string_utils.hpp"

namespace engines {

    namespace {
        // Helper utilitário de benchmarking
        template<typename F>
        auto measure_execution(F&& func) {
            const auto start = std::chrono::high_resolution_clock::now();
            auto result = func();
            const auto end = std::chrono::high_resolution_clock::now();
            return std::make_pair(
                std::move(result),
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            );
        }
    } // anonymous namespace

    void ResourceMappingEngine::scan_public_xml(const std::filesystem::path& path) {
        try {
            utils::MappedFile mfile(path);
            if (mfile.is_empty()) return;

            utils::LineIterator it(mfile.view());
            std::string_view line;

            // OPTIMIZAÇÃO EXTREMA: Erradicação do std::regex.
            // O parsing manual sobre std::string_view evita a invocação da complexa state-machine
            // do motor de regex. É uma operação O(N) atómica que roda diretamente na cache L1.
            while (it.next(line)) {
                // Formato Alvo: <public type="string" name="app_name" id="0x7f0b0001" />
                const size_t t_pos = line.find("type=\"");
                const size_t n_pos = line.find("name=\"");
                const size_t i_pos = line.find("id=\"");

                if (t_pos != std::string_view::npos &&
                    n_pos != std::string_view::npos &&
                    i_pos != std::string_view::npos)
                {
                    const size_t t_start = t_pos + 6;
                    const size_t t_end   = line.find('"', t_start);

                    const size_t n_start = n_pos + 6;
                    const size_t n_end   = line.find('"', n_start);

                    const size_t i_start = i_pos + 4;
                    const size_t i_end   = line.find('"', i_start);

                    if (t_end != std::string_view::npos &&
                        n_end != std::string_view::npos &&
                        i_end != std::string_view::npos)
                    {
                        const std::string_view type = line.substr(t_start, t_end - t_start);
                        const std::string_view name = line.substr(n_start, n_end - n_start);
                        const std::string_view id_str = line.substr(i_start, i_end - i_start);

                        // std::from_chars é a forma padrão ouro do C++ moderno para parse numérico
                        if (id_str.starts_with("0x")) {
                            uint32_t id = 0;
                            auto [ptr, ec] = std::from_chars(id_str.data() + 2, id_str.data() + id_str.size(), id, 16);
                            if (ec == std::errc()) {
                                id_to_name_.emplace(id, std::format("{}/{}", type, name));
                            }
                        }
                    }
                }
            }
        } catch (const std::system_error&) {
            // Ignora silenciosamente falhas de mapeamento (ex: ficheiro não legível)
        }
    }

    void ResourceMappingEngine::scan_r_smali(const std::filesystem::path& path) {
        try {
            utils::MappedFile mfile(path);
            if (mfile.is_empty()) return;

            std::string type = path.stem().string(); // R$string -> string
            const size_t dollar_pos = type.find('$');
            if (dollar_pos != std::string::npos) {
                type = type.substr(dollar_pos + 1);
            }

            utils::LineIterator it(mfile.view());
            std::string_view line;

            // OPTIMIZAÇÃO EXTREMA: Erradicação do std::regex também no Smali.
            // Formato Alvo: .field public static final app_name:I = 0x7f0b0001
            while (it.next(line)) {
                const size_t eq_pos = line.find("= 0x");
                if (eq_pos != std::string_view::npos) {

                    const size_t colon_pos = line.rfind(":I", eq_pos);
                    if (colon_pos != std::string_view::npos) {

                        const size_t space_pos = line.rfind(' ', colon_pos);
                        if (space_pos != std::string_view::npos) {

                            const std::string_view name = line.substr(space_pos + 1, colon_pos - space_pos - 1);
                            const std::string_view id_str = line.substr(eq_pos + 4); // Ignora "= 0x"

                            uint32_t id = 0;
                            auto [ptr, ec] = std::from_chars(id_str.data(), id_str.data() + id_str.size(), id, 16);
                            if (ec == std::errc()) {
                                id_to_name_.emplace(id, std::format("{}/{}", type, name));
                            }
                        }
                    }
                }
            }
        } catch (const std::system_error&) {
            // Ficheiro protegido ou inacessível no momento
        }
    }

    void ResourceMappingEngine::load_map(const std::filesystem::path& root_dir) {
        if (loaded_) return;

        // Mitigação de Rehashing: Reserva arbitrária de capacidade para evitar amortizações caras.
        // O Android tipicamente possui milhares de recursos.
        id_to_name_.reserve(10000);

        // 1. Tentar ler directamente do public.xml (mais fiável e limpo)
        const std::filesystem::path public_xml = root_dir / "res" / "values" / "public.xml";

        std::error_code ec;
        if (std::filesystem::exists(public_xml, ec) && !ec) {
            scan_public_xml(public_xml);
        }

        // 2. Fallback: Varrer ficheiros R.smali
        auto options = std::filesystem::directory_options::skip_permission_denied;
        auto fs_it = std::filesystem::recursive_directory_iterator(root_dir, options, ec);
        auto fs_end = std::filesystem::recursive_directory_iterator();

        while (fs_it != fs_end && !ec) {
            if (fs_it->is_regular_file(ec) && !ec) {
                const std::string filename = fs_it->path().filename().string();
                if (filename.starts_with("R$") && fs_it->path().extension() == ".smali") {
                    scan_r_smali(fs_it->path());
                }
            }
            fs_it.increment(ec);
            if (ec) ec.clear();
        }

        loaded_ = true;
    }

    std::vector<SearchResult> ResourceMappingEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        auto& root_dir = ctx.root_dir();

        auto [results, elapsed] = measure_execution([&]() {
            load_map(root_dir);
            std::vector<SearchResult> local_results;
            local_results.reserve(std::min<size_t>(config.max_results, id_to_name_.size()));

            if (config.query.empty()) {
                // Dump completo de mapeamento se query for vazia
                for (const auto& [id, name] : id_to_name_) {
                    if (local_results.size() >= static_cast<size_t>(config.max_results)) break;

                    SearchResult res;
                    res.line_content = name;
                    res.context = utils::to_hex(id); // Usa std::format optimizado do nosso utils
                    res.engine_name = this->name();
                    local_results.push_back(std::move(res));
                }
            } else {
                // Otimização inteligente: Se a query for perfeitamente convertível em Hex,
                // podemos fazer um lookup O(1) direto no Hash Map, evitando a iteração O(N)!
                std::string_view n_query = config.query;
                if (n_query.starts_with("0x")) n_query.remove_prefix(2);

                uint32_t query_as_id = 0;
                auto [ptr, conv_ec] = std::from_chars(n_query.data(), n_query.data() + n_query.size(), query_as_id, 16);

                if (conv_ec == std::errc() && ptr == n_query.data() + n_query.size()) {
                    // Consulta direta O(1)
                    auto it = id_to_name_.find(query_as_id);
                    if (it != id_to_name_.end()) {
                        SearchResult res;
                        res.line_content = it->second;
                        res.context = utils::to_hex(it->first);
                        res.engine_name = this->name();
                        local_results.push_back(std::move(res));
                    }
                }
                else {
                    // Busca textual genérica O(N)
                    for (const auto& [id, name] : id_to_name_) {
                        if (local_results.size() >= static_cast<size_t>(config.max_results)) break;

                        const std::string id_hex = utils::to_hex(id);
                        if (id_hex.find(config.query) != std::string::npos || name.find(config.query) != std::string::npos) {
                            SearchResult res;
                            res.line_content = name;
                            res.context = id_hex;
                            res.engine_name = this->name();
                            local_results.push_back(std::move(res));
                        }
                    }
                }
            }
            return local_results;
        });

        stats_.total_time = elapsed;
        stats_.matches_found = results.size();

        return results;
    }

    // Alinhamento da assinatura contratual para suportar retorno Zero-Copy garantido
    std::string_view ResourceMappingEngine::resolve_id(uint32_t id) const noexcept {
        auto it = id_to_name_.find(id);
        if (it != id_to_name_.end()) {
            return it->second;
        }
        return {};
    }

    std::unique_ptr<ISearchEngine> create_resource_mapping_engine() {
        return std::make_unique<ResourceMappingEngine>();
    }

} // namespace engines
