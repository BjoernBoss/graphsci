/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 Bjoern Boss Henrichsen */
#pragma once

#include "ascg-common.h"
#include "ascg-layer.h"
#include "ascg-grid.h"

namespace ascg::detail {
	class Renderer {
	private:
		const detail::LayerState& pLayers;
		const detail::GridState& pGrid;

	public:
		Renderer(const detail::LayerState& layers, const detail::GridState& grid) : pLayers{ layers }, pGrid{ grid } {}

	private:
		std::string fDrawRow(const detail::Row& row, const ascg::Theme& theme) const {
			std::string line;
			line.reserve(row.size() + theme.spacing * (row.size() - 1));

			/* iterate over the cells and convert them to the string */
			for (size_t i = 0; i < row.size(); ++i) {
				const detail::Cell& cell = row[i];

				/* add the character between the previous and current cell */
				if (i > 0 && theme.spacing > 0) {
					std::string_view space;
					if (cell.kind == detail::CellKind::node && row[i - 1].kind == cell.kind)
						space = theme.node;
					else
						space = (cell.anyLeft() ? theme.horizontal : theme.empty);
					for (size_t j = 0; j < theme.spacing; ++j)
						line.append(space);
				}

				/* convert the cell itself to a character */
				switch (cell.kind) {
				case detail::CellKind::empty:
					line.append(theme.empty);
					break;
				case detail::CellKind::node:
					line.append(theme.node);
					break;
				case detail::CellKind::crossing:
					line.append(theme.crossing);
					break;
				case detail::CellKind::horizontal:
					line.append(theme.horizontal);
					break;
				case detail::CellKind::vertical:
					line.append(theme.vertical);
					break;
				case detail::CellKind::leftTop:
					line.append(theme.leftTop);
					break;
				case detail::CellKind::leftBottom:
					line.append(theme.leftBottom);
					break;
				case detail::CellKind::rightTop:
					line.append(theme.rightTop);
					break;
				case detail::CellKind::rightBottom:
					line.append(theme.rightBottom);
					break;
				case detail::CellKind::arrowTop:
					line.append(theme.arrowTop);
					break;
				case detail::CellKind::arrowBottom:
					line.append(theme.arrowBottom);
					break;
				}
			}
			return line;
		}

	public:
		ascg::Graph draw(const ascg::Theme& theme) const {
			/* check if the graph is empty, in which case nothing needs to be drawn */
			ascg::Graph graph;
			if (pGrid.empty())
				return graph;

			/* reserve the memory for the output rows */
			graph.rows.reserve(pGrid.size());
			graph.map.reserve(pLayers.layers.size());

			/* render all of the rows and setup the map back to the
			*	layers (based on the preserved ordering of the nodes) */
			size_t node = 0;
			for (size_t i = 0; i < pGrid.size(); ++i) {
				graph.rows.push_back(fDrawRow(pGrid[i], theme));

				/* check if this row contained a node and update the map accordingly */
				if (std::ranges::any_of(pGrid[i], [](auto val) -> bool { return val.kind == detail::CellKind::node; }))
					graph.map.push_back({ pLayers.layers[node++].index, i });
			}
			return graph;
		}
	};
}
