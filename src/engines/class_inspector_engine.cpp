#include "engines/class_inspector_engine.hpp"
#include "core/scanner.hpp"
#include "utils/string_utils.hpp"
#include "utils/mmap_file.hpp"
#include <fstream>
#include <chrono>
#include <thread>

namespace engines {

    std::vector<SearchResult> ClassInspectorEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        auto& root_dir = ctx.root_dir();
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<SearchResult> results;

        if (config.query.empty()) return results;

        // Tentar encontrar o arquivo da classe
        auto class_file = core::find_class_file(root_dir, config.query);
        if (!class_file) return results;

        ClassStructure info = inspect_file(*class_file);
        
        SearchResult res;
        res.file_path = std::filesystem::relative(*class_file, root_dir);
        res.engine_name = name();
        res.line_content = "Class DNA: " + config.query;
        
        auto node = serialize_structure(info);
        res.context = node.to_string();
        
        results.push_back(std::move(res));

        auto end = std::chrono::high_resolution_clock::now();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        stats_.files_scanned = 1;
        stats_.matches_found = 1;

        return results;
    }

    ClassInspectorEngine::ClassStructure ClassInspectorEngine::inspect_file(const std::filesystem::path& file) {
        ClassStructure info;
        if (!std::filesystem::exists(file)) return info;

        // Aguardar sincronização de metadados se necessário
        int retries = 5;
        while (retries-- > 0 && std::filesystem::file_size(file) == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        if (std::filesystem::file_size(file) == 0) return info;

        utils::MappedFile mfile(file);
        std::string_view data;
        std::string fallback_buffer;

        if (mfile.is_open() && mfile.size() > 0) {
            data = mfile.view();
        } else {
            std::ifstream ifs(file, std::ios::binary);
            if (ifs.is_open()) {
                fallback_buffer = std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                data = fallback_buffer;
            }
        }

        if (data.empty()) return info;

        utils::LineIterator it(data);
        std::string_view line;
        while (it.next(line)) {
            std::string_view trimmed = utils::trim(line);
            if (trimmed.empty()) continue;

            if (trimmed.starts_with(".class ")) {
                size_t pos = trimmed.find_last_of(' ');
                if (pos != std::string_view::npos) info.name = std::string(trimmed.substr(pos + 1));
            } else if (trimmed.starts_with(".super ")) {
                size_t pos = trimmed.find_last_of(' ');
                if (pos != std::string_view::npos) info.super = std::string(trimmed.substr(pos + 1));
            } else if (trimmed.starts_with(".implements ")) {
                size_t pos = trimmed.find_last_of(' ');
                if (pos != std::string_view::npos) info.interfaces.push_back(std::string(trimmed.substr(pos + 1)));
            } else if (trimmed.starts_with(".field ")) {
                info.fields.push_back(std::string(trimmed.substr(7)));
            } else if (trimmed.starts_with(".method ")) {
                info.methods.push_back(std::string(trimmed.substr(8)));
            }
        }

        return info;
    }

    sexpr::Node ClassInspectorEngine::serialize_structure(const ClassStructure& info) {
        auto n = sexpr::form("class-dna");
        n.kv("name", sexpr::string(info.name));
        n.kv("super", sexpr::string(info.super));
        
        auto interfaces = sexpr::list();
        for (const auto& i : info.interfaces) interfaces.push(sexpr::string(i));
        n.kv("interfaces", interfaces);

        auto fields = sexpr::list();
        for (const auto& f : info.fields) fields.push(sexpr::string(f));
        n.kv("fields", fields);

        auto methods = sexpr::list();
        for (const auto& m : info.methods) {
            auto mn = sexpr::form("method");
            mn.kv("signature", sexpr::string(m));
            
            if (m.find("getDeviceId") != std::string::npos || m.find("getImei") != std::string::npos) {
                mn.kv("risk", sexpr::string("privacy-leak"));
                mn.kv("permission", sexpr::string("android.permission.READ_PHONE_STATE"));
            } else if (m.find("getLastKnownLocation") != std::string::npos) {
                mn.kv("risk", sexpr::string("location-tracking"));
                mn.kv("permission", sexpr::string("android.permission.ACCESS_FINE_LOCATION"));
            }
            
            methods.push(mn);
        }
        n.kv("methods", methods);

        return n;
    }

    std::unique_ptr<ISearchEngine> create_class_inspector_engine() {
        return std::make_unique<ClassInspectorEngine>();
    }

} // namespace engines
