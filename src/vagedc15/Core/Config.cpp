#include "Config.h"
#include "vagedc15/Core/Utilities.h"

#include <fstream>
#include <format>

Config::Config() {
}

Config::Config(std::filesystem::path path, bool saveIfNoFile) : _path(std::move(path))
{

	if (!std::filesystem::is_regular_file(_path)) {
		if(saveIfNoFile)
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
		std::string line{ std::format("{}={}\n", key, val) };
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

		auto equalPos = line.find_first_of('=');
		
		if (equalPos == 0 || equalPos == std::string::npos || line.size() == equalPos + 1 || line.find_first_of('=', equalPos + 1) != std::string::npos) {
			throw std::runtime_error{std::format("Line {} is badly formatted", lineNo)};
		}
		std::string key{ Utilities::toLower(Utilities::trim(line.substr(0, equalPos))) };
		std::string val{ Utilities::trim(line.substr(equalPos + 1, line.size())) };

		auto [_, inserted] = _parameters.try_emplace(std::string{ key }, std::string{ val });
		if (!inserted) {
			throw std::runtime_error{ std::format("Multiple declaration of key {}, last occurence at line {}", key, lineNo) };
		}

		
	}
	
}
