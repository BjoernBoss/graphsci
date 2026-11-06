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
	*	Describes a layered graph made up of: # | - , ' A V
	*	Edges can be directed or undirected but at most one edge per two nodes.
	*	The graph cannot contain self-loops.
	*	The layouting will select an ordering of the transitions and lay it out as ascii art.
	*	The ordering of the nodes from top to bottom will not be modified and will be used as provided.
	*/
	struct Graph {
		/* separate rows of the ascii graph */
		std::vector<std::string> rows;

		/* list of mappings from node index to row (sorted) */
		std::vector<std::pair<size_t, size_t>> map;
	};
}
