#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <sstream>

namespace utils {
    inline std::string json_to_sexpr(const nlohmann::json& j) {
        std::stringstream ss;
        if (j.is_null()) {
            ss << "null";
        } else if (j.is_boolean()) {
            ss << (j.get<bool>() ? "true" : "false");
        } else if (j.is_number_integer()) {
            ss << j.get<long long>();
        } else if (j.is_number_unsigned()) {
            ss << j.get<unsigned long long>();
        } else if (j.is_number_float()) {
            ss << j.get<double>();
        } else if (j.is_string()) {
            // Escape string for sexpr (simple version)
            std::string s = j.get<std::string>();
            ss << "\"";
            for (char c : s) {
                if (c == '"' || c == '\\') ss << '\\';
                ss << c;
            }
            ss << "\"";
        } else if (j.is_array()) {
            ss << "(";
            bool first = true;
            for (const auto& item : j) {
                if (!first) ss << " ";
                ss << json_to_sexpr(item);
                first = false;
            }
            ss << ")";
        } else if (j.is_object()) {
            ss << "(";
            bool first = true;
            for (auto it = j.begin(); it != j.end(); ++it) {
                if (!first) ss << " ";
                ss << "(" << it.key() << " " << json_to_sexpr(it.value()) << ")";
                first = false;
            }
            ss << ")";
        }
        return ss.str();
    }
}
