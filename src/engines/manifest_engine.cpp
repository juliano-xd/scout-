#include "engines/manifest_engine.hpp"
#include "utils/mmap_file.hpp"
#include "utils/string_utils.hpp"
#include <regex>
#include <chrono>
#include <fstream>

namespace engines {

    namespace {
        // Helper para extrair atributos de tags XML
        std::string get_attr(const std::string& tag, const std::string& attr) {
            std::regex reg(attr + "=\"([^\"]+)\"");
            std::smatch match;
            if (std::regex_search(tag, match, reg)) {
                return match[1].str();
            }
            return "";
        }
    }

    std::vector<SearchResult> ManifestEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        auto& root_dir = ctx.root_dir();
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<SearchResult> results;

        std::filesystem::path manifest_path = root_dir / "AndroidManifest.xml";
        if (!std::filesystem::exists(manifest_path)) return results;

        ManifestInfo info = parse_manifest(manifest_path);
        
        SearchResult res;
        res.file_path = "AndroidManifest.xml";
        res.engine_name = name();
        res.line_content = "Manifest Analysis Summary";
        
        // Serializar info para S-Expression no contexto do resultado
        auto node = serialize_info(info);
        res.context = node.to_string();
        
        results.push_back(std::move(res));

        auto end = std::chrono::high_resolution_clock::now();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        stats_.files_scanned = 1;
        stats_.matches_found = 1;

        return results;
    }

    ManifestEngine::ManifestInfo ManifestEngine::parse_manifest(const std::filesystem::path& path) {
        ManifestInfo info;
        std::ifstream file(path);
        if (!file.is_open()) return info;

        std::string line;
        std::string full_xml;
        while (std::getline(file, line)) {
            full_xml += line + " ";
        }

        // Extrair Package
        std::regex pkg_reg("<manifest[^>]+package=\"([^\"]+)\"");
        std::smatch m;
        if (std::regex_search(full_xml, m, pkg_reg)) {
            info.package = m[1].str();
        }

        // Security Linting
        if (full_xml.find("android:debuggable=\"true\"") != std::string::npos)
            info.security_alerts.push_back("debuggable-enabled");
        if (full_xml.find("android:allowBackup=\"true\"") != std::string::npos)
            info.security_alerts.push_back("allowBackup-enabled");

        // Extrair Permissões
        std::regex perm_reg("<uses-permission[^>]+android:name=\"([^\"]+)\"");
        auto perms_begin = std::sregex_iterator(full_xml.begin(), full_xml.end(), perm_reg);
        auto perms_end = std::sregex_iterator();
        for (auto i = perms_begin; i != perms_end; ++i) {
            info.permissions.push_back((*i)[1].str());
        }

        // Extrair Componentes (simplificado)
        auto extract_components = [&](const std::string& type, const std::string& tag_name) {
            std::regex comp_reg("<" + tag_name + "([^>]+)>");
            auto comp_begin = std::sregex_iterator(full_xml.begin(), full_xml.end(), comp_reg);
            auto comp_end = std::sregex_iterator();
            for (auto i = comp_begin; i != comp_end; ++i) {
                std::string tag_content = (*i)[1].str();
                ManifestInfo::Component comp;
                comp.name = get_attr(tag_content, "android:name");
                comp.type = type;
                comp.exported = (get_attr(tag_content, "android:exported") == "true");
                
                // Extrair ações de intent-filters (aproximado via regex no bloco da tag)
                // Nota: Em um parser real usaríamos uma árvore, aqui simulamos a busca por <action android:name="..."/>
                std::regex action_re("<action[^>]+android:name=\"([^\"]+)\"");
                auto actions_begin = std::sregex_iterator(tag_content.begin(), tag_content.end(), action_re);
                auto actions_end = std::sregex_iterator();
                for (auto j = actions_begin; j != actions_end; ++j) {
                    comp.intent_filters.push_back((*j)[1].str());
                }

                info.components.push_back(std::move(comp));
            }
        };

        extract_components("activity", "activity");
        extract_components("service", "service");
        extract_components("receiver", "receiver");
        extract_components("provider", "provider");

        return info;
    }

    sexpr::Node ManifestEngine::serialize_info(const ManifestInfo& info) {
        auto n = sexpr::form("manifest-recon");
        n.kv("package", sexpr::string(info.package));
        
        auto perms = sexpr::list();
        for (const auto& p : info.permissions) perms.push(sexpr::string(p));
        n.kv("permissions", perms);

        auto comps = sexpr::list();
        for (const auto& c : info.components) {
            auto cn = sexpr::form("component");
            cn.kv("name", sexpr::string(c.name));
            cn.kv("type", sexpr::string(c.type));
            cn.kv("exported", sexpr::boolean(c.exported));
            
            auto intents = sexpr::list();
            for (const auto& i : c.intent_filters) intents.push(sexpr::string(i));
            cn.kv("intents", intents);
            
            comps.push(cn);
        }
        n.kv("components", comps);

        auto alerts = sexpr::list();
        for (const auto& s : info.security_alerts) alerts.push(sexpr::string(s));
        n.kv("security-alerts", alerts);

        return n;
    }

    std::unique_ptr<ISearchEngine> create_manifest_engine() {
        return std::make_unique<ManifestEngine>();
    }

} // namespace engines
