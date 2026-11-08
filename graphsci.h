#pragma once

#include "ascg-common.h"
#include "ascg-render.h"

namespace ascg {
	/* collect all edges of the graph and convert them to a
	*	final graph (only one transition between any two nodes) */
	class Builder {
	private:
		std::vector<detail::Range> pRanges;

	private:
		ascg::Graph fFinalize(ascg::IterationStop auto&& stopTest, const ascg::Theme& theme) const {
			detail::LayerState layers = detail::LayerBuilder{ pRanges }.build();
			detail::GridState grid = detail::GridBuilder{ layers }.build(stopTest);
			auto [rows, map] = detail::Renderer{ layers, grid }.draw(theme);
			return ascg::Graph{ .rows = std::move(rows), .map = std::move(map) };
		}

	public:
		void connect(size_t a, size_t b, bool aHead = false, bool bHead = false) {
			/* ensure that the nodes are well ordered */
			if (a > b)
				std::swap(a, b), std::swap(aHead, bHead);
			pRanges.push_back(detail::Range{ .first = a, .last = b, .headFirst = aHead, .headLast = bHead });
		}
		ascg::Graph finalize(const ascg::Theme& theme = ascg::AsciiTheme) const {
			return fFinalize([] { return false; }, theme);
		}
		ascg::Graph finalize(ascg::IterationStop auto&& stopTest, const ascg::Theme& theme = ascg::AsciiTheme) const {
			return fFinalize(stopTest, theme);
		}
	};
}
