#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <execution>
#include <mutex>
#include <system_error>
#include <atomic>      // C++11: Para short-circuit lock-free em processamento paralelo
#include <span>        // C++20: Zero-cost array passing
#include <type_traits> // C++11/17: Para inspecionar o retorno de lambdas

namespace core {

    /**
     * @brief Utilitário nativo de parsing de nomenclatura de classes Java/Dalvik.
     * @details Optimizado para minimizar alocações (Heap) operando sobre Views.
     */
    struct ClassInfo {
        std::string package_path;
        std::string class_name;
        std::string file_name;

        [[nodiscard]] static ClassInfo parse(std::string_view dalvik_name) {
            // Remove 'L' prefix e ';' suffix se presentes em O(1)
            if (dalvik_name.size() >= 2 && dalvik_name.front() == 'L' && dalvik_name.back() == ';') {
                dalvik_name.remove_prefix(1);
                dalvik_name.remove_suffix(1);
            }

            ClassInfo info;

            // Suporta tanto dot-notation (com.ex.A) como slash-notation (com/ex/A)
            const size_t sep_pos = dalvik_name.find_last_of("./");

            if (sep_pos != std::string_view::npos) {
                // Aloca apenas a porção do pacote e formata os separadores
                info.package_path = std::string(dalvik_name.substr(0, sep_pos));
                std::ranges::replace(info.package_path, '.', '/');

                info.class_name = std::string(dalvik_name.substr(sep_pos + 1));
            } else {
                info.package_path = "";
                info.class_name = std::string(dalvik_name);
            }

            // Evita múltiplas pequenas alocações
            info.file_name.reserve(info.class_name.size() + 6);
            info.file_name = info.class_name;
            info.file_name += ".smali";

            return info;
        }
    };

    /**
     * @brief Verifica se um caminho deve ser incluído na busca com base em filtros.
     * @details Utiliza std::span (C++20) para evitar cópias ocultas de std::vector,
     * e std::ranges para iteração funcional com curto-circuito (Early-Out).
     */
    [[nodiscard]] inline bool is_path_filtered(
        const std::filesystem::path& p,
        std::span<const std::string> include_dirs,
        std::span<const std::string> exclude_dirs)
    {
        if (include_dirs.empty() && exclude_dirs.empty()) return true;

        const std::string path_str = p.string();

        // Regra de Inclusão: Caminho deve conter pelo menos UM dos targets
        if (!include_dirs.empty()) {
            const bool found_include = std::ranges::any_of(include_dirs, [&](const std::string& inc) {
                return path_str.find(inc) != std::string::npos;
            });
            if (!found_include) return false;
        }

        // Regra de Exclusão: Caminho não deve conter NENHUM dos targets
        if (!exclude_dirs.empty()) {
            const bool found_exclude = std::ranges::any_of(exclude_dirs, [&](const std::string& exc) {
                return path_str.find(exc) != std::string::npos;
            });
            if (found_exclude) return false;
        }

        return true;
    }

    /**
     * @brief Busca o arquivo .smali correspondente a uma classe Dalvik.
     * @param search_dir Diretório base onde a busca inicia.
     * @param dalvik_name Nome da classe (ex: Lcom/ex/A; ou com.ex.A).
     * @return Caminho absoluto, ou nullopt em caso de falha/ausência.
     */
    [[nodiscard]] inline std::optional<std::filesystem::path> find_class_file(
        const std::filesystem::path& search_dir,
        std::string_view dalvik_name)
    {
        std::error_code ec;
        if (!std::filesystem::is_directory(search_dir, ec) || ec) {
            return std::nullopt;
        }

        const auto info = ClassInfo::parse(dalvik_name);

        // Fast-path: Verifica heurística de diretórios comuns em O(1) antes de fazer o recursive scan
        static constexpr std::string_view common_dirs[] = {
            "smali", "smali_classes2", "smali_classes3", "smali_classes4", "smali_classes5", "smali_classes6"
        };

        for (const auto d : common_dirs) {
            std::filesystem::path p = search_dir / d / info.package_path / info.file_name;
            if (std::filesystem::exists(p, ec) && !ec) return p;
        }

        // Slow-path: Generic recursive scan (com mitigação contra throw de FileSystem)
        const auto options = std::filesystem::directory_options::skip_permission_denied;
        auto it = std::filesystem::recursive_directory_iterator(search_dir, options, ec);
        auto end = std::filesystem::recursive_directory_iterator();

        while (it != end && !ec) {
            if (it->is_regular_file(ec) && !ec && it->path().filename() == info.file_name) {
                if (info.package_path.empty()) return it->path();

                const std::string path_str = it->path().string();
                if (path_str.find(info.package_path) != std::string::npos) {
                    return it->path();
                }
            }
            it.increment(ec);
            if (ec) ec.clear();
        }

        return std::nullopt;
    }

