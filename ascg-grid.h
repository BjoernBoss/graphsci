#pragma once

#include "ascg-common.h"
#include "ascg-layer.h"

namespace ascg::detail {
	static constexpr size_t SlotUnused = std::numeric_limits<size_t>::max();

	enum class CellKind : uint8_t {
		empty,
		node,
		crossing,
		horizontal,
		vertical,
		leftTop,
		leftBottom,
		rightTop,
		rightBottom
	};
	struct Cell {
		detail::CellKind kind = detail::CellKind::empty;
		bool cornerLeft() const {
			return (kind == detail::CellKind::leftTop || kind == detail::CellKind::leftBottom);
		}
		bool cornerRight() const {
			return (kind == detail::CellKind::rightTop || kind == detail::CellKind::rightBottom);
		}
		bool cornerTop() const {
			return (kind == detail::CellKind::leftTop || kind == detail::CellKind::rightTop);
		}
		bool cornerBottom() const {
			return (kind == detail::CellKind::leftBottom || kind == detail::CellKind::rightBottom);
		}
		bool lineHorizontal() const {
			return (kind == detail::CellKind::crossing || kind == detail::CellKind::horizontal);
		}
		bool lineVertical() const {
			return (kind == detail::CellKind::crossing || kind == detail::CellKind::vertical);
		}
		bool anyLeft() const {
			return (cornerLeft() || lineHorizontal());
		}
		bool anyTop() const {
			return (cornerTop() || lineVertical());
		}
		bool anyBottom() const {
			return (cornerBottom() || lineVertical());
		}
		bool anyCorner() const {
			return (cornerLeft() || cornerRight());
		}
		bool freeHorizontal() const {
			return (kind == detail::CellKind::empty || kind == detail::CellKind::vertical);
		}
		bool freeVertical() const {
			return (kind == detail::CellKind::empty || kind == detail::CellKind::horizontal);
		}
		detail::CellKind mirrorHorizontal() const {
			if (kind == detail::CellKind::leftTop)
				return detail::CellKind::leftBottom;
			if (kind == detail::CellKind::rightTop)
				return detail::CellKind::rightBottom;
			if (kind == detail::CellKind::leftBottom)
				return detail::CellKind::leftTop;
			if (kind == detail::CellKind::rightBottom)
				return detail::CellKind::rightTop;
			return kind;
		}
		detail::CellKind mirrorVertical() const {
			if (kind == detail::CellKind::leftTop)
				return detail::CellKind::rightTop;
			if (kind == detail::CellKind::rightTop)
				return detail::CellKind::leftTop;
			if (kind == detail::CellKind::leftBottom)
				return detail::CellKind::rightBottom;
			if (kind == detail::CellKind::rightBottom)
				return detail::CellKind::leftBottom;
			return kind;
		}
		detail::CellKind mirrorFull() const {
			if (kind == detail::CellKind::leftTop)
				return detail::CellKind::rightBottom;
			if (kind == detail::CellKind::rightTop)
				return detail::CellKind::leftBottom;
			if (kind == detail::CellKind::leftBottom)
				return detail::CellKind::rightTop;
			if (kind == detail::CellKind::rightBottom)
				return detail::CellKind::leftTop;
			return kind;
		}
		detail::CellKind addHorizontal() const {
			return (kind == detail::CellKind::vertical ? detail::CellKind::crossing : detail::CellKind::horizontal);
		}
		detail::CellKind addVertical() const {
			return (kind == detail::CellKind::horizontal ? detail::CellKind::crossing : detail::CellKind::vertical);
		}
		detail::CellKind removeHorizontal() const {
			return (kind == detail::CellKind::crossing ? detail::CellKind::vertical : detail::CellKind::empty);
		}
		detail::CellKind removeVertical() const {
			return (kind == detail::CellKind::crossing ? detail::CellKind::horizontal : detail::CellKind::empty);
		}
	};
	using Row = std::vector<detail::Cell>;

	class GridBuilder {
	private:
		enum class Alignment : uint8_t {
			leftwards,
			rightwards,
			outwards
		};

	private:
		const std::vector<detail::Layer>& pLayers;
		mutable std::vector<detail::Row> pGrid;

	public:
		GridBuilder(const std::vector<detail::Layer>& layers) : pLayers{ layers } {}

