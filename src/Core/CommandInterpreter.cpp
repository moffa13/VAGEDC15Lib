#include "CommandInterpreter.h"
#include "Core/Utilities.h"

#include <string_view>
#include <string>
#include <algorithm>
#include <optional>
#include <format>
#include <utility>
#include <stdexcept>

CommandInterpreter::CommandInterpreter(int argc, const char* const* argv) : _args(argv + 1, argv + argc)
{


}

void CommandInterpreter::addNeedsValue(std::string_view name)
{
    if (_parsed) {
        _parsed = false;
        _optArgs.clear();
        _noOptArgs.clear();
    }
    _needsValue.insert(standardizeRemoveArg(name));
}

bool CommandInterpreter::isOption(std::string_view name)
{
    return name.starts_with("--");
}

std::string_view CommandInterpreter::standardizeRemoveArg(std::string_view name)
{
    return isOption(name) ? name.substr(2) : name;
}

std::string CommandInterpreter::standardizeAddArg(std::string_view name)
{
    return isOption(name) ? std::string{ name } : "--" + std::string{ name };
}

bool CommandInterpreter::optionNeedsValue(std::string_view name) const
{
    return _needsValue.contains(standardizeRemoveArg(name));
}


bool CommandInterpreter::hasOption(std::string_view s) const
{
    return std::find(_args.begin(), _args.end(), standardizeAddArg(s)) != _args.end();
}

bool CommandInterpreter::optionHasValue(std::string_view s) const
{
    auto it = getOption(s);
    return it ? true : false;
}

size_t CommandInterpreter::countFreeArgs() const
{
    makeArgs();
    return _noOptArgs.size();
}

size_t CommandInterpreter::countOptArgs() const
{
    makeArgs();
    return _optArgs.size();
}

bool CommandInterpreter::hasArgs() const
{
    // Return size-1 because command is not an arg
    // And command is not an arg
    // s can be negative if no command
    int s = static_cast<int>(_args.size()) - 1;
    return s > 0;
}

const std::vector<std::string_view>& CommandInterpreter::freeArgs() const
{
    makeArgs();
    return _noOptArgs;
}

const std::vector<std::pair<std::string_view, std::optional<std::string_view>>>& CommandInterpreter::optArgs() const
{
    makeArgs();
    return _optArgs;
}

std::string_view CommandInterpreter::freeArg(size_t i) const
{
    return freeArgs()[i];
}

void CommandInterpreter::makeArgs() const
{
    if (_parsed) return;

    size_t i{ 0 };
    while (i < _args.size()) {
        if (isOption(_args[i])) {
            const bool hasValue = optionNeedsValue(_args[i]) && i + 1 < _args.size() && !isOption(_args[i + 1]);

            _optArgs.emplace_back(_args[i], hasValue ? std::optional<std::string_view>{ _args[i + 1] } : std::optional<std::string_view>{});
            i += hasValue ? 2 : 1;
        }
        else if (i > 0) {        
            _noOptArgs.push_back(_args[i]);
            i++;
        }
        else {
            i++; 
        }
    }

    _parsed = true;
}



bool CommandInterpreter::isCommand(std::string_view name) const
{
    auto cmd = getCommand();
    return cmd ? *cmd == name : false;
}

std::optional<std::string_view> CommandInterpreter::getCommand() const
{
    if (_args.size() == 0) return std::nullopt;
    return _args[0];
}

/// <summary>
/// Returns the value associated with an option.
/// </summary>
/// <param name="name">
/// Option name, with or without leading dashes.
/// Must have been declared via addNeedsValue(), otherwise the call throws.
/// </param>
/// <returns>
/// The option's value, or std::nullopt if the option was not passed
/// or was passed without a value. Use hasOption() / optionHasValue()
/// to tell those two cases apart.
/// The returned view points into argv and stays valid for the
/// lifetime of this CommandInterpreter.
/// </returns>
/// <exception cref="std::runtime_error">
/// Thrown if the option was not declared via addNeedsValue().
/// This signals a programming error, not bad user input.
/// </exception>
std::optional<std::string_view> CommandInterpreter::getOption(std::string_view name) const {

    makeArgs();
    
    if(!optionNeedsValue(name))
        throw std::runtime_error{"Trying to access a value of an option that does not need one"};
    
    auto standardizedName = standardizeAddArg(name);
    auto it = std::find_if(_optArgs.begin(), _optArgs.end(), [&](const auto& o) {
        return o.first == standardizedName;
    });

    if (it == _optArgs.end())
        return std::nullopt;
    return it->second;
}

std::optional<size_t> CommandInterpreter::getOptionHex(std::string_view name) const {

    auto opt = getOption(name);
    if (!opt) return std::nullopt;

    size_t n;
    auto hex = *opt;
    auto begin = hex.data();
    auto end = hex.data() + hex.size();
    if (hex.starts_with("0x")) {
        begin += 2;
    }
    else if (hex.ends_with("h")) {
        end -= 1;
    }
    auto [ptr, errc] = std::from_chars(begin, end, n, 16);
    if (errc != std::errc{} || ptr != end) {
        throw std::runtime_error{"Cannot parse hex string, should be in the format 0xAAAA or AAAA or AAAAh"};
    }
    return n;
}
