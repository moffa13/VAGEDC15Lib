#pragma once

#include <vector>
#include <string_view>
#include <string>
#include <optional>
#include <unordered_set>
#include <utility>

class CommandInterpreter {

public:
	CommandInterpreter(int argc, const char* const* argv);
	const std::vector<std::string_view>& freeArgs() const;
	const std::vector<std::pair<std::string_view, std::optional<std::string_view>>>& optArgs() const;
	std::string_view freeArg(size_t i) const;
	bool hasOption(std::string_view s) const;
	bool isCommand(std::string_view name) const;
	bool optionNeedsValue(std::string_view name) const;
	bool optionHasValue(std::string_view s) const;
	void addNeedsValue(std::string_view name);
	size_t countFreeArgs() const;
	size_t countOptArgs() const;
	bool hasArgs() const;
	std::optional<std::string_view> getCommand() const;
	std::optional<std::string_view>  getOption(std::string_view name) const;
	std::optional<size_t> getOptionHex(std::string_view name) const;
private:
	void makeArgs() const;
	static std::string_view standardizeRemoveArg(std::string_view name);
	static std::string standardizeAddArg(std::string_view name);
	static bool isOption(std::string_view);
	std::unordered_set<std::string_view> _needsValue;
	std::vector<std::string_view> _args;
	mutable std::vector<std::string_view> _noOptArgs;
	mutable bool _parsed{ false };
	mutable std::vector<std::pair<std::string_view, std::optional<std::string_view>>> _optArgs;
};