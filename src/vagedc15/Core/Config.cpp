#include "Config.h"
#include "vagedc15/Core/Utilities.h"

#include <fstream>
#include <format>
#include <string_view>

namespace {

	// Backslashes and '=' get a backslash in front, so any value can be written to the file
	std::string escape(std::string_view s)
	{
		std::string out;
		out.reserve(s.size());
		for (char c : s) {
			if (c == '\\' || c == '=')
				out.push_back('\\');
			out.push_back(c);
		}
		return out;
	}

	// Only \\ and \= are escape sequences, any other backslash is kept as is,
	// so hand-written Windows paths like C:\Users\me still read correctly
	std::string unescape(std::string_view s)
	{
		std::string out;
		out.reserve(s.size());
		for (size_t i = 0; i < s.size(); ++i) {
			if (s[i] == '\\' && i + 1 < s.size() && (s[i + 1] == '\\' || s[i + 1] == '=')) {
				++i;
			}
			out.push_back(s[i]);
		}
		return out;
	}

	// Position of the first '=' that is not escaped, npos if none
	size_t findSeparator(std::string_view s, size_t from = 0)
	{
		for (size_t i = from; i < s.size(); ++i) {
			if (s[i] == '\\')
				++i;    // skip the escaped char
			else if (s[i] == '=')
				return i;
		}
		return std::string_view::npos;
	}

}

Config::Config() {}

Config::Config(std::filesystem::path path, bool saveIfNoFile) : _path(std::move(path))
{

	if (!std::filesystem::is_regular_file(_path)) {
		if (saveIfNoFile)
			saveConfig();
	}
	else {
		readConfig();
	}


}

void Config::saveConfig()
{

	std::ofstream file{ _path, std::ios::out | std::ios::trunc };

	for (auto [key, val] : _parameters) {
		std::string line{ std::format("{}={}\n", escape(key), escape(val)) };
		file.write(line.data(), line.size());
	}

}

void Config::readConfig()
{

	std::ifstream file{ _path, std::ios::in };

	if (!file) {
		throw std::runtime_error{ std::format("Error reading config file {}", _path.string()) };
	}

	std::string line;
	size_t lineNo{ 0 };
	while (std::getline(file, line)) {
		++lineNo;
		line = Utilities::trim(line);

		if ((line.size() >= 1 && line[0] == '#') || line.empty()) {
			continue;
		}

		auto equalPos = findSeparator(line);

		const bool badFormat = equalPos == 0
			|| equalPos == std::string_view::npos
			|| findSeparator(line, equalPos + 1) != std::string_view::npos;

		if (badFormat) {
			throw std::runtime_error{ std::format("Line {} is badly formatted", lineNo) };
		}

		std::string key{ Utilities::toLower(
			unescape(Utilities::trim(line.substr(0, equalPos)))
		) };
		std::string val{ unescape(Utilities::trim(line.substr(equalPos + 1))) };

		auto [_, inserted] = _parameters.try_emplace(std::string{ key }, std::string{ val });
		if (!inserted) {
			throw std::runtime_error{ std::format("Multiple declaration of key {}, last occurence at line {}", key, lineNo) };
		}


	}

}