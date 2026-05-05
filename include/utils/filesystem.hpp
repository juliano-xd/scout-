#pragma once

#include <optional>
#include <string_view>
#include <stdexcept>
#include <filesystem>
#include <system_error> // Necessário para std::error_code

namespace fs = std::filesystem;

/**
 * @file file_search.hpp
 * @brief Utilitário de sistema de ficheiros para busca recursiva.
 * @details Arquitetura refactorizada para prevenir crashes durante I/O Operations
 * utilizando iteração no-throw (std::error_code) e mitigação de TOCTOU.
 */
namespace utils {

    /**
     * @brief Busca recursivamente um ficheiro num diretório.
     * @param pasta_base Path inicial da busca.
     * @param nome_arquivo Nome exato do ficheiro procurado.
     * @return std::optional contendo o path absoluto/resolvido se encontrado.
     * @throws std::invalid_argument apenas se a pasta inicial for estritamente inválida.
     */
    [[nodiscard]] inline std::optional<fs::path> buscar_arquivo_recursivo(
        const fs::path& pasta_base,
        std::string_view nome_arquivo)
    {
        std::error_code ec;

        // Mitigação de TOCTOU: is_directory(path, ec) retorna false em caso de erro
        // (ex: não existe), testando a existência e o tipo numa única chamada atómica.
        if (!fs::is_directory(pasta_base, ec) || ec) [[unlikely]] {
            throw std::invalid_argument("A pasta base é inválida, não existe ou é inacessível.");
        }

        // Pré-alocação estática do target: Evita alocações O(N) na heap dentro do loop
        // a cada comparação de string no path::filename().
        const fs::path target_name(nome_arquivo);

        // Ignorar ativamente pastas com acessos negados (Permission Denied).
        const auto options = fs::directory_options::skip_permission_denied;

        // Construção do iterador com error_code garante que a root folder não lança exceção.
        auto it = fs::recursive_directory_iterator(pasta_base, options, ec);
        const auto end = fs::recursive_directory_iterator();

        if (ec) [[unlikely]] {
            return std::nullopt; // Falha silenciosa se a raiz falhar logo após a verificação
        }

        // OPTIMIZAÇÃO CRÍTICA: Iteração manual ao invés de Range-Based For.
        // Motivo: Se uma subpasta dentro da árvore sofrer um erro I/O fatal (EIO, bad sector),
        // o iterador avança silenciosamente ou termina em vez de abortar todo o Scout++ com throw.
        while (it != end) {
            // is_regular_file(ec) suprime exceções em symlinks partidos.
            if (it->is_regular_file(ec) && !ec) {

                if (it->path().filename() == target_name) [[unlikely]] {
                    // Match encontrado: Early return
                    return it->path();
                }
            }

            // Avanço manual no-throw. Se a estrutura subjacente do SO retornar falha
            // na leitura do próximo nó, `ec` é preenchido e o iterador entra em estado `end`.
            it.increment(ec);
            if (ec) [[unlikely]] {
                // Recuperação de erro: limpamos o estado para não afetar loops subsequentes
                ec.clear();
            }
        }

        return std::nullopt;
    }

} // namespace utils
