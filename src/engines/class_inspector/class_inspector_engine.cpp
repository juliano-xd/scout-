#include "../../../include/engines/class_inspector/class_inspector_engine.hpp"
#include "../../../include/core/scanner.hpp"
#include "../../../include/utils/string_utils.hpp"
#include "../../../include/utils/mmap_file.hpp"

#include <chrono>
#include <system_error>
#include <format>
#include <string_view>

namespace engines {

    std::vector<SearchResult> ClassInspectorEngine::search(
        core::AnalysisContext& ctx,
        const SearchConfig& config
    ) {
        const auto& root_dir = ctx.root_dir();
        const auto start = std::chrono::high_resolution_clock::now();
        std::vector<SearchResult> results;

        if (config.query.empty()) return results;

        // Tentar encontrar o arquivo da classe no contexto mapeado
        const auto class_file = core::find_class_file(root_dir, config.query);
        if (!class_file) return results;

        const ClassStructure info = inspect_file(*class_file);

        SearchResult res;
        res.file_path = std::filesystem::relative(*class_file, root_dir);
        res.engine_name = name();

        // Optimização C++20: std::format elimina as invocações dinâmicas de operator+
        res.line_content = std::format("Class DNA: {}", config.query);

        const auto node = serialize_structure(info);
        res.context = node.to_string();

        results.push_back(std::move(res));

        const auto end = std::chrono::high_resolution_clock::now();
        stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        stats_.files_scanned = 1;
        stats_.matches_found = 1;

        return results;
    }

    ClassInspectorEngine::ClassStructure ClassInspectorEngine::inspect_file(const std::filesystem::path& file) {
        ClassStructure info;

        try {
            // Mapeamento RAII strict. Sem necessidade de verificações "is_open()" ou "sleeps" mágicos.
            // O sistema operacional resolve o I/O na melhor janela de tempo.
            utils::MappedFile mfile(file);
            if (mfile.is_empty()) return info;

            const std::string_view data = mfile.view();
            utils::LineIterator it(data);
            std::string_view line;

            // Motor O(N) para extracção do layout lógico da classe Smali
            while (it.next(line)) {
                const std::string_view trimmed = utils::trim(line);
                if (trimmed.empty()) continue;

                // A utilização de emplace_back + substr (com conversão implícita para std::string)
                // reduz os ciclos no invocador do construtor
                if (trimmed.starts_with(".class ")) {
                    const size_t pos = trimmed.find_last_of(' ');
                    if (pos != std::string_view::npos) {
                        info.name = trimmed.substr(pos + 1);
                    }
                }
                else if (trimmed.starts_with(".super ")) {
                    const size_t pos = trimmed.find_last_of(' ');
                    if (pos != std::string_view::npos) {
                        info.super = trimmed.substr(pos + 1);
                    }
                }
                else if (trimmed.starts_with(".implements ")) {
                    const size_t pos = trimmed.find_last_of(' ');
                    if (pos != std::string_view::npos) {
                        info.interfaces.emplace_back(trimmed.substr(pos + 1));
                    }
                }
                else if (trimmed.starts_with(".field ")) {
                    info.fields.emplace_back(trimmed.substr(7));
                }
                else if (trimmed.starts_with(".method ")) {
                    info.methods.emplace_back(trimmed.substr(8));
                }
            }
        } catch (const std::system_error&) {
            // Captura silenciosa de falhas de partilha ou exclusividade de ficheiros no SO
        }

        return info;
    }

    sexpr::Node ClassInspectorEngine::serialize_structure(const ClassStructure& info) {
        auto n = sexpr::form("class-dna");
        n.kv("name", sexpr::string(info.name));
        n.kv("super", sexpr::string(info.super));

        auto interfaces = sexpr::list();
        for (const auto& i : info.interfaces) {
            interfaces.push(sexpr::string(i));
        }
        // Movimentação Zero-Copy da sub-árvore inteira
        n.kv("interfaces", std::move(interfaces));

        auto fields = sexpr::list();
        for (const auto& f : info.fields) {
            fields.push(sexpr::string(f));
        }
        n.kv("fields", std::move(fields));

        auto methods = sexpr::list();
        for (const auto& m : info.methods) {
            auto mn = sexpr::form("method");
            mn.kv("signature", sexpr::string(m));

            // Converter explicitamente a std::string armazenada para std::string_view
            // antes da busca (find) para garantir velocidade máxima no match de segurança.
            const std::string_view m_view = m;

            if (m_view.find("getDeviceId") != std::string_view::npos || m_view.find("getImei") != std::string_view::npos) {
                mn.kv("risk", sexpr::string("privacy-leak"));
                mn.kv("permission", sexpr::string("android.permission.READ_PHONE_STATE"));
            }
            else if (m_view.find("getLastKnownLocation") != std::string_view::npos) {
                mn.kv("risk", sexpr::string("location-tracking"));
                mn.kv("permission", sexpr::string("android.permission.ACCESS_FINE_LOCATION"));
            }

            methods.push(std::move(mn));
        }
        n.kv("methods", std::move(methods));

        return n;
    }

    std::unique_ptr<ISearchEngine> create_class_inspector_engine() {
        return std::make_unique<ClassInspectorEngine>();
    }

} // namespace engines
