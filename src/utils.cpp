#include "utils.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace Utils {

std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start)) ++start;
    auto end = s.end();
    while (end != start && std::isspace(*(end - 1))) --end;
    return std::string(start, end);
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::istringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, delim))
        if (!tok.empty()) result.push_back(tok);
    return result;
}

bool icontains(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(),   needle.end(),
        [](char a, char b){ return std::tolower(a) == std::tolower(b); }
    );
    return it != haystack.end();
}

std::string human_size(long kb) {
    std::ostringstream ss;
    ss << std::fixed;
    if (kb < 1024) {
        ss.precision(0);
        ss << kb << "K";
    } else if (kb < 1024 * 1024) {
        ss.precision(1);
        ss << static_cast<float>(kb) / 1024.0f << "M";
    } else {
        ss.precision(2);
        ss << static_cast<float>(kb) / (1024.0f * 1024.0f) << "G";
    }
    return ss.str();
}

} // namespace Utils
