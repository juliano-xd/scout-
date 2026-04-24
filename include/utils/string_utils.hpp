#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace utils {

    inline std::string_view trim(std::string_view s) {
        auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) return "";
        auto last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, (last - first + 1));
    }

    inline std::string to_hex(uint32_t val) {
        std::stringstream ss;
        ss << "0x" << std::setfill('0') << std::setw(8) << std::hex << val;
        return ss.str();
    }

    inline bool is_base64(std::string_view s) {
        if (s.length() < 12) return false;
        for (char c : s) {
            if (!isalnum(c) && c != '+' && c != '/' && c != '=') return false;
        }
        return true;
    }

    inline std::string decode_base64(std::string_view in) {
        static const std::string b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::vector<int> T(256, -1);
        for (int i = 0; i < 64; i++) T[b64[i]] = i;

        int val = 0, valb = -8;
        std::string out;
        for (unsigned char c : in) {
            if (T[c] == -1) break;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) {
                out.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return out;
    }

    inline double calculate_entropy(std::string_view s) {
        if (s.empty()) return 0.0;
        std::map<char, int> freq;
        for (char c : s) freq[c]++;
        
        double entropy = 0.0;
        for (auto const& [c, f] : freq) {
            double p = static_cast<double>(f) / s.length();
            entropy -= p * log2(p);
        }
        return entropy;
    }

    inline bool contains_insensitive(std::string_view haystack, std::string_view needle) {
        auto it = std::search(
            haystack.begin(), haystack.end(),
            needle.begin(), needle.end(),
            [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
        );
        return it != haystack.end();
    }

    struct LineIterator {
        std::string_view data;
        size_t pos = 0;
        LineIterator(std::string_view d) : data(d) {}
        bool next(std::string_view& line) {
            if (pos >= data.size()) return false;
            size_t end = data.find('\n', pos);
            if (end == std::string_view::npos) {
                line = data.substr(pos);
                pos = data.size();
            } else {
                line = data.substr(pos, end - pos);
                pos = end + 1;
            }
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            return true;
        }
    };

    inline std::vector<std::string> extract_smali_method(const std::filesystem::path& path, const std::string& method_sig) {
        std::ifstream file(path);
        std::string line;
        std::vector<std::string> body;
        bool in_method = false;
        while (std::getline(file, line)) {
            std::string_view trimmed = trim(line);
            if (trimmed.starts_with(".method ") && trimmed.find(method_sig) != std::string_view::npos) {
                in_method = true;
                continue;
            }
            if (in_method) {
                if (trimmed.starts_with(".end method")) break;
                body.push_back(line);
            }
        }
        return body;
    }

} // namespace utils
