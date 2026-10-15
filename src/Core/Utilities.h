#pragma once
#include <string_view>
#include <string>
#include <vector>
#include <format>

namespace Utilities
{

	std::string normalize(std::string_view s);
	std::string toLower(std::string_view s);
	std::vector<std::string> split(std::string_view line, char sep);
	std::string_view trim(std::string_view s);
	template<typename Container>
	std::string alignRow(const Container& cells, const std::vector<size_t>& widths);

	template<typename Container, typename T>
	inline bool contains(const Container& vec, const T& elem)
	{

		return std::find(vec.begin(), vec.end(), elem) != vec.end();
	}

	template<typename ContainerA, typename ContainerB>
	inline bool containsOneOf(const ContainerA& vec, const ContainerB& elems)
	{
		for (const auto& elem : elems) {
			if (contains(vec, elem)) {
				return true;
			}
		}
		return false;
	}

	template<typename Container>
	inline std::string toString(Container c, std::string_view sep = ", ") {
		std::string result;
		for (const auto& item : c) {
			result += std::format("{}{}", item, sep);
		}
		if(c.size() > 0)
			result.resize(result.size() - sep.size());
		return result;
	}


	template<typename Container>
	std::string alignRow(const Container& cells, const std::vector<size_t>& widths) {
		std::string res;
		for (size_t i = 0; i < cells.size(); ++i) {
			if (i) res += "  ";
			res += std::format("{:>{}}", cells[i], widths[i]);
		}
		return res;
	}
	
	
}