#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <system_error>

#include "../utils/mmap_file.hpp"
#include "../utils/string_utils.hpp"

/**
 * @file smali_parser.hpp
 * @brief Utilitário de parsing de métodos Smali.
 * @details Refatorizado para C++26. O parsing intermédio é Zero-Allocation (via std::string_view),
 * instanciando memória dinâmica apenas para a estrutura MethodInfo final, garantindo
 * a segurança do ciclo de vida após o encerramento do ficheiro (RAII).
 */
namespace core {

    struct MethodInfo {
        std::string access_modifiers;
        std::string name;
        std::string signature;
    };

    class SmaliParser {
    public:
        /**
         * @brief Extrai todos os métodos declarados num ficheiro Smali.
         * @param file_path O caminho para o ficheiro .smali.
         * @return Vector contendo as informações (modificadores, nome, assinatura) de cada método.
         */
        [[nodiscard]] static std::vector<MethodInfo> extract_methods(const std::filesystem::path& file_path) {
            std::vector<MethodInfo> methods;

            try {
                // Mapeamento RAII Estrito: Sem I/O bloqueante explícito.
                utils::MappedFile mfile(file_path);
                if (mfile.is_empty()) return methods;

                // Pre-alloc mitigatório: A maioria das classes tem <= 32 métodos.
                // Impede o redimensionamento amortizado contínuo do vector.
                methods.reserve(32);

                utils::LineIterator it(mfile.view());
                std::string_view line;

                while (it.next(line)) {
                    // Operação O(1) Zero-Copy
                    const std::string_view trimmed = utils::trim(line);

                    if (trimmed.starts_with(".method ")) {
                        // .method public static doLogin(Ljava/lang/String;)Z
                        const std::string_view method_decl = trimmed.substr(8); // Salta ".method "

                        const size_t paren_pos = method_decl.find('(');
                        if (paren_pos != std::string_view::npos) {

                            // Todo este parsing é resolvido deslizando iteradores (0 alocações)
                            const std::string_view signature = method_decl.substr(paren_pos);
                            const std::string_view before_paren = method_decl.substr(0, paren_pos);
                            const size_t last_space = before_paren.find_last_of(' ');

                            std::string_view name;
                            std::string_view modifiers;

                            if (last_space != std::string_view::npos) {
                                name = before_paren.substr(last_space + 1);
                                modifiers = before_paren.substr(0, last_space);
                            } else {
                                name = before_paren;
                                // modifiers fica vazio (default de std::string_view)
                            }

                            // Materialização segura para a Heap via emplace_back.
                            // std::string_view invoca o construtor explícito de std::string
                            methods.emplace_back(MethodInfo{
                                std::string(modifiers),
                                std::string(name),
                                std::string(signature)
                            });
                        }
                    }
                }
            } catch (const std::system_error&) {
                // Falhas de partilha ou acesso negado (ex: hardware) geram abandono seguro do scan
            }

            return methods;
        }
    };

} // namespace core
