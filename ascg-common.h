#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <ranges>
#include <set>
#include <map>
#include <utility>
#include <limits>
#include <variant>
#include <optional>
#include <unordered_set>
#include <initializer_list>
#include <concepts>


/*
*	Integrate proper metric for grid complexity? Or have it just use a 'process' method so the user can decide when to stop?
*
*	Add comments on LayerBuilder and GraphBuilder how they work
*
*	add self-loops?/double-edges?/directions?
*
*	add bounds (like: max node-shifting iterations/max...) to conditionally ensure algorithm limits
*/
#include <ustring/ustring.h>

namespace ascg {
	/* callable queried between the optimization iterations, returning true
	*	stops the execution and the best result found so far is returned */
	template <class Type>
	concept IterationStop = std::predicate<Type>;
}

namespace ascg::detail {
	struct Layer;
	class Renderer;

	template <class RendererType = detail::Renderer>
	inline void PrintGrid(auto& grid, std::initializer_list<std::pair<size_t, size_t>> highlight = {}) {
		str::PrintLn();
		std::vector<detail::Layer> layers(grid.size());
		std::vector<std::string> lines = RendererType{ layers, grid }.draw(0).first;

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

	/* non-standard compliant version of fnv-1a, which does not just hash over single bytes */
	struct Hasher {
	private:
		size_t pHash = 0xcbf29ce484222325;

	public:
		void add(size_t value) {
			pHash = (pHash ^ value) * 0x00000100000001b3;
		}
		size_t digest() const {
			return pHash;
		}
	};
}
