#pragma once

#include <string>
#include <vector>
#include <cinttypes>
#include <algorithm>
#include <ranges>
#include <set>
#include <map>
#include <utility>
#include <limits>
#include <list>
#include <variant>
#include <optional>


/*
*	cleanup in includes
*	add self-loops?/double-edges?/directions?
*
*	finish layer-builder (cleanup)
*	update detail::LayoutGraph to default offset preferred positions to be centered
*
*	add bounds (like: max node-shifting iterations/max...) to conditionally ensure algorithm limits*
*/
#include <ustring/ustring.h>

namespace ascg::detail {
	struct Layer;
	class Renderer;

	inline void PrintGrid(auto& grid, std::initializer_list<std::pair<size_t, size_t>> highlight = {}) {
		str::PrintLn();
		std::vector<detail::Layer> layers(grid.size());
		std::vector<std::string> lines = detail::Renderer{ layers, grid }.draw(0).first;

		size_t index = 0;
		for (const auto& [x, y] : highlight)
			lines[y][x * 2] = "0123456789ABCDEF"[index++];

		for (const std::string& line : lines)
			str::PrintLn('[', line, ']');
		return;
	}
}



namespace ascg::detail {
	struct MinMaxWalk {
	public:
		struct Iterator {
			size_t index = 0;
			size_t operator*() const { return index; }
			Iterator& operator++() { ++index; return *this; }
			bool operator==(Iterator other) const { return index == other.index; }
			bool operator!=(Iterator other) const { return !(*this == other); }
		};

	private:
		size_t pBegin = 0;
		size_t pEnd = 0;

	public:
		MinMaxWalk(size_t a, size_t b, bool exclusive) : pBegin{ std::min(a, b) }, pEnd{ std::max(a, b) + 1 } {
			if (exclusive && ++pBegin < pEnd)
				--pEnd;
		}
		Iterator begin() const { return Iterator{ pBegin }; };
		Iterator end() const { return Iterator{ pEnd }; };
	};
	inline size_t Towards(size_t value, size_t other, size_t step = 1) {
		if (value == other)
			return value;
		return (value < other ? value + step : value - step);
	}
	inline size_t AwayFrom(size_t value, size_t other, size_t step = 1) {
		if (value == other)
			return value;
		return (value < other ? value - step : value + step);
	}
	inline bool Inbetween(size_t value, size_t a, size_t b) {
		if (a > b) std::swap(a, b);
		return (value >= a && value <= b);
	}

	struct Range {
		size_t first = 0;
		size_t last = 0;
	};
}