	private:
		void fFillTraversals(detail::Row& row, std::vector<size_t>& slice, size_t begin, size_t end) const {
			for (size_t i = begin; i < end; ++i) {
				if (slice[i] == detail::SlotUnused)
					row.push_back(detail::Cell{});
				else
					row.push_back(detail::Cell{ detail::CellKind::vertical });
			}
		}
		size_t fInsertRotation(std::vector<size_t>& slice, size_t begin, size_t end, size_t target, size_t where) const {
			/* add the rotation of the range [begin, end) with begin finally being placed at target */
			if (begin == target) return where;
			bool leftwards = (target < begin);
			size_t distance = (leftwards ? (begin - target) : (target - begin));

			/* iterate until no more entries need to be moved */
			while (begin != end) {
				detail::Row& row = *pGrid.emplace(pGrid.begin() + where++);

				/* add all traversals before the next rotation */
				fFillTraversals(row, slice, 0, (leftwards ? begin - distance : end - 1));

				/* add the next rotation itself */
				row.push_back(detail::Cell{ leftwards ? detail::CellKind::rightBottom : detail::CellKind::rightTop });
				for (size_t i = 1; i < distance; ++i)
					row.push_back(detail::Cell{ detail::CellKind::horizontal });
				row.push_back(detail::Cell{ leftwards ? detail::CellKind::leftTop : detail::CellKind::leftBottom });

				/* add the traversals after the rotation and update the slice itself and the indices */
				fFillTraversals(row, slice, (leftwards ? begin + 1 : end + distance), slice.size());
				if (leftwards) {
					std::swap(slice[begin], slice[begin - distance]);
					++begin;
				}
				else {
					--end;
					std::swap(slice[end], slice[end + distance]);
				}
			}
			return where;
		}
		size_t fInsertSliceCorrections(std::vector<size_t>& slice, size_t where) const {
			/* find the next misplaced entry, which can be moved to its new placement (loop until they have all
			*	been moved; must always find one, as the caller guarantees that the slice targets are possible) */
			for (size_t i = 0; i < slice.size();) {
				if (slice[i] == detail::SlotUnused || slice[i] == i) {
					++i;
					continue;
				}

				/* check if the entire range is free for the slice slot to be rotated into place */
				bool blocked = false;
				for (size_t j : detail::MinMaxWalk{ i, slice[i], false }) {
					if (j == i || slice[j] == detail::SlotUnused)
						continue;
					blocked = true;
					break;
				}

				/* apply the rotation, if its not blocked, and update the indices */
				if (blocked)
					++i;
				else {
					where = fInsertRotation(slice, i, i + 1, slice[i], where);
					i = 0;
				}
			}
			return where;
		}
		std::pair<size_t, size_t> fCreateEmptyCell(size_t x, size_t y, bool leftwards) const {
			if (pGrid[y][x].kind == detail::CellKind::empty)
				return{ x, y };

			/* check if the entire grid needs to be widened to accomodate the shifting of the affected
			*	cells (to ensure all upcoming operations can perform the shifts without checking bounds) */
			auto shiftIntoRange = std::ranges::subrange(pGrid[y].begin() + (leftwards ? 0 : x + 1), (leftwards ? pGrid[y].begin() + x : pGrid[y].end()));
			if (!std::ranges::any_of(shiftIntoRange, [](auto v) { return (v == detail::CellKind::empty); }, &detail::Cell::kind))
				x = fInsertEmptyColumn(leftwards, x);

			/* setup the slice to move the previous entries (may invalidate references) */
			std::vector<size_t> slice(pGrid[y].size(), detail::SlotUnused);
			if (y > 0) {
				detail::Row& prev = pGrid[y - 1];
				detail::Row& cur = pGrid[y];

				/* setup the initial identity mapping (look at both neighbors to track transitions coming out of nodes) */
				for (size_t i = 0; i < cur.size(); ++i)
					slice[i] = ((prev[i].anyBottom() || cur[i].anyTop()) ? i : detail::SlotUnused);

				/* iterate again and apply the shift to the slice such that they all target their new shifted position */
				if (leftwards) for (size_t i = x + 1; i > 0; --i) {
					if (slice[i - 1] != detail::SlotUnused)
						--slice[i - 1];
					else if (i <= x && cur[i - 1].kind == detail::CellKind::empty)
						break;
				}
				else for (size_t i = x; i < cur.size(); ++i) {
					if (slice[i] != detail::SlotUnused)
						++slice[i];
					else if (i > x && cur[i].kind == detail::CellKind::empty)
						break;
				}

				/* insert the shifting */
				y = fInsertSliceCorrections(slice, y);
			}

			/* setup the slice to move the next entries (may invalidate references) */
			if (y + 1 < pGrid.size()) {
				detail::Row& cur = pGrid[y];
				detail::Row& next = pGrid[y + 1];

				/* setup the initial identity mapping (look at both neighbors to track transitions coming out of nodes) */
				for (size_t i = 0; i < cur.size(); ++i)
					slice[i] = ((cur[i].anyBottom() || next[i].anyTop()) ? i : detail::SlotUnused);

				/* iterate again and apply the shift to the slice such that they each start at their new shifted position */
				size_t current = detail::SlotUnused;
				if (leftwards) for (size_t i = x + 1; i > 0; --i) {
					std::swap(current, slice[i - 1]);
					if (current == detail::SlotUnused && i <= x && cur[i - 1].kind == detail::CellKind::empty)
						break;
				}
				else for (size_t i = x; i < cur.size(); ++i) {
					std::swap(current, slice[i]);
					if (current == detail::SlotUnused && i > x && cur[i].kind == detail::CellKind::empty)
						break;
				}

				/* insert the shifting */
				fInsertSliceCorrections(slice, y + 1);
			}

			/* shift the original row itself into position (after the previous and
			*	upcoming rows in order to ensure the slices can be constructed properly) */
			detail::Row& row = pGrid[y];
			detail::Cell temp{};
			if (leftwards) for (size_t i = x + 1; i > 0; --i) {
				std::swap(temp, row[i - 1]);
				if (temp.kind == detail::CellKind::empty)
					break;
			}
			else for (size_t i = x; i < row.size(); ++i) {
				std::swap(temp, row[i]);
				if (temp.kind == detail::CellKind::empty)
					break;
			}
			return{ x, y };
		}
		void fInsertNullRow(size_t index) const {
			detail::Row& row = *pGrid.emplace(pGrid.begin() + index);
			detail::Row& prev = pGrid[index - 1];
			detail::Row& next = pGrid[index + 1];
			row.reserve(prev.size());

			/* insert a null row at index, which only consists of vertical edges or empty space */
			for (size_t i = 0; i < prev.size(); ++i)
				row.push_back({ (prev[i].anyBottom() || next[i].anyTop()) ? detail::CellKind::vertical : detail::CellKind::empty });
		}
		size_t fInsertEmptyColumn(bool leftSide, size_t index) const {
			size_t where = (leftSide ? 0 : pGrid[0].size());

			/* insert the new empty cells */
			for (size_t i = 0; i < pGrid.size(); ++i)
				pGrid[i].insert(pGrid[i].begin() + where, detail::Cell{});

			/* check if the index needs to be moved */
			return (leftSide ? index + 1 : index);
		}

