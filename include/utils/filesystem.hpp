#pragma once
#include <optional>
#include <string_view>
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

namespace utils {
    /**
     * @brief Busca um arquivo recursivamente pelo nome em uma pasta
     * @param pasta_base Pasta inicial da busca
     * @param nome_arquivo Nome do arquivo a ser procurado
     * @return Caminho completo do arquivo se encontrado, ou std::nullopt se não achado
     */
    inline std::optional<fs::path> buscar_arquivo_recursivo(const fs::path& pasta_base, std::string_view nome_arquivo) {
        if (!fs::exists(pasta_base) || !fs::is_directory(pasta_base)) {
            throw std::invalid_argument("A pasta base não existe ou não é um diretório válido!");
        }

        fs::path target_name(nome_arquivo);
        auto options = fs::directory_options::skip_permission_denied;

        for (const auto& entrada : fs::recursive_directory_iterator(pasta_base, options)) {
            if (entrada.is_regular_file()) {
                if (entrada.path().filename() == target_name) {
                    return entrada.path();
                }
            }
        }
        return std::nullopt;
    }
}
