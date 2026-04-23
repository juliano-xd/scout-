#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

namespace core {
    struct MethodInfo {
        std::string access_modifiers;
        std::string name;
        std::string signature;
    };

    class SmaliParser {
    public:
        static std::vector<MethodInfo> extract_methods(const std::filesystem::path& file_path) {
            std::vector<MethodInfo> methods;
            std::ifstream file(file_path);
            if (!file.is_open()) return methods;

            std::string line;
            while (std::getline(file, line)) {
                // Trim leading whitespace
                size_t first_non_space = line.find_first_not_of(" \t");
                if (first_non_space == std::string::npos) continue;
                
                std::string trimmed = line.substr(first_non_space);
                
                if (trimmed.find(".method ") == 0) {
                    std::string method_decl = trimmed.substr(8); // skip ".method "
                    
                    // Parse method declaration
                    // e.g. "public static doLogin(Ljava/lang/String;)Z"
                    size_t paren_pos = method_decl.find('(');
                    if (paren_pos != std::string::npos) {
                        std::string signature = method_decl.substr(paren_pos);
                        
                        std::string before_paren = method_decl.substr(0, paren_pos);
                        size_t last_space = before_paren.find_last_of(' ');
                        
                        std::string name;
                        std::string modifiers;
                        
                        if (last_space != std::string::npos) {
                            name = before_paren.substr(last_space + 1);
                            modifiers = before_paren.substr(0, last_space);
                        } else {
                            name = before_paren;
                            modifiers = "";
                        }
                        
                        methods.push_back({modifiers, name, signature});
                    }
                }
            }
            return methods;
        }
    };
}