	private:
		std::map<size_t, size_t> fSetupNodeMap(size_t index, size_t width) const {
			const detail::Layer& layer = pLayers[index];
			std::map<size_t, size_t> out;

			/* helper to collect the endpoint ranges and sort them by their
			*	other side (the other-side ordering is preserved into the grid) */
			const auto sortEndpoints = [](const detail::Endpoints& eps) -> std::vector<size_t> {
				const detail::Endpoint* list = eps.begin();
				std::vector<size_t> out;

				/* initially collect the indices and sort on them */
				out.reserve(eps.size());
				for (size_t i = 0; i < eps.size(); ++i)
					out.push_back(i);
				std::sort(out.begin(), out.end(), [list](size_t f, size_t s) -> bool {
					return ((list[f].other != list[s].other) ? (list[f].other < list[s].other) : (f < s));
				});

				/* convert the indices back to the actual ranges */
				for (size_t& val : out)
					val = list[val].range;
				return out;
			};

			/* iterate over the nodes and assign each range the index it lies at (identical for both starting and ending) */
			size_t slotIndex = 0;
			for (size_t i = 0; i < layer.points.size(); ++i) {
				const detail::Point& point = layer.points[i];

				/* check if its a simple traversal */
				if (const detail::Traverse* traverse = point.isTraverse(); traverse != nullptr) {
					out[traverse->range] = slotIndex++;
					continue;
				}
				const detail::Node& node = point.node();

				/* update the indices for the starting and ending nodes */
				size_t endIndex = slotIndex;
				for (size_t range : sortEndpoints(node.start))
					out[range] = slotIndex++;
				for (size_t range : sortEndpoints(node.end))
					out[range] = endIndex++;
				slotIndex = std::max<size_t>(slotIndex, endIndex);
			}

			/* compute the graph shift to center the layer and apply it to the indices */
			if (size_t shift = (width - slotIndex) / 2; shift > 0) for (auto& [_, slot] : out)
				slot += shift;
			return out;
		}
		std::vector<size_t> fConstructNode(size_t index, size_t width, std::map<size_t, size_t>& mapping) const {
			const detail::Layer& layer = pLayers[index];

			/* setup the initial slice and row */
			detail::Row& row = pGrid.emplace_back(width, detail::Cell{});
			std::vector<size_t> slice(width, detail::SlotUnused);

			/* setup the mapping of the next node (necessary to populate the slice) */
			std::map<size_t, size_t> upcoming = (index + 1 >= pLayers.size() ? std::map<size_t, size_t>{} : fSetupNodeMap(index + 1, width));

			/* iterate over the nodes and write them out to the grid and update the slice simultaneously */
			for (const detail::Point& point : layer.points) {
				/* check if its a simple traversal */
				if (const detail::Traverse* traverse = point.isTraverse(); traverse != nullptr) {
					size_t where = mapping[traverse->range];
					row[where].kind = detail::CellKind::vertical;
					slice[where] = upcoming[traverse->range];
					continue;
				}
				const detail::Node& node = point.node();

				/* insert all of the node points to the row (iterate over the larger of the nodes to ensure all cells are filled) */
				for (const detail::Endpoint& ep : (node.start.size() > node.end.size() ? node.start : node.end))
					row[mapping[ep.range]].kind = detail::CellKind::node;

				/* update the slice to contain the ranges starting here */
				for (const detail::Endpoint& ep : node.start)
					slice[mapping[ep.range]] = upcoming[ep.range];
			}

			/* replace the mapping with the mapping of the upcoming node and return the slice */
			mapping = std::move(upcoming);
			return slice;
		}
		void fConstructAddCrossing(std::vector<size_t>& slice, size_t index, size_t target) const {
			detail::Row& row = pGrid.emplace_back();
			bool leftwards = (target < index);

			/* add the traversals before the crossing */
			fFillTraversals(row, slice, 0, std::min<size_t>(index, target));

			/* add the crossing itself */
			row.push_back(detail::Cell{ leftwards ? detail::CellKind::rightBottom : detail::CellKind::rightTop });
			for (size_t i : detail::MinMaxWalk{ index, target, true }) {
				if (slice[i] == detail::SlotUnused)
					row.push_back(detail::Cell{ detail::CellKind::horizontal });
				else
					row.push_back(detail::Cell{ detail::CellKind::crossing });
			}
			row.push_back(detail::Cell{ leftwards ? detail::CellKind::leftTop : detail::CellKind::leftBottom });

			/* add the traversals after the crossing and update the slice itself */
			fFillTraversals(row, slice, std::max<size_t>(index, target) + 1, slice.size());
			std::swap(slice[index], slice[target]);
		}
		void fConstructTransitions(std::vector<size_t>& slice) const {
			size_t numberOfRows = pGrid.size();

			/* add all rotations and swaps to ensure the slice is in order */
			while (true) {
				size_t bestIndex = 0, bestSkip = 0, bestTarget = 0;
				bool bestCanMove = false, bestMoveLeft = false;

				/* find the next entry which can already move or which has to move the furthest */
				for (size_t i = 0; i < slice.size(); ++i) {
					if (slice[i] == detail::SlotUnused)
						continue;
					size_t leftSkip = 0, leftTarget = 0, rightSkip = 0, rightTarget = 0;

					/* count the number of connections to skip to move it in order
					*	and check if the entry is already positioned correctly */
					for (size_t j = i; j > 0; --j) {
						if (slice[j - 1] == detail::SlotUnused) continue;
						if (slice[j - 1] < slice[i]) break;
						++leftSkip;
						leftTarget = j - 1;
					}
					for (size_t j = i + 1; j < slice.size(); ++j) {
						if (slice[j] == detail::SlotUnused) continue;
						if (slice[j] > slice[i]) break;
						++rightSkip;
						rightTarget = j;
					}
					if (leftSkip == 0 && rightSkip == 0)
						continue;

					/* check if the left direction is better than the current best */
					if (leftSkip > bestSkip) {
						bestIndex = i;
						bestSkip = leftSkip;
						bestTarget = leftTarget;
						bestMoveLeft = true;
					}

					/* check if the right direction is better than the current best */
					if (rightSkip > bestSkip) {
						bestIndex = i;
						bestSkip = rightSkip;
						bestTarget = rightTarget;
						bestMoveLeft = false;
					}
				}

				/* check if no swap was found or if it can be moved immediately */
				if (bestSkip == 0)
					break;
				bestCanMove = (bestMoveLeft ? (bestTarget > 0 && slice[bestTarget - 1] == detail::SlotUnused) : (bestTarget + 1 < slice.size() && slice[bestTarget + 1] == detail::SlotUnused));
				if (bestCanMove) {
					fConstructAddCrossing(slice, bestIndex, (bestMoveLeft ? bestTarget - 1 : bestTarget + 1));
					continue;
				}

				/* shift the target for right movements, as this reduces the off-by-one patching for the upcoming loops */
				if (!bestMoveLeft)
					++bestTarget;

				/* find the next possible empty slot in either direction, and count the number of
				*	moves it would require (on the right side, there are more implicit free slots) */
				size_t leftMove = 0, rightMove = 0;
				for (size_t i = bestTarget + 1; i <= slice.size(); ++i) {
					if (i < slice.size() && slice[i] != detail::SlotUnused) continue;
					rightMove = i - bestTarget;
					break;
				}
				for (size_t i = bestTarget; i > 0; --i) {
					if (slice[i - 1] != detail::SlotUnused) continue;
					leftMove = bestTarget - i;
					break;
				}

				/* check if the left empty slot should be used and add the necessary rotations and patch the indices accordingly */
				if ((leftMove > 0 && leftMove <= rightMove) || rightMove == 0) {
					fInsertRotation(slice, bestTarget - leftMove, bestTarget, bestTarget - leftMove - 1, pGrid.size());
					--bestTarget;
					if (bestTarget > bestIndex && bestTarget - leftMove < bestIndex)
						--bestIndex;
				}

				/* apply the rotation to the right and patch the indices accordingly (right side might require an implicit new entry) */
				else {
					if (bestTarget + rightMove == slice.size())
						slice.push_back(detail::SlotUnused);
					fInsertRotation(slice, bestTarget, bestTarget + rightMove, bestTarget + 1, pGrid.size());
					if (bestTarget < bestIndex && bestTarget + rightMove > bestIndex)
						++bestIndex;
				}

				/* apply the final actual crossing */
				fConstructAddCrossing(slice, bestIndex, bestTarget);
			}

			/* insert the remaining slice corrections until the slice is well-placed (must not be possible, as they are well-ordered
			*	and can all fit into the row, and any misplacement implies that there must be empty spots to use for shifting) */
			fInsertSliceCorrections(slice, pGrid.size());

			/* ensure that at least one row was added, as otherwise two nodes may directly touch */
			if (numberOfRows == pGrid.size())
				fFillTraversals(pGrid.emplace_back(), slice, 0, slice.size());
		}
		void fConstructGrid() const {
			pGrid.clear();
			if (pLayers.empty())
				return;

			/* compute the minimum width to give to the graph (minimum
			*	required plus maximum number of crossings between two layers) */
			size_t lastWidth = 0, totalWidth = 0;
			for (const detail::Layer& layer : pLayers) {
				size_t width = layer.points.size(), crossings = 0;

				/* helper to sum up the crossings of the current edge with the previous edges */
				const auto fetchCrossings = [&layer](size_t until, size_t target) -> size_t {
					size_t count = 0;
					for (size_t i = 0; i < until; ++i) {
						if (const detail::Traverse* traverse = layer.points[i].isTraverse(); traverse != nullptr)
							count += (traverse->prev > target ? 1 : 0);
						else for (const detail::Endpoint& ep : layer.points[i].node().end)
							count += (ep.other > target ? 1 : 0);
					}
					return count;
				};

				/* iterate over all points and update the width due to the node and compute the crossings */
				for (size_t i = 0; i < layer.points.size(); ++i) {
					if (const detail::Node* node = layer.points[i].isNode(); node != nullptr) {
						for (const detail::Endpoint& ep : node->end)
							crossings += fetchCrossings(i, ep.other);
						width += std::max<size_t>(node->start.size(), node->end.size()) - 1;
					}
					else
						crossings += fetchCrossings(i, layer.points[i].traverse().prev);
				}

				/* patch the total width to include the width plus the crossings for both this and the previous layer */
				totalWidth = std::max<size_t>({ totalWidth, width + crossings, lastWidth + crossings });
				lastWidth = width;
			}

			/* fetch the initial node mapping and iterate over the layers and convert
			*	them to node-rows in the grid and add the intermediate transitions */
			std::map<size_t, size_t> mapping = fSetupNodeMap(0, totalWidth);
			for (size_t i = 0;; ++i) {
				std::vector<size_t> slice = fConstructNode(i, totalWidth, mapping);
				if (i + 1 >= pLayers.size())
					break;
				fConstructTransitions(slice);
			}

			/* flood the entire grid to be rectangular (re-compute the width,
			*	in case some placement or rotations added additional columns) */
			for (size_t i = 0; i < pGrid.size(); ++i)
				totalWidth = std::max<size_t>(totalWidth, pGrid[i].size());
			for (detail::Row& row : pGrid)
				row.resize(totalWidth);
		}

