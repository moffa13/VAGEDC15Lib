#pragma once

#include <vector>
#include <string_view>
#include <string>
#include <optional>
#include <unordered_set>
#include <utility>
#include <unordered_map>

class CommandInterpreter {


public:
	/// <summary>
	/// What a command accepts on the command line. Must stay in sync with printUsage().
	/// Anything not listed here is rejected, so typos in option names don't go unnoticed.
	/// </summary>
	struct CommandSpec {
		std::string_view name;
		std::vector<std::string_view> flags{};      // options without value  (--save)
		std::vector<std::string_view> valued{};     // options with a value   (--file x.csv)
		std::optional<size_t> minFreeArgs{ 0 };
		std::optional<size_t> maxFreeArgs{ 0 };
		std::string_view freeArgsError{};
		std::unordered_map<std::string, std::string> additionalParams;
	};
	std::optional<std::string> validateCommandLine();
	void setCommandSpecs(std::vector<CommandSpec> const * specs);
	std::optional<std::string_view> firstUnregisteredOption() const;
	const CommandSpec* getCommandSpec() const;

	CommandInterpreter(int argc, const char* const* argv);
	const std::vector<std::string_view>& freeArgs() const;
	const std::vector<std::pair<std::string_view, std::optional<std::string_view>>>& optArgs() const;
	std::string_view freeArg(size_t i) const;
	bool hasOption(std::string_view s) const;
	bool isCommand(std::string_view name) const;
	bool optionNeedsValue(std::string_view name) const;
	bool optionHasValue(std::string_view s) const;
	void addNeedsValue(std::string_view name);
	void addOption(std::string_view name);
	size_t countFreeArgs() const;
	size_t countOptArgs() const;
	bool hasArgs() const;
	bool validateArgs() const;
	bool validateFreeArgs() const;
	bool validate() const;
	void setFreeArgSize(std::optional<size_t> min, std::optional<size_t> max);
	std::optional<std::string_view> getCommand() const;
	std::optional<std::string_view>  getOption(std::string_view name) const;
	std::optional<size_t> getOptionHex(std::string_view name) const;

private:
	void makeArgs() const;
	static std::string_view standardizeRemoveArg(std::string_view name);
	static std::string standardizeAddArg(std::string_view name);
	static bool isOption(std::string_view);
	std::unordered_set<std::string> _needsValue;
	std::unordered_set<std::string> _doesntNeedValue;
	std::vector<std::string_view> _args;
	mutable std::vector<std::string_view> _noOptArgs;
	mutable std::vector<std::pair<std::string_view, std::optional<std::string_view>>> _optArgs;
	std::optional<size_t> _minFreeArgs = std::nullopt;
	std::optional<size_t> _maxFreeArgs = std::nullopt;
	mutable bool _parsed{ false };

	std::vector<CommandInterpreter::CommandSpec> const * _commandSpecs = nullptr;
};