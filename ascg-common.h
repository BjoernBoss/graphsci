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
*	finish layer-builder (cleanup)
*
*	only shift nodes with highest score
*	update detail::LayoutGraph to default offset preferred positions to be centered
*
*	shrink horizontally
*	shrink vertically
*	straighten up any lines
*
*	grid creation: dont account for number of crossings anymore for minimum size?
*/
#include <ustring/ustring.h>

namespace ascg::detail {
	struct Layer;
	class Renderer;

	inline void PrintGrid(auto& grid) {
		str::PrintLn();
		std::vector<detail::Layer> layers(grid.size());
		for (const std::string& line : detail::Renderer{ layers, grid }.draw(0).first)
			str::PrintLn('[', line, ']');
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

	public:
		size_t first = 0;
		size_t last = 0;

	public:
		MinMaxWalk(size_t a, size_t b, bool exclusive) : first{ std::min(a, b) }, last{ std::max(a, b) } {
			if (exclusive) {
				if (++first < last)
					--last;
				else
					++first;
			}
		}
		Iterator begin() const { return Iterator{ first }; };
		Iterator end() const { return Iterator{ last + 1 }; };
	};
	inline size_t Towards(size_t value, size_t other, size_t step = 1) {
		return (value < other ? value + step : value - step);
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