	private:
		size_t fCountPathCrossings(size_t x, size_t y, size_t other, bool horizontal, bool future) const {
			size_t count = 0;

			/* select the value to be walked across the grid and the kind, which
			*	either is a crossing, or would result in a crossing in the future */
			size_t& walker = (horizontal ? x : y);
			if (walker == other)
				return 0;
			detail::CellKind critical = (future
				? (horizontal ? detail::CellKind::vertical : detail::CellKind::horizontal)
				: detail::CellKind::crossing);

			/* walk across the exclusive range and count the number of critical kind encounters */
			while ((walker = detail::Towards(walker, other)) != other) {
				if (pGrid[y][x].kind == critical)
					++count;
			}
			return count;
		}
		bool fMapIndentCorner(size_t x, size_t y, size_t& xOther, size_t& yOther, size_t& xEnd, size_t& target) const {
			const detail::Cell& cell = pGrid[y][x];

			/* find the other corner of this construct (i.e. after moving horizontal and vertical again, the endpoint) */
			if (cell.cornerLeft()) while (pGrid[yOther][--xOther].lineHorizontal());
			else while (pGrid[yOther][++xOther].lineHorizontal());
			if (cell.cornerTop()) while (pGrid[++yOther][xOther].lineVertical());
			else while (pGrid[--yOther][xOther].lineVertical());

			/* find the end of the next horizontal axis walking towards the original horizontal position */
			bool endIsNode = (pGrid[yOther][xOther].kind == detail::CellKind::node);
			xEnd = xOther;
			if (endIsNode) for (size_t i = xEnd; pGrid[yOther][i].kind == detail::CellKind::node && xEnd != x; i = detail::Towards(i, x))
				xEnd = i;
			else if (pGrid[yOther][xOther].kind != cell.kind) {
				size_t limit = (xOther < x ? pGrid[y].size() : 0);
				while (pGrid[yOther][xEnd = detail::Towards(xEnd, limit)].lineHorizontal());

				/* check if the other side is larger than the current horizontal, in which case the corner [x,y]
				*	is not the primary corner, and it can be ignored until the primary corner is inspected again */
				if (!detail::Inbetween(xEnd, x, xOther))
					return false;
			}

			/* check if the end of the node has already been reached, in which case nothing can be done anymore */
			if (endIsNode && xEnd == xOther)
				return false;

			/* find the target axis to reduce the indentation by */
			target = (endIsNode ? xEnd : x);
			while (target != xOther) {
				/* check if the own edge is free and if the other corner is free */
				bool valid = (target == x || pGrid[y][target].kind == detail::CellKind::horizontal);
				if (valid && !endIsNode && !detail::Inbetween(target, xOther, xEnd))
					valid = (pGrid[yOther][target].kind == detail::CellKind::empty);

				/* check the vertical axis from the primary corner to the height of the other corner */
				if (valid) for (size_t i : detail::MinMaxWalk{ y, yOther, true }) {
					if (!(valid = pGrid[i][target].freeVertical()))
						break;
				}

				/* incase of the primary corner lying further than the other corner,
				*	check if the horizontal to the secondary corner is free as well */
				if (valid && !detail::Inbetween(target, xOther, xEnd)) for (size_t i : detail::MinMaxWalk{ xEnd, target, true }) {
					if (!(valid = pGrid[yOther][i].freeHorizontal()))
						break;
				}

				/* ensure that the future new shortcut would not introduce any new crossings */
				if (valid) {
					size_t current = fCountPathCrossings(xOther, y, target, true, false)
						+ fCountPathCrossings(xOther, y, yOther, false, false);
					if (!endIsNode) {
						size_t end = (detail::Inbetween(target, xOther, xEnd) ? target : xEnd);
						current += fCountPathCrossings(xOther, yOther, end, true, false);
					}
					size_t future = fCountPathCrossings(target, y, yOther, false, true);
					if (!endIsNode && detail::Inbetween(xEnd, xOther, target))
						future += fCountPathCrossings(target, yOther, xEnd, true, true);
					valid = (future <= current);
				}

				/* check if a valid target was found and otherwise move it closer to the other corner and try again */
				if (valid)
					break;
				target = detail::Towards(target, xOther);
			}
			return true;
		}
		void fInsertDualSwap(size_t y, size_t yOther, size_t xCorner, size_t xCross) const {
			/* insert the empty row to be used (insert it away from the y-other) */
			size_t yMove = (yOther > y ? y++ : y + 1);
			fInsertNullRow(yMove);

			/* move the original corner down to let the blocking transition be used the the original transition */
			pGrid[y][xCross].kind = pGrid[y][xCorner].kind;

			/* patch the original transition path to move into the original blocker */
			pGrid[yMove][xCorner].kind = pGrid[y][xCorner].kind;
			pGrid[y][xCorner].kind = detail::CellKind::vertical;
			pGrid[yMove][xCross].kind = pGrid[y][xCross].mirrorFull();
		}
		void fInsertDualCornerBlock(size_t xCorner, size_t yCorner, bool leftwards) const {
			/* check if the index lies right at the bounds, in which case
			*	the grid needs to be extended to provide space for a detour */
			if (leftwards ? xCorner == 0 : xCorner + 1 == pGrid[yCorner].size())
				xCorner = fInsertEmptyColumn(leftwards, xCorner);

			/* ensure an empty space exists exists on the other side of the corner to bend around (caller
			*	must guarantee that the corner does not point into the empty spot to prevent invalid graphs)
			*	and afterwards duplicate the line before and after to ensure three consecutive empty spots */
			auto [xEmpty, yEmpty] = fCreateEmptyCell((leftwards ? xCorner - 1 : xCorner + 1), yCorner, leftwards);
			fInsertNullRow(yEmpty++);
			fInsertNullRow(yEmpty + 1);

			/* insert the vertical connection through the empty three spots, which will now be on the other side of the corner */
			pGrid[yEmpty - 1][xEmpty].kind = (leftwards ? detail::CellKind::rightBottom : detail::CellKind::leftBottom);
			pGrid[yEmpty][xEmpty].kind = pGrid[yEmpty][xEmpty].addVertical();
			pGrid[yEmpty + 1][xEmpty].kind = (leftwards ? detail::CellKind::rightTop : detail::CellKind::leftTop);
			xEmpty = (leftwards ? xEmpty + 1 : xEmpty - 1);

			/* insert the horizontal connections for the old transition to connect to the new detour */
			pGrid[yEmpty - 1][xEmpty].kind = pGrid[yEmpty - 1][xEmpty].addHorizontal();
			pGrid[yEmpty + 1][xEmpty].kind = pGrid[yEmpty + 1][xEmpty].addHorizontal();
			xEmpty = (leftwards ? xEmpty + 1 : xEmpty - 1);

			/* patch the original transition to connect to the detour */
			pGrid[yEmpty - 1][xEmpty].kind = (leftwards ? detail::CellKind::leftTop : detail::CellKind::rightTop);
			pGrid[yEmpty][xEmpty].kind = pGrid[yEmpty][xEmpty].removeVertical();
			pGrid[yEmpty + 1][xEmpty].kind = (leftwards ? detail::CellKind::leftBottom : detail::CellKind::rightBottom);
		}
		bool fInsertSingleCornerBlock(size_t xBase, size_t yBase, size_t xBlock, size_t yBlock) const {
			size_t xSpacer = (xBase < xBlock ? xBlock + 1 : xBlock - 1);
			bool baseLower = (yBase < yBlock);

			/* check if the blocking corner points into the base row and the base row can be crossed */
			if (baseLower ? !pGrid[yBlock][xBlock].cornerTop() : !pGrid[yBlock][xBlock].cornerBottom())
				return false;
			if (!pGrid[yBase][xSpacer].freeVertical())
				return false;

			/* check if the entire path between the corner and the base is free in order for the corner transition to be moved */
			for (size_t i : detail::MinMaxWalk{ yBase, yBlock, true }) {
				if (!pGrid[i][xSpacer].freeVertical())
					return false;
			}

			/* insert the empty rows around the base row */
			fInsertNullRow(yBase++);
			fInsertNullRow(yBase + 1);
			if (baseLower)
				yBlock += 2;

			/* insert the shift on the outside of the base row */
			size_t yOutside = (baseLower ? yBase - 1 : yBase + 1);
			pGrid[yOutside][xBlock].kind = pGrid[yBlock][xBlock].kind;
			pGrid[yOutside][xSpacer].kind = pGrid[yBlock][xBlock].mirrorFull();

			/* shift the crossing itself over */
			pGrid[yBase][xBlock].kind = detail::CellKind::horizontal;
			pGrid[yBase][xSpacer].kind = detail::CellKind::crossing;

			/* insert the shift on the inside of the base row */
			size_t yInside = (baseLower ? yBase + 1 : yBase - 1);
			pGrid[yInside][xBlock].kind = pGrid[yOutside][xBlock].mirrorHorizontal();
			pGrid[yInside][xSpacer].kind = pGrid[yOutside][xSpacer].mirrorHorizontal();
			return true;
		}
		intptr_t fShiftCheckPreference(size_t y, size_t x, bool forwards, bool leftwards) const {
			enum ShiftScore : intptr_t {
				extendShort = -2,
				addShift = -2,
				extendShift = -1,
				uncertain = 0,
				nothing = 0,
				reduceShift = 1,
				dropShift = 3
			};

			/* check if the transition actually exists and points towards the node */
			size_t last = (forwards ? pGrid.size() - 1 : 0);
			if (y == last)
				return ShiftScore::nothing;
			y = detail::Towards(y, last);
			if (forwards ? !pGrid[y][x].anyTop() : !pGrid[y][x].anyBottom())
				return ShiftScore::nothing;

			/* follow the strand until the first shift to the left or right is
			*	encountered or until the connection is considered blocked */
			while (pGrid[y][x].lineVertical()) {
				detail::Row& row = pGrid[y];
				y = detail::Towards(y, last);

				/* check if the connection is blocked to the checked side */
				if (leftwards) for (size_t i = x; i > 0 && row[i - 1].kind != detail::CellKind::empty; --i) {
					if (row[i - 1].kind == detail::CellKind::node)
						return ShiftScore::addShift;
				}
				else for (size_t i = x + 1; i < row.size() && row[i].kind != detail::CellKind::empty; ++i) {
					if (row[i].kind == detail::CellKind::node)
						return ShiftScore::addShift;
				}
			}

			/* evaluate the final pull direction */
			if (pGrid[y][x].cornerLeft()) {
				bool single = pGrid[y][x - 1].cornerRight();
				if (!leftwards)
					return (single ? ShiftScore::extendShort : ShiftScore::extendShift);
				return (single ? ShiftScore::dropShift : ShiftScore::reduceShift);
			}
			if (pGrid[y][x].cornerRight()) {
				bool single = pGrid[y][x + 1].cornerLeft();
				if (leftwards)
					return (single ? ShiftScore::extendShort : ShiftScore::extendShift);
				return (single ? ShiftScore::dropShift : ShiftScore::reduceShift);
			}

			/* the final position must be ending in a node,
			*	check if there is space in the given direction */
			const detail::Row& prev = pGrid[forwards ? y - 1 : y + 1];
			if (leftwards ? x == 0 : x + 1 == prev.size())
				return ShiftScore::nothing;
			const detail::Cell& neighbor = prev[leftwards ? x - 1 : x + 1];
			if (forwards ? neighbor.anyBottom() : neighbor.anyTop())
				return ShiftScore::addShift;
			return ShiftScore::nothing;
		}