    /**
     * @brief Varre o FS e encontra classes .smali cujo caminho contenha a string especificada.
     * @details Implementa Short-Circuit Atómico: aborta a pesquisa instantaneamente quando o limite é atingido.
     */
    [[nodiscard]] inline std::vector<std::filesystem::path> find_classes_containing(
        const std::filesystem::path& search_dir,
        std::string_view query,
        std::span<const std::string> include_dirs = {},
        std::span<const std::string> exclude_dirs = {},
        size_t limit = 0 // NOVO: Parâmetro de paragem antecipada (Short-Circuit)
    ) {
        std::vector<std::filesystem::path> results;
        std::error_code ec;

        if (!std::filesystem::is_directory(search_dir, ec) || ec) return results;

        std::vector<std::filesystem::path> files_to_check;
        files_to_check.reserve(10000); // Mitiga alocações durante listagem pesada do FS

        const auto options = std::filesystem::directory_options::skip_permission_denied;
        auto it = std::filesystem::recursive_directory_iterator(search_dir, options, ec);
        auto end = std::filesystem::recursive_directory_iterator();

        while (it != end && !ec) {
            if (it->is_regular_file(ec) && !ec && it->path().extension() == ".smali") {
                if (is_path_filtered(it->path(), include_dirs, exclude_dirs)) {
                    files_to_check.push_back(it->path());
                }
            }
            it.increment(ec);
            if (ec) ec.clear();
        }

        std::mutex mtx;

        // C++26: Variável atómica para abortar threads no par_unseq imediatamente
        std::atomic<bool> limit_reached{false};

        // Análise textual paralela do caminho (Semântica C++17/20)
        std::for_each(std::execution::par_unseq, files_to_check.begin(), files_to_check.end(),
            [&](const std::filesystem::path& p) {
                // Short-Circuit Check (memory_order_relaxed custa virtualmente 0 ciclos)
                if (limit_reached.load(std::memory_order_relaxed)) return;

                if (p.string().find(query) != std::string::npos) {
                    std::lock_guard<std::mutex> lock(mtx);

                    // Double check dentro da secção crítica
                    if (limit > 0 && results.size() >= limit) {
                        limit_reached.store(true, std::memory_order_relaxed);
                        return;
                    }

                    results.push_back(p);

                    // Se atingirmos o limite após inserir, avisa todas as threads para abortarem
                    if (limit > 0 && results.size() >= limit) {
                        limit_reached.store(true, std::memory_order_relaxed);
                    }
                }
            }
        );

        return results;
    }

    /**
     * @brief Interface Genérica O(N) que varre .smali recursivamente executando callbacks Paralelas.
     * @param callback Função invocável (lambda) a aplicar sobre cada std::filesystem::path. Pode retornar `bool` (false=abortar).
     */
    template<typename Func>
    inline void scan_files(
        const std::filesystem::path& search_dir,
        Func callback,
        std::span<const std::string> include_dirs = {},
        std::span<const std::string> exclude_dirs = {}
    ) {
        std::error_code ec;
        if (!std::filesystem::is_directory(search_dir, ec) || ec) return;

        std::vector<std::filesystem::path> files;
        files.reserve(20000); // Pre-alloc proativo para APKs grandes

        const auto options = std::filesystem::directory_options::skip_permission_denied;
        auto it = std::filesystem::recursive_directory_iterator(search_dir, options, ec);
        auto end = std::filesystem::recursive_directory_iterator();

        // Extração assíncrona do layout de ficheiros sem risco de interrupção I/O
        while (it != end && !ec) {
            if (it->is_regular_file(ec) && !ec && it->path().extension() == ".smali") {
                if (is_path_filtered(it->path(), include_dirs, exclude_dirs)) {
                    files.push_back(it->path());
                }
            }
            it.increment(ec);
            if (ec) ec.clear();
        }

        std::atomic<bool> abort_scan{false};

        // Executa a callback invocável através de N threads nativas disponíveis
        std::for_each(std::execution::par_unseq, files.begin(), files.end(),
            [&](const std::filesystem::path& p) {
                // Early Out Global
                if (abort_scan.load(std::memory_order_relaxed)) return;

                // C++17 constexpr if: Inspeciona em tempo de compilação a assinatura da lambda.
                // Se ela retornar bool, respeitamos a ordem de short-circuit!
                if constexpr (std::is_same_v<std::invoke_result_t<Func, const std::filesystem::path&>, bool>) {
                    if (!callback(p)) {
                        abort_scan.store(true, std::memory_order_relaxed);
                    }
                } else {
                    callback(p);
                }
            }
        );
    }

} // namespace core
