#include "Logger.h"
#include <iostream>
#include <string_view>

using namespace std;

void Logger::print(std::string_view message)
{
	std::cout << message << '\n';
}


void Logger::breakLine(int n) {

	if (n <= 0)
		return;

	for (int i{ 0 }; i < n; ++i)
		cout << '\n';

}

std::string_view Logger::lvlToString(Level l)
{
	switch (l) {
		case Logger::Level::Debug:   return "debug";   break;
		case Logger::Level::Warning: return "warning"; break;
		case Logger::Level::Error:   return "error";   break;
		case Logger::Level::Trace:   return "trace";   break;
		case Logger::Level::Info:   return "info";   break;
        case Logger::Level::None:   return "none";   break;
	}
    throw std::invalid_argument{ "Invalid log level value" };
}

Logger::Level Logger::parseLogLevel(std::string_view s) {
	if (s == "debug")   return Logger::Level::Debug;
	if (s == "warning") return Logger::Level::Warning;
	if (s == "error")   return Logger::Level::Error;
	if (s == "trace")   return Logger::Level::Trace;
	if (s == "info")   return Logger::Level::Info;
	return Logger::Level::None;
}

void Logger::logLevel(Level l, std::string_view message)
{
	if (l < level) {
		return;
	}

	std::cout << prefix(l) << message << '\n';
}

void Logger::trace(std::string_view message) { logLevel(Level::Trace, message); }
void Logger::debug(std::string_view message) { logLevel(Level::Debug, message); }
void Logger::info(std::string_view message) { logLevel(Level::Info, message); }
void Logger::warning(std::string_view message) { logLevel(Level::Warning, message); }
void Logger::error(std::string_view message) { logLevel(Level::Error, message); }

void Logger::overlay(std::string_view message)
{
	static size_t lastLength = 0;

	std::cout << '\r' << message;
	if (message.size() < lastLength)
		std::cout << std::string(lastLength - message.size(), ' ')
		<< std::string(lastLength - message.size(), '\b');
	std::cout << std::flush;

	lastLength = message.size();
}
