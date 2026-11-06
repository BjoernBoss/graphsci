#pragma once

#include "ascg-common.h"
#include "ascg-render.h"

namespace ascg {
	/* invalid value for normal node indices; used to identify intermediate rows in the 'ascg::Graph' */
	static constexpr size_t NoNode = std::numeric_limits<size_t>::max();

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

		/* mapping of each row to the node it belongs to with 'ascg::NoNode' for intermediate rows */
		std::vector<size_t> map;
	};

	/* collect all edges of the graph and convert them to a final graph (ranges must not include
	*	ascg::NoNode as value and must not be identical, i.e. a connection to the same node) */
	class Builder {
	private:
		std::vector<detail::Range> pRanges;

	private:
		ascg::Graph fFinalize(ascg::IterationStop auto&& stopTest) const {
			detail::LayerState layers = detail::LayerBuilder{ pRanges }.build();
			detail::GridState grid = detail::GridBuilder{ layers }.build(stopTest);
			auto [rows, map] = detail::Renderer{ layers, grid }.draw(ascg::NoNode);
			return ascg::Graph{ .rows = std::move(rows), .map = std::move(map) };
		}

	public:
		void connect(size_t a, size_t b, bool aHead = false, bool bHead = false) {
			/* ensure that the nodes are well ordered and considered valid */
			if (a > b)
				std::swap(a, b), std::swap(aHead, bHead);
			if (a == b || b >= ascg::NoNode)
				return;
			pRanges.push_back(detail::Range{ .first = a, .last = b, .headFirst = aHead, .headLast = bHead });
		}
		ascg::Graph finalize() const {
			return fFinalize([] { return false; });
		}
		ascg::Graph finalize(ascg::IterationStop auto&& stopTest) const {
			return fFinalize(stopTest);
		}
	};
}
