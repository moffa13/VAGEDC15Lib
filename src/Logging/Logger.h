#pragma once
#include <cstdint>
#include <format>
#include <iostream>
#include <string_view>

class Logger
{
public:

	template<typename ...Args>
	static void print(std::format_string<Args...> format, Args&&... args);

	enum class Level { Trace, Debug, Info, Warning, Error, None };

	static std::string_view lvlToString(Level l);
	static Level parseLogLevel(std::string_view s);

	static inline Level level = Level::Debug;

	template<typename... Args>
	static void trace(std::format_string<Args...> format, Args&&... args);

	template<typename... Args>
	static void debug(std::format_string<Args...> format, Args&&... args);

	template<typename... Args>
	static void info(std::format_string<Args...> format, Args&&... args);

	template<typename... Args>
	static void warning(std::format_string<Args...> format, Args&&... args);

	template<typename... Args>
	static void error(std::format_string<Args...> format, Args&&... args);

	template<typename... Args>
	static void logLevel(Level l, std::format_string<Args...> format, Args&&... args);

	static void logLevel(Level l, std::string_view message);

	static void breakLine(int n);

	static void print(std::string_view message);

	static void trace(std::string_view message);
	static void debug(std::string_view message);
	static void info(std::string_view message);
	static void warning(std::string_view message);
	static void error(std::string_view message);

	static void overlay(std::string_view message);

private:

	static constexpr std::string_view prefix(Level l)
	{
		switch (l) {
			case Level::Trace: return "[TRACE] ";
			case Level::Debug: return "[DEBUG] ";
			case Level::Info:  return "[INFO] ";
			case Level::Warning:  return "[WARN] ";
			case Level::Error: return "[ERR] ";
			case Level::None: throw std::invalid_argument{ "Cannot use none Level for logging" };
		}
		throw std::invalid_argument{ "Invalid log level value" };
	}

};

template<>
struct std::formatter<Logger::Level> : std::formatter<std::string_view> {
	auto format(Logger::Level lvl, auto& ctx) const {
		return std::formatter<std::string_view>::format(Logger::lvlToString(lvl), ctx);
	}
};

template<typename ...Args>
inline void Logger::print(std::format_string<Args...> format, Args&&... args) {
	std::cout << std::format(format, std::forward<Args>(args)...) << '\n';
}


template<typename ...Args>
inline void Logger::logLevel(Level l, std::format_string<Args...> format, Args&&... args)
{
	if (l < level) {
		return;
	}

	std::cout << prefix(l)
		<< std::format(format, std::forward<Args>(args)...)
		<< '\n';
}

template<typename ...Args>
inline void Logger::trace(std::format_string<Args...> format, Args&&... args)
{
	logLevel(Level::Trace, format, std::forward<Args>(args)...);
}

template<typename ...Args>
inline void Logger::debug(std::format_string<Args...> format, Args&&... args)
{
	logLevel(Level::Debug, format, std::forward<Args>(args)...);
}

template<typename ...Args>
inline void Logger::info(std::format_string<Args...> format, Args&&... args)
{
	logLevel(Level::Info, format, std::forward<Args>(args)...);
}

template<typename ...Args>
inline void Logger::warning(std::format_string<Args...> format, Args&&... args)
{
	logLevel(Level::Warning, format, std::forward<Args>(args)...);
}

template<typename ...Args>
inline void Logger::error(std::format_string<Args...> format, Args&&... args)
{
	logLevel(Level::Error, format, std::forward<Args>(args)...);
}