	private:
		bool fStraightenGraph(Alignment align) const {
			/* find the next corner to pursue (iterate until one has been found or all have been checked) */
			for (size_t y = 0; y < pGrid.size(); ++y) {
				detail::Row& row = pGrid[y];

				/* check if the width of the row needs to be determined (to find its center) */
				size_t halfRow = row.size();
				if (align == Alignment::outwards) {
					while (halfRow > 0 && row[halfRow - 1].kind == detail::CellKind::empty)
						--halfRow;
					if (halfRow > 0) for (size_t i = 0; row[i].kind == detail::CellKind::empty; ++i)
						--halfRow;
					halfRow /= 2;
				}

				/* find any matching corners within the row */
				for (size_t x = 0; x < row.size(); ++x) {
					detail::Cell& cell = row[x];

					/* select the allowed direction to be used and check if the corner is a potential indented corner for this iteration
					*	(must have a corner in the opposite direction in order to potentially want a change in the intended direction) */
					bool dirLeftwards = (align == Alignment::outwards ? (x <= halfRow) : (align == Alignment::leftwards));
					if (!(dirLeftwards ? cell.cornerRight() : cell.cornerLeft()))
						continue;

					/* resolve the indentation and check if a reduction path can be found and apply it */
					size_t xOther = x, yOther = y, xEnd = 0, target = 0;
					if (!fMapIndentCorner(x, y, xOther, yOther, xEnd, target) || target == xOther)
						continue;
					bool endIsNode = (pGrid[yOther][xOther].kind == detail::CellKind::node);

					/* check if this is an outwards movement and the other side sits on the other side of
					*	the center, in which case the move needs to be skipped to prevent oscillations */
					if (align == Alignment::outwards && (xOther <= halfRow) != (x <= halfRow))
						continue;

					/* move the vertical line over */
					for (size_t i : detail::MinMaxWalk{ y, yOther, true }) {
						pGrid[i][target].kind = pGrid[i][target].addVertical();
						pGrid[i][xOther].kind = pGrid[i][xOther].removeVertical();
					}

					/* remove the primary and update the other horizontal line */
					for (size_t i : detail::MinMaxWalk{ xOther, target, true }) {
						pGrid[y][i].kind = pGrid[y][i].removeHorizontal();
						if (!endIsNode && i != xEnd)
							pGrid[yOther][i].kind = (detail::Inbetween(i, xOther, xEnd) ? pGrid[yOther][i].removeHorizontal() : pGrid[yOther][i].addHorizontal());
					}

					/* patch all of the affected corners */
					if (!endIsNode) {
						if (detail::Inbetween(target, xOther, xEnd))
							pGrid[yOther][target].kind = (target == xEnd ? detail::CellKind::vertical : pGrid[y][x].mirrorVertical());
						else {
							pGrid[yOther][xEnd].kind = pGrid[yOther][xEnd].mirrorVertical();
							pGrid[yOther][target].kind = pGrid[y][x].kind;
						}
						pGrid[yOther][xOther].kind = (xOther == xEnd ? detail::CellKind::horizontal : detail::CellKind::empty);
					}
					pGrid[y][target].kind = (target == x ? detail::CellKind::vertical : pGrid[y][xOther].kind);
					pGrid[y][xOther].kind = detail::CellKind::empty;
					return true;
				}
			}
			return false;
		}
		bool fFixInterlocks(bool leftwards) const {
			/* find the next corner to pursue (iterate until one has been found or all have been checked) */
			for (size_t y = 0; y < pGrid.size(); ++y) {
				detail::Row& row = pGrid[y];
				for (size_t x = 0; x < row.size(); ++x) {
					detail::Cell& cell = row[x];

					/* check if the corner is a potential indented corner for this iteration (must have a corner
					*	in the opposite direction in order to potentially want a change in the intended direction) */
					if (!(leftwards ? cell.cornerRight() : cell.cornerLeft()))
						continue;

					/* resolve the indentation and check if the current indentation is blocked by
					*	another indentation into the opposite direction and try to resolve them */
					size_t xOther = x, yOther = y, xEnd = 0, target = 0;
					if (!fMapIndentCorner(x, y, xOther, yOther, xEnd, target) || target != xOther)
						continue;
					bool endIsNode = (pGrid[yOther][xOther].kind == detail::CellKind::node);

					/* check if there are any nodes in the way, in which case the interlocking
					*	cannot be removed and otherwise look for the first and last blocking corner */
					size_t xCorner = detail::Towards(xOther, x), yFirstCorner = y, yLastCorner = y;
					bool blockedTransition = false;
					for (size_t i : detail::MinMaxWalk{ y, yOther, true }) {
						if (pGrid[i][xCorner].kind == detail::CellKind::node) {
							blockedTransition = true;
							break;
						}
						if (!pGrid[i][xCorner].anyCorner())
							continue;
						if (yFirstCorner == y)
							yFirstCorner = i;
						yLastCorner = i;
					}
					if (blockedTransition)
						continue;

					/* check if no corner has been found, which can only be the case if the blocker spans the
					*	entire range of the original transition (might result in reduction of crossings) */
					if (yFirstCorner == y) {
						/* check if both end in the same node, in which case the transitions can be swapped */
						if (pGrid[yOther][xCorner].kind == detail::CellKind::node && pGrid[yOther][xOther].kind == detail::CellKind::node) {
							fInsertDualSwap(y, yOther, xOther, xCorner);
							return true;
						}

						/* check if the blocked transition crosses the blocking transition again as nothing can otherwise be done */
						if (pGrid[yOther][xCorner].kind != detail::CellKind::crossing)
							continue;

						/* the two transitions are crossing but lay in parallel next to each other, in which case they can just be
						*	swapped (first the higher positions, as the swap inserts new rows and may otherwise skew the other y position) */
						size_t yPrev = std::min<size_t>(y, yOther), yNext = std::max<size_t>(y, yOther);
						fInsertDualSwap(yNext, yPrev, xOther, xCorner);
						fInsertDualSwap(yPrev, yNext, xOther, xCorner);
						return true;
					}

					/* check if the first/last corner point into or out of the blocking
					*	transition (i.e. blocking each other or only one blocked by the other) */
					bool firstDualBlock = (leftwards ? pGrid[yFirstCorner][xCorner].cornerRight() : pGrid[yFirstCorner][xCorner].cornerLeft());
					bool lastDualBlock = (leftwards ? pGrid[yLastCorner][xCorner].cornerRight() : pGrid[yLastCorner][xCorner].cornerLeft());

					/* check if a corner points into the blocked transition (i.e. they block each other;
					*	only resolve one corner, as it will alter the graph and invalidate the indices) */
					if (firstDualBlock) {
						detail::PrintGrid(pGrid);
						fInsertDualCornerBlock(xCorner, yFirstCorner, leftwards);
						detail::PrintGrid(pGrid);
						return true;
					}
					if (lastDualBlock && yFirstCorner != yLastCorner) {
						detail::PrintGrid(pGrid);
						fInsertDualCornerBlock(xCorner, yLastCorner, leftwards);
						detail::PrintGrid(pGrid);
						return true;
					}

					/* check if either one of the two blocking corners can be rotated outside of the current transition */
					if (xCorner != x && fInsertSingleCornerBlock(xOther, y, xCorner, yFirstCorner))
						return true;
					if (xCorner != xEnd && yFirstCorner != yLastCorner && fInsertSingleCornerBlock(xOther, y, xCorner, yLastCorner))
						return true;
				}
			}
			return false;
		}
		bool fShiftNodes(size_t complexity) const {
			intptr_t bestScore = -1, bestRow = 0, bestEmpty = 0;
			bool bestLeftwards = false;

			static std::set<std::pair<uint64_t, size_t>> checked;
			uint64_t hash = fMeasureHash();

			/* find the next node and check if it should be shifted (iterate until one has been found or all have been checked;
			*	Important: two rows back-to-back must not both contain nodes, there must always exist an intermediate row) */
			for (size_t y = 0; y < pGrid.size(); ++y) {
				detail::Row& row = pGrid[y];

				if (checked.contains({ hash, y }))
					continue;

				/* check if the row contains a node */
				size_t first = std::ranges::find(row, detail::CellKind::node, &detail::Cell::kind) - row.begin(), last = 0;
				if (first >= row.size())
					continue;

				/* count the preferences of all transitions going out of the node and find the end of the current node */
				intptr_t leftScore = 0, rightScore = 0;
				for (size_t i = first; (i < row.size() && row[i].kind == detail::CellKind::node); ++i) {
					last = i;
					leftScore += fShiftCheckPreference(y, i, false, true) + fShiftCheckPreference(y, i, true, true);
					rightScore += fShiftCheckPreference(y, i, false, false) + fShiftCheckPreference(y, i, true, false);
				}

				/* check if the neighboring spot is not empty, which will add to the penalty */
				for (size_t i = first; i > 0 && row[i - 1].kind != detail::CellKind::empty; --i)
					leftScore -= 2;
				for (size_t i = last + 1; i < row.size() && row[i].kind != detail::CellKind::empty; ++i)
					rightScore -= 2;

				/* check if any of the scores is better than the current best score */
				if (leftScore > bestScore)
					bestScore = leftScore, bestRow = y, bestEmpty = last, bestLeftwards = true;
				if (rightScore > bestScore)
					bestScore = rightScore, bestRow = y, bestEmpty = first, bestLeftwards = false;
			}

			/* check if a best match has been found to be applied */
			if (bestScore <= static_cast<intptr_t>(complexity))
				return false;

			/* move the node over by ensuring that the spot next to it becomes empty */
			checked.insert({ hash, bestRow });
			fCreateEmptyCell(bestEmpty, bestRow, bestLeftwards);
			return true;
		}

