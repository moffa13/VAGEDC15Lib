#include "EDC15File.h"
#include "vagedc15/Logging/Logger.h"
#include <fstream>
#include <cstdint>
#include <vector>
#include <optional>
#include <string_view>
#include <span>
#include <format>
#include <regex>

EDC15File::EDC15File(std::filesystem::path const& path) : _flash(readFile(path)), _sizeAsWord(_flash.size() / 2)
{
    if (_flash.size() != EDC15File::flashSize)
        throw std::runtime_error{"Flash size for an EDC15 is wrong, error was thrown as a security measure."};
    
    Logger::debug("Opening file with software {}", getFirmwareInfos());
}

std::vector<PatternByte> EDC15File::parsePattern(std::string_view pattern)
{
    std::vector<PatternByte> out;

    for (size_t i = 0; i < pattern.size(); ) {
        if (pattern[i] == ' ') { ++i; continue; }

        if (i + 1 >= pattern.size()) {
            throw std::invalid_argument{ "Pattern has an odd number of hex digits" };
        }

        const std::string_view pair = pattern.substr(i, 2);

        if (pair == "XX" || pair == "xx" || pair == "??") {
            out.push_back({ 0, true });
        }
        else {
            uint8_t value{};
            const auto [ptr, err] = std::from_chars(
                pair.data(), pair.data() + 2, value, 16);
            if (err != std::errc{} || ptr != pair.data() + 2) {
                throw std::invalid_argument{ std::format("Invalid hex pair '{}'", pair) };
            }
            out.push_back({ value, false });
        }
        i += 2;
    }

    return out;
}

std::vector<PatternByte> EDC15File::containerToPatternByte(std::span<uint8_t> span)
{
    std::vector<PatternByte> bytes;
    for (auto e : span)
    {
        bytes.emplace_back(e, false);
    }
    return bytes;
}

std::vector<size_t> EDC15File::findPatterns(
    std::string_view pattern, size_t from, std::optional<size_t> to, SearchDirection dir
) const
{
    auto pat = parsePattern(pattern);
    return findPatterns(pat, from, to, dir);
}

std::vector<size_t> EDC15File::findPatterns(
    const std::vector<PatternByte>& pat, size_t from, std::optional<size_t> to, SearchDirection dir
) const
{
    std::vector<size_t> positions;
    size_t startOffset{ from };

    while (startOffset < _flash.size()) {
        auto pos = findPattern(pat, startOffset, to, dir);
        if (pos) {
            positions.push_back(*pos);
            if (dir == SearchDirection::Forward) {
                startOffset = (*pos) + 1;
            }
            else {
                startOffset = (*pos);
            }
        }
        else {
            break;
        }
    }

    return positions;
}

std::optional<size_t> EDC15File::findPattern(
    std::string_view pattern, size_t from, std::optional<size_t> to, SearchDirection dir
) const
{
    auto pat = parsePattern(pattern);
    return findPattern(pat, from, to, dir);
}

/// <summary>
/// Find a pattern from a position in byte and to an optional position.
/// The resulting pattern positions are within the from<->to bounds.
/// </summary>
/// <param name="pattern"></param>
/// <param name="from"></param>
/// <param name="to"></param>
/// <param name="dir"></param>
/// <returns></returns>
std::optional<size_t> EDC15File::findPattern(
    const std::vector<PatternByte>& pat, size_t from, std::optional<size_t> to, SearchDirection dir
) const
{
    const size_t n = _flash.size();


    if (pat.empty()) {
        throw std::invalid_argument{ "Pattern is empty" };
    }
    else if (pat.size() > n) {
        throw std::invalid_argument{ "Pattern is greater than data" };
    }
    else if (from >= n) {
        throw std::invalid_argument{ "<From> has invalid value" };
    }
    else if (to && to >= n) {
        throw std::invalid_argument{ "<To> has invalid value" };
    }

    

    const auto match = [&](size_t pos) {
        for (size_t k = 0; k < pat.size(); ++k)
            if (!pat[k].wildcard && _flash[pos + k] != pat[k].value)
                return false;
        return true;
    };

    if (dir == SearchDirection::Forward) {
        const size_t last = n - pat.size();
        if (from > last) return std::nullopt;
        const size_t stop = std::min(to.value_or(last), last);
        if (from > stop)
            throw std::invalid_argument{ "findPattern: from > bound (forward)" };
        for (size_t pos = from; pos <= stop; ++pos)
            if (match(pos)) return pos;
    }
    else {
        if (from < pat.size()) return std::nullopt;
        const size_t start = from - pat.size();
        const size_t stop = to.value_or(0);
        if (stop > start)
            throw std::invalid_argument{ "findPattern: bound > from (backward)" };
        for (size_t pos = start + 1; pos-- > stop; )
            if (match(pos)) return pos;
    }

    return std::nullopt;
}

