#pragma once

#include <vector>
#include <filesystem>
#include <string_view>
#include <span>
#include <optional>

enum class SearchDirection { Forward, Backward };

struct PatternByte {
	uint8_t value{};
	bool wildcard{};
};

class EDC15File
{
public:
	EDC15File(std::filesystem::path const& path);
	static std::vector<uint8_t> readFile(const std::filesystem::path& path);
	static constexpr size_t flashToECUOffset = 0x80000;
	static constexpr size_t flashSize = 0x80000;
	uint16_t readWordAligned(size_t pos) const;
	uint16_t readWord(size_t pos) const;
	void writeLittleEndian(size_t pos, uint16_t data);
	void writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& content);
	void writeFile(const std::filesystem::path& path);
	std::optional<size_t> findPattern(const std::vector<PatternByte>& pat, size_t from, std::optional<size_t> max = std::nullopt, SearchDirection dir = SearchDirection::Forward) const;
	std::optional<size_t> findPattern(std::string_view pattern, size_t from, std::optional<size_t> max = std::nullopt, SearchDirection dir = SearchDirection::Forward) const;
	std::vector<size_t> findPatterns(std::string_view pattern, size_t from, std::optional<size_t> max = std::nullopt, SearchDirection dir = SearchDirection::Forward) const;
	std::vector<size_t> findPatterns(const std::vector<PatternByte>& pat, size_t from, std::optional<size_t> max = std::nullopt, SearchDirection dir = SearchDirection::Forward) const;
	static std::vector<PatternByte> parsePattern(std::string_view pattern);
	inline size_t getRealFlashSize() const { return _flash.size(); };
	inline size_t getRealFlashSizeAsWord() const { return _sizeAsWord; };
	static std::vector<PatternByte> containerToPatternByte(std::span<uint8_t> span);
	std::vector<uint8_t>& data() { return _flash; }
	const std::vector<uint8_t>& data() const { return _flash; }
	std::vector<uint8_t> _flash;
protected:
	bool isEmptyBlock(size_t addr, size_t len, uint8_t emptyByte);
	const std::size_t _sizeAsWord;
private:
	std::span<const uint8_t> getBlock(size_t addr, size_t len);
};

