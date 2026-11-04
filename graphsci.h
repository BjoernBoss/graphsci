#pragma once

#include "ascg-common.h"
#include "ascg-render.h"

namespace ascg {
	/* invalid value for normal node indices; used to identify intermediate rows in the 'ascg::Graph' */
	static constexpr size_t NoNode = std::numeric_limits<size_t>::max();

	/*
	*	Describes a layered graph made up of: # | - , '
	*	The graph is bidirectional, and cannot contain self-loops.
	*	The the layouting will select an ordering of the transitions and lay it out as ascii art.
	*	The ordering of the nodes from top to bottom will not be modified and will be used as provided.
	*/
	struct Graph {
		/* separate rows of the ascii graph */
		std::vector<std::string> rows;

		/* mapping of each row to the node it belongs to with 'ascg::NoNode' for intermediate rows */
		std::vector<size_t> map;
	};

	/* arguments for builders finalize method */
	struct ArgsBuilderFinalize {
		/* upper bound for iterations to shift grid nodes around to find the best placement */
		std::optional<size_t> gridBound = std::nullopt;
	};

	/* collect all edges of the graph and convert them to a final graph (ranges must not include
	*	ascg::NoNode as value and must not be identical, i.e. a connection to the same node) */
	class Builder {
	private:
		std::vector<detail::Range> pRanges;

	public:
		void connect(size_t a, size_t b) {
			/* ensure that the nodes are well ordered and considered valid */
			if (a > b)
				std::swap(a, b);
			else if (a == b)
				return;

			if (b < ascg::NoNode)
				pRanges.push_back(detail::Range{ .first = a, .last = b });
		}
		ascg::Graph finalize(const ascg::ArgsBuilderFinalize& args = {}) const {
			std::vector<detail::Layer> layers = detail::LayerBuilder{ pRanges }.build();
			std::vector<detail::Row> grid = detail::GridBuilder{ layers }.build(args.gridBound);
			auto [rows, map] = detail::Renderer{ layers, grid }.draw(ascg::NoNode);
			return ascg::Graph{ .rows = std::move(rows), .map = std::move(map) };
		}
	};
}
