#pragma once
#include <string>
#include <vector>

namespace Utils {
    // Trim leading/trailing whitespace
    std::string trim(const std::string& s);

    // Split string by delimiter
    std::vector<std::string> split(const std::string& s, char delim);

    // Case-insensitive substring search
    bool icontains(const std::string& haystack, const std::string& needle);

    // Format bytes → human readable (KB, MB, GB)
    std::string human_size(long kb);

    // Clamp value to [lo, hi]
    template<typename T>
    T clamp(T val, T lo, T hi) {
        return val < lo ? lo : (val > hi ? hi : val);
    }
}