std::string_view EDC15File::getFirmwareInfos() const
{
    auto edcPatternPos = findPattern("45 44 43 20 20", 0x40000); // "EDC__"
    if (!edcPatternPos) {
        return {};
    }

    auto posBase = _flash.data() + *edcPatternPos;
    auto flashBegin = _flash.data();
    auto flashEnd = _flash.data() + _flash.size();

    auto first = reinterpret_cast<const char*>(std::find(std::reverse_iterator{posBase}, std::reverse_iterator{ flashBegin }, 0).base());
    auto last = reinterpret_cast<const char*>(std::find(posBase, flashEnd, 0));

    // the first match will give the offset of the first sequence not made of not allowed chars
    static const std::regex reg{"[0-9]{6,}"};

    std::cmatch m;

    if (!std::regex_search(first, last, m, reg))
        return {};

    return { first + m.position(), last};
}

std::string_view EDC15File::getVAGPartnumber() const
{
    return getVAGPartnumber(getFirmwareInfos());
}

std::string_view EDC15File::getVAGPartnumber(std::string_view text)
{
    static const std::regex reg{ "[0-9]{9}[A-Z]{1,2}" };
    std::cmatch m;

    if (!std::regex_search(text.data(), text.data() + text.size(), m, reg))
        return {};

    return text.substr(m.position(), m.length());
}

bool EDC15File::isEmptyBlock(size_t addr, size_t len, uint8_t emptyChar) {
    auto block = getBlock(addr, len);
    for (size_t i{ 0 }; i < len; ++i) {
        if (block[i] != emptyChar)
            return false;
    }
    return true;
}

std::span<const uint8_t> EDC15File::getBlock(size_t addr, size_t len) {
    return std::span{ _flash }.subspan(addr, len);
}


std::vector<uint8_t> EDC15File::readFile(const std::filesystem::path& path)
{
    std::ifstream file{ path, std::ios::binary | std::ios::ate };
    if (!file) {
        throw std::runtime_error{ std::format("Cannot open {}", path.string()) };
    }

    const auto size = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);

    return data;
}

void EDC15File::writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& content)
{
    std::ofstream file{ path, std::ios::binary | std::ios::out | std::ios::trunc };
    if (!file) {
        throw std::runtime_error{ std::format("Cannot open {} for writing", path.string()) };
    }

    file.write(reinterpret_cast<const char*>(content.data()), content.size());
}

void EDC15File::writeFile(const std::filesystem::path& path)
{
    writeFile(path, _flash);
}

uint16_t EDC15File::readWordAligned(size_t pos) const {

    if (pos >= _flash.size() / 2) {
        throw std::invalid_argument{std::format("Trying to access out of range address 0x{:05X} from file ", pos)};
    }

    size_t posBase = pos * 2;

    return (static_cast<uint16_t>(_flash[posBase + 1]) << 8) | _flash[posBase];
  
}

uint16_t EDC15File::readWord(size_t pos) const {

    if (pos + 2 > _flash.size()) {
        throw std::invalid_argument{ std::format("Trying to access out of range address 0x{:05X} from file ", pos) };
    }

    return (static_cast<uint16_t>(_flash[pos + 1]) << 8) | _flash[pos];

}

void EDC15File::writeLittleEndian(size_t pos, uint16_t data)
{
    if (pos + 2 > _flash.size()) {
        throw std::invalid_argument{
            std::format("Trying to write at out of range address 0x{:05X}", pos)
        };
    }

    _flash[pos] = static_cast<uint8_t>(data);
    _flash[pos + 1] = static_cast<uint8_t>(data >> 8);
}
 