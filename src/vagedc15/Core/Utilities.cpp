#include "Utilities.h"
#include <vector>
#include <string_view>
#include <cctype>
#include <string>
#include <format>
#include <sstream>

namespace Utilities
{
    std::filesystem::path& addToPath(std::filesystem::path& path, std::string_view content, char separator)
    {
        auto name = path.stem();
        name += separator;
        name += content;
        name += path.extension();
        path.replace_filename(name);
        return path;
    }
    std::string normalize(std::string_view s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '_' || c == ' ') continue;
            out.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(c))));
        }
        return out;
    }

    std::string toLower(std::string_view s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            out.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(c))));
        }
        return out;
    }

    std::vector<std::string> split(std::string_view line, char sep)
    {
        std::vector<std::string> out;
        size_t start = 0, pos;
        while ((pos = line.find(sep, start)) != std::string_view::npos) {
            out.emplace_back(line.substr(start, pos - start));
            start = pos + 1;
        }
        out.emplace_back(line.substr(start));
        return out;
    }

    std::string_view trim(std::string_view s)
    {
        constexpr std::string_view ws = " \t\r\n";
        const size_t first = s.find_first_not_of(ws);
        if (first == std::string_view::npos) return {};
        const size_t last = s.find_last_not_of(ws);
        return s.substr(first, last - first + 1);
    }


}