	private:
		void fShrinkHorizontal() const {
			if (pGrid.empty())
				return;
			std::vector<bool> empty(pGrid[0].size(), true);

			/* iterate over all cells and mark all non-removable columns as not empty */
			for (size_t y = 0; y < pGrid.size(); ++y) {
				for (size_t x = 0; x < pGrid[y].size(); ++x) {
					if (pGrid[y][x].kind != detail::CellKind::empty && pGrid[y][x].kind != detail::CellKind::horizontal)
						empty[x] = false;
				}
			}

			/* reduce the grid by removing all empty columns */
			size_t index = 0;
			for (size_t x = 0; x < empty.size(); ++x) {
				if (empty[x])
					continue;

				/* shift all values down in the grid */
				if (index != x) for (size_t y = 0; y < pGrid.size(); ++y)
					pGrid[y][index] = pGrid[y][x];
				++index;
			}

			/* resize all of the rows */
			if (index < empty.size()) for (detail::Row& row : pGrid)
				row.resize(index);
		}
		void fShrinkVertical() const {
			size_t sizeReduced = 0, afterNode = 0;
			bool lastRemovable = false;

			/* iterate over the rows and remove all removable rows, while ensuring
			*	that there is at least one row between two neighboring nodes */
			for (size_t i = 0; i < pGrid.size(); ++i) {
				bool removable = true, node = false;

				/* check if the current row either is a node or fully removable */
				for (const detail::Cell& cell : pGrid[i]) {
					if (cell.kind != detail::CellKind::empty && cell.kind != detail::CellKind::vertical)
						removable = false;
					if (cell.kind == detail::CellKind::node)
						node = true;
				}

				/* check if the last row was optional and can be removed */
				if (lastRemovable && !node)
					--sizeReduced;

				/* check if the row can immediately be removed */
				if (removable && afterNode < sizeReduced) {
					lastRemovable = false;
					continue;
				}
				lastRemovable = removable;

				/* add the row to the ouptut */
				if (sizeReduced != i)
					pGrid[sizeReduced] = std::move(pGrid[i]);
				++sizeReduced;
				if (node)
					afterNode = sizeReduced;
			}

			/* check if the actual grid can be reduced */
			if (sizeReduced < pGrid.size())
				pGrid.erase(pGrid.begin() + sizeReduced, pGrid.end());
		}

