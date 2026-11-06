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
		std::string fDrawRow(const detail::Row& row) const {
			std::string line;

			/* iterate over the cells and convert them to the string */
			for (size_t i = 0; i < row.size(); ++i) {
				const detail::Cell& cell = row[i];

				/* add the character between the previous and current cell */
				if (i > 0) {
					if (cell.kind == detail::CellKind::node && row[i - 1].kind == cell.kind)
						line.push_back('#');
					else if (cell.anyLeft())
						line.push_back('-');
					else
						line.push_back(' ');
				}

				/* convert the cell itself to a character */
				if (cell.kind == detail::CellKind::node)
					line.push_back('#');
				else if (cell.kind == detail::CellKind::arrowTop)
					line.push_back('A');
				else if (cell.kind == detail::CellKind::arrowBottom)
					line.push_back('V');
				else if (cell.lineVertical())
					line.push_back('|');
				else if (cell.cornerTop())
					line.push_back('\'');
				else if (cell.cornerBottom())
					line.push_back(',');
				else if (cell.kind == detail::CellKind::horizontal)
					line.push_back('-');
				else
					line.push_back(' ');
			}
			return line;
		}

	public:
		ascg::Graph draw() const {
			ascg::Graph graph;

			/* reserve the memory for the output rows */
			graph.rows.reserve(pGrid.size());
			graph.map.reserve(pLayers.layers.size());

			/* render all of the rows and setup the map back to the layers (based on the preserved ordering of the nodes) */
			size_t node = 0;
			for (size_t i = 0; i < pGrid.size(); ++i) {
				graph.rows.push_back(fDrawRow(pGrid[i]));

				/* check if this row contained a node and update the map accordingly */
				if (std::ranges::any_of(pGrid[i], [](auto val) -> bool { return val.kind == detail::CellKind::node; }))
					graph.map.push_back({ pLayers.layers[node++].index, i });
			}
			return graph;
		}
	};
}
