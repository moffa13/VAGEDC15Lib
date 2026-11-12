#pragma once
#include "vagedc15/Core/Utilities.h"
#include "vagedc15/Logging/Logger.h"
#include <charconv>
#include <filesystem>
#include <unordered_map>
#include <format>


class Config {

public:
		Config();
		Config(std::filesystem::path path, bool saveIfNoFile = false);
		template<typename T>
		T readKey(const std::string& name, T defaultV);
		template<typename T>
		T readKey(const std::string& name) const;
		template<typename T>
		std::string serialize(const T& value);
		template<typename T>
		void saveKey(const std::string& name, T value);
private:
	std::filesystem::path _path;
	std::unordered_map<std::string, std::string> _parameters;
	void readConfig();
	void saveConfig();
};

template<typename T>
inline void Config::saveKey(const std::string& name, T value)
{
	std::string serialized{ serialize(value) };
	_parameters[Utilities::toLower(name)] = serialized;
	saveConfig();
}

template<typename T>
inline T Config::readKey(const std::string& name, T defaultV)
{
	std::string serialized{ serialize(defaultV) };

	auto [_, inserted] = _parameters.try_emplace(Utilities::toLower(name), std::move(serialized));
	if (inserted) {
		saveConfig();
		return defaultV;
	}
	return readKey<T>(Utilities::toLower(name));
}

template<typename T>
inline std::string Config::serialize(const T& value) {
	std::string serialized;
	if constexpr (std::is_same_v<T, std::string>) {
		serialized = value;
	}
	else if constexpr (std::is_same_v<T, char>) {
		serialized = std::string(1, value);
	}
	else if constexpr (std::is_same_v<T, bool>) {
		serialized = value ? "true" : "false";
	}
	else if constexpr (std::is_same_v<T, Logger::Level>) {
		serialized = Logger::lvlToString(value);
	}
	else {
		serialized = std::format("{}", value);
	}
	return serialized;
}

template<typename T>
inline T Config::readKey(const std::string& name) const
{

	auto lname = Utilities::toLower(name);

	auto it = _parameters.find(lname);

	if (it == _parameters.end()) {
		throw std::runtime_error{std::format("No key named {} in config", lname)};
	}

	const std::string& val = it->second;

	if constexpr (std::is_same_v<T, std::string>) {
		return val;
	}
	else if constexpr (std::is_same_v<T, bool>) {
		return val == "1" || val == "true" || val == "yes";
	}
	else if constexpr (std::is_same_v<T, Logger::Level>) {
		return Logger::parseLogLevel(val);
	}
	else if constexpr (std::is_same_v<T, char>) {
		if (val.size() != 1) {
			throw std::runtime_error{
				std::format("Invalid value for {} -> {} (expected a single character)", lname, val) };
		}
		return val[0];
	}
	else {
		T value{};
		auto [ptr, err] = std::from_chars(val.data(), val.data() + val.size(), value);
		if (err != std::errc{} || ptr != val.data() + val.size()) {
			throw std::runtime_error{std::format("Invalid value for {} -> {}", lname, val)};
		}
		return value;
	}
}
