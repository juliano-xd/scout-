#include "engines/resource_mapping_engine.hpp"
#include "utils/filesystem.hpp"
#include "utils/string_utils.hpp"
#include "utils/mmap_file.hpp"
#include <fstream>
#include <regex>
#include <chrono>

namespace engines {

    void ResourceMappingEngine::scan_public_xml(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file.is_open()) return;

        std::string line;
        // <public type="string" name="app_name" id="0x7f0b0001" />
        std::regex re("type=\"([^\"]+)\"\\s+name=\"([^\"]+)\"\\s+id=\"(0x[0-9a-fA-F]+)\"");
        std::smatch match;

        while (std::getline(file, line)) {
            if (std::regex_search(line, match, re)) {
                std::string type = match[1];
                std::string name = match[2];
                std::string id_str = match[3];
                try {
                    uint32_t id = std::stoul(id_str, nullptr, 16);
                    id_to_name_[id] = type + "/" + name;
                } catch (...) {}
            }
        }
    }

    void ResourceMappingEngine::scan_r_smali(const std::filesystem::path& path) {
        utils::MappedFile mfile(path);
        if (!mfile.is_open()) return;

        utils::LineIterator it(mfile.view());
        std::string_view line;
        // .field public static final app_name:I = 0x7f0b0001
        std::regex re("\\.field\\s+.*?\\s+([^:\\s]+):I\\s+=\\s+(0x[0-9a-fA-F]+)");
        std::smatch match;

        std::string type = path.stem().string(); // R$string.smali -> string
        if (type.find('$') != std::string::npos) {
            type = type.substr(type.find('$') + 1);
        }

        while (it.next(line)) {
            std::string s_line(line);
            if (std::regex_search(s_line, match, re)) {
                std::string name = match[1];
                std::string id_str = match[2];
                try {
                    uint32_t id = std::stoul(id_str, nullptr, 16);
                    id_to_name_[id] = type + "/" + name;
                } catch (...) {}
            }
        }
    }

    void ResourceMappingEngine::load_map(const std::filesystem::path& root_dir) {
        if (loaded_) return;

        // Tentar public.xml
        std::filesystem::path public_xml = root_dir / "res" / "values" / "public.xml";
        if (std::filesystem::exists(public_xml)) {
            scan_public_xml(public_xml);
        }

        // Tentar R.smali
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root_dir)) {
            if (entry.is_regular_file() && entry.path().filename().string().starts_with("R$") && entry.path().extension() == ".smali") {
                scan_r_smali(entry.path());
            }
        }

        loaded_ = true;
    }

    std::vector<SearchResult> ResourceMappingEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        auto& root_dir = ctx.root_dir();
        auto start = std::chrono::high_resolution_clock::now();
        load_map(root_dir);

        std::vector<SearchResult> results;
        
        if (config.query.empty()) {
            // Se query vazia, retornar estatísticas ou alguns mapeamentos?
            // Por enquanto, retorna vazio ou o mapa todo se max_results permitir
            for (const auto& [id, name] : id_to_name_) {
                if (results.size() >= config.max_results) break;
                SearchResult res;
                res.line_content = name;
                res.context = utils::to_hex(id);
                res.engine_name = this->name();
                results.push_back(std::move(res));
            }
        } else {
            // Busca por ID ou nome
            for (const auto& [id, name] : id_to_name_) {
                std::string id_hex = utils::to_hex(id);
                if (id_hex.find(config.query) != std::string::npos || name.find(config.query) != std::string::npos) {
                    SearchResult res;
                    res.line_content = name;
                    res.context = id_hex;
                    res.engine_name = this->name();
                    results.push_back(std::move(res));
                    if (results.size() >= config.max_results) break;
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        stats_.matches_found = results.size();
        
        return results;
    }

    std::unique_ptr<ISearchEngine> create_resource_mapping_engine() {
        return std::make_unique<ResourceMappingEngine>();
    }

} // namespace engines
