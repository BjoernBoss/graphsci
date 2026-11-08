#pragma once

#include <string>
#include <string_view>
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
#include <unordered_map>
#include <initializer_list>
#include <concepts>

namespace ascg {
	namespace detail {
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
			bool headFirst = false;
			bool headLast = false;
		};
		using Ranges = std::vector<detail::Range>;

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

	/* callable queried between the optimization iterations, returning true
	*	stops the execution and the best result found so far is returned */
	template <class Type>
	concept IterationStop = std::predicate<Type>;

	/*
	*	Describes a layered graph made up of the character set defined by the theme.
	*	Edges can be directed or undirected but at most one edge per any two nodes.
	*	The layouting will select an ordering of the transitions and lay it out as ascii art.
	*	The ordering of the nodes from top to bottom will not be modified and will be used as provided.
	*/
	struct Graph {
		/* separate rows of the ascii graph */
		std::vector<std::string> rows;

		/* list of mappings from node index to row (sorted) */
		std::vector<std::pair<size_t, size_t>> map;
	};

	/* character theme to be used for the rendering */
	struct Theme {
		std::string_view horizontal;
		std::string_view vertical;
		std::string_view crossing;
		std::string_view leftTop;
		std::string_view leftBottom;
		std::string_view rightTop;
		std::string_view rightBottom;
		std::string_view node;
		std::string_view empty;
		std::string_view arrowTop;
		std::string_view arrowBottom;
		size_t spacing = 0;
	};

	/* default pure ascii character set */
	static constexpr ascg::Theme AsciiTheme = {
		.horizontal = "-",
		.vertical = "|",
		.crossing = "|",
		.leftTop = "'",
		.leftBottom = ",",
		.rightTop = "'",
		.rightBottom = ",",
		.node = "#",
		.empty = " ",
		.arrowTop = "A",
		.arrowBottom = "V",
		.spacing = 1
	};

	/* default extended ascii character set (codepage 437) */
	static constexpr ascg::Theme ExtendedTheme = {
		.horizontal = "\xc4",
		.vertical = "\xb3",
		.crossing = "\xb3",
		.leftTop = "\xd9",
		.leftBottom = "\xbf",
		.rightTop = "\xc0",
		.rightBottom = "\xda",
		.node = "\xfe",
		.empty = " ",
		.arrowTop = "A",
		.arrowBottom = "V",
		.spacing = 1
	};

	/* configuration arguments for the graphical theme */
	struct UnicodeThemeArgs {
		bool touchCrossing = false;
		bool heavyLines = false;
		bool heavyNodes = false;
		bool roundCorners = false;
		size_t spacing = 1;
	};

	/* graphical theme builder using unicode */
	inline ascg::Theme UnicodeTheme(ascg::UnicodeThemeArgs args = {}) {
		ascg::Theme theme;
		if (args.roundCorners)
			args.heavyLines = false;

		/* add the corner styles */
		if (args.roundCorners) {
			theme.leftTop = "\u256f";
			theme.leftBottom = "\u256e";
			theme.rightTop = "\u2570";
			theme.rightBottom = "\u256d";
		}
		else {
			theme.leftTop = (args.heavyLines ? "\u251b" : "\u2518");
			theme.leftBottom = (args.heavyLines ? "\u2513" : "\u2510");
			theme.rightTop = (args.heavyLines ? "\u2517" : "\u2514");
			theme.rightBottom = (args.heavyLines ? "\u250f" : "\u250c");
		}

		/* add the line styles */
		theme.horizontal = (args.heavyLines ? "\u2501" : "\u2500");
		theme.vertical = (args.heavyLines ? "\u2503" : "\u2502");

		/* add the crossing */
		if (args.touchCrossing)
			theme.crossing = (args.heavyLines ? "\u254b" : "\u253c");
		else
			theme.crossing = theme.vertical;

		/* add the remaining themes */
		theme.node = (args.heavyNodes ? "\u2588" : "\u25a0");
		theme.empty = " ";
		theme.arrowTop = "\u25b2";
		theme.arrowBottom = "\u25bc";
		theme.spacing = args.spacing;
		return theme;
	}
}
