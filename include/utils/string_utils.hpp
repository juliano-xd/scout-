#pragma once

#include <string_view>
#include <algorithm>
#include <cctype>

namespace utils {

    /**
     * @brief Trim whitespace from both ends of a string_view.
     */
    inline std::string_view trim(std::string_view sv) {
        auto first = sv.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) return {};
        auto last = sv.find_last_not_of(" \t\r\n");
        return sv.substr(first, last - first + 1);
    }

    /**
     * @brief Case-insensitive substring search for string_view.
     */
    inline bool contains_insensitive(std::string_view haystack, std::string_view needle) {
        if (needle.empty()) return true;
        auto it = std::search(
            haystack.begin(), haystack.end(),
            needle.begin(), needle.end(),
            [](unsigned char ch1, unsigned char ch2) {
                return std::tolower(ch1) == std::tolower(ch2);
            }
        );
        return it != haystack.end();
    }

    /**
     * @brief Fast line-by-line iterator for string_view.
     */
    class LineIterator {
    public:
        LineIterator(std::string_view data) : data_(data), pos_(0) {}

        bool next(std::string_view& line) {
            if (pos_ >= data_.size()) return false;
            
            size_t next_pos = data_.find('\n', pos_);
            if (next_pos == std::string_view::npos) {
                line = data_.substr(pos_);
                pos_ = data_.size();
            } else {
                size_t len = next_pos - pos_;
                // Handle \r\n
                if (len > 0 && data_[next_pos - 1] == '\r') {
                    line = data_.substr(pos_, len - 1);
                } else {
                    line = data_.substr(pos_, len);
                }
                pos_ = next_pos + 1;
            }
            return true;
        }

    private:
        std::string_view data_;
        size_t pos_;
    };

    /**
     * @brief Convert an integer to a hex string.
     */
    inline std::string to_hex(uint32_t val) {
        char buf[12];
        std::snprintf(buf, sizeof(buf), "0x%08x", val);
        return std::string(buf);
    }

} // namespace utils
