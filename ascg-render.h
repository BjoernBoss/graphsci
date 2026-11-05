#pragma once

#include "ascg-common.h"
#include "ascg-layer.h"
#include "ascg-grid.h"

namespace ascg::detail {
	class Renderer {
	private:
		const std::vector<detail::Layer>& pLayers;
		const std::vector<detail::Row>& pGrid;

	public:
		Renderer(const std::vector<detail::Layer>& layers, const std::vector<detail::Row>& grid) : pLayers{ layers }, pGrid{ grid } {}

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
		std::pair<std::vector<std::string>, std::vector<size_t>> draw(size_t unusedNode) const {
			std::vector<std::string> drawn;
			std::vector<size_t> map;

			/* reserve the memory for the output types */
			drawn.reserve(pGrid.size());
			map.reserve(pGrid.size());

			/* render all of the rows and setup the map back to the layers (based on the preserved ordering of the nodes) */
			size_t node = 0;
			for (size_t i = 0; i < pGrid.size(); ++i) {
				drawn.push_back(fDrawRow(pGrid[i]));

				/* check if this row contained a node and otherwise mark it as an intermediate row */
				if (std::ranges::any_of(pGrid[i], [](auto val) -> bool { return val.kind == detail::CellKind::node; }))
					map.push_back(pLayers[node++].index);
				else
					map.push_back(unusedNode);
			}
			return { drawn, map };
		}
	};
}