	public:
		std::vector<detail::Row> build() const {
			fConstructGrid();
			detail::PrintGrid(pGrid);

			const auto straightenGraph = [this]() -> void {
				/* first straighten across the graph and then finally outwards */
				for (auto align : { Alignment::leftwards, Alignment::rightwards, Alignment::outwards }) {
					while (fStraightenGraph(align));
					fShrinkVertical();
					fShrinkHorizontal();
				}
			};
			const auto fixInterlocks = [this](bool leftwards) -> bool {
				bool dirty = false;
				while (fFixInterlocks(leftwards)) {
					dirty = true;
					while (fStraightenGraph(leftwards ? Alignment::leftwards : Alignment::rightwards));
					fShrinkVertical();
					fShrinkHorizontal();
				}
				detail::PrintGrid(pGrid);
				return dirty;
			};

			for (size_t i = 0; i < 2; ++i) {
				for (size_t j = 0; j < 2; ++j) {
					while (fixInterlocks(true));
					while (fixInterlocks(false));
					straightenGraph();
				}
				detail::PrintGrid(pGrid);
				int k = 0;

				while (fShiftNodes(true, i / 3)) {
					straightenGraph();
					detail::PrintGrid(pGrid);
					int k = 0;
				}
				while (fShiftNodes(false, i / 3)) {
					straightenGraph();
					detail::PrintGrid(pGrid);
					int k = 0;
				}
			}

			if (false) for (size_t i = 0;; ++i) {
				size_t tCount = 0;
				for (bool dirty = false;; dirty = false) {
					for (size_t dir = 0; dir < 2; ++dir) {
						bool leftOnly = (dir == 0);

						while (true) {
							while (fStraightenGraph(leftOnly ? Alignment::leftwards : Alignment::rightwards)) {
								dirty = true;
								fShrinkVertical();
								detail::PrintGrid(pGrid);
								int k = 0;
							}
							if (fShiftNodes(0)) {
								dirty = true;
								fShrinkVertical();
								detail::PrintGrid(pGrid);
								continue;
							}
							break;
						}


					}


					fShrinkVertical();
					detail::PrintGrid(pGrid);
					if (!dirty || ++tCount >= 20)
						break;
				}

				if (i >= 40)
					break;

				detail::PrintGrid(pGrid);

				/* shifting nodes requires intermediate rows between any two neighboring nodes */
				//if (!fShiftNodes())
				//	break;
				//detail::PrintGrid(pGrid);
			}

			detail::PrintGrid(pGrid);


			std::vector<detail::Row> grid;
			std::swap(pGrid, grid);
			return grid;
		}
	};
}
