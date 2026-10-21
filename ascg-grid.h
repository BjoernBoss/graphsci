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
		bool isEmpty() const {
			return (kind == detail::CellKind::empty);
		}
		bool isNode() const {
			return (kind == detail::CellKind::node);
		}
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
		bool anyRight() const {
			return (cornerRight() || lineHorizontal());
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
		bool simpleVertical() const {
			return (kind == detail::CellKind::empty || kind == detail::CellKind::vertical);
		}
		bool simpleHorizontal() const {
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
		struct GridScore {
			size_t height = 0;
			size_t width = 0;
			size_t crossings = 0;
			size_t corners = 0;
		};
		struct NodePoint {
			size_t index = 0;
			size_t x = 0;
			bool operator<(const NodePoint& r) const {
				if (index != r.index)
					return (index < r.index);
				return (x < r.x);
			}
		};
		struct GridPoint {
			size_t x = 0;
			size_t y = 0;
		};

	private:
		const std::vector<detail::Layer>& pLayers;
		mutable std::vector<detail::Row> pGrid;

	public:
		GridBuilder(const std::vector<detail::Layer>& layers) : pLayers{ layers } {}

	private:
		GridScore fMeasureGrid() const {
			GridScore score{ .height = pGrid.size(), .width = pGrid[0].size(), .crossings = 0, .corners = 0 };

			/* count the number of corners in the grid */
			for (const detail::Row& row : pGrid) {
				for (const detail::Cell& cell : row) {
					score.crossings += (cell.kind == detail::CellKind::crossing ? 1 : 0);
					score.corners += (cell.anyCorner() ? 1 : 0);
				}
			}
			return score;
		}
		bool fApplyBestGrid(std::vector<detail::Row>& bestGrid, GridScore& bestScore) const {
			GridScore score = fMeasureGrid();

			/* compare the grid score to determine if a change has occurred */
			intptr_t cmp = 0;
			if (score.height != bestScore.height)
				cmp = (bestScore.height - score.height);
			else if (score.width != bestScore.width)
				cmp = (bestScore.width - score.width);
			else if (score.crossings != bestScore.crossings)
				cmp = (bestScore.crossings - score.crossings);
			else if (score.corners != bestScore.corners)
				cmp = (bestScore.corners - score.corners);

			/* check if a new best graph has been encountered and apply it and notify the caller */
			if (cmp > 0)
				bestGrid = pGrid, bestScore = score;
			else if (cmp < 0)
				pGrid = bestGrid;
			return (cmp > 0);
		}

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
			if (pGrid[y][x].isEmpty())
				return{ x, y };

			/* check if the entire grid needs to be widened to accomodate the shifting of the affected
			*	cells (to ensure all upcoming operations can perform the shifts without checking bounds) */
			auto shiftIntoRange = std::ranges::subrange(pGrid[y].begin() + (leftwards ? 0 : x + 1), (leftwards ? pGrid[y].begin() + x : pGrid[y].end()));
			if (!std::ranges::any_of(shiftIntoRange, [](auto v) { return (v == detail::CellKind::empty); }, &detail::Cell::kind))
				fInsertEmptyColumn(leftwards, x);

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
					else if (i <= x && cur[i - 1].isEmpty())
						break;
				}
				else for (size_t i = x; i < cur.size(); ++i) {
					if (slice[i] != detail::SlotUnused)
						++slice[i];
					else if (i > x && cur[i].isEmpty())
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
					if (current == detail::SlotUnused && i <= x && cur[i - 1].isEmpty())
						break;
				}
				else for (size_t i = x; i < cur.size(); ++i) {
					std::swap(current, slice[i]);
					if (current == detail::SlotUnused && i > x && cur[i].isEmpty())
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
				if (temp.isEmpty())
					break;
			}
			else for (size_t i = x; i < row.size(); ++i) {
				std::swap(temp, row[i]);
				if (temp.isEmpty())
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
		void fInsertEmptyColumn(bool leftSide, auto&... values) const {
			size_t where = (leftSide ? 0 : pGrid[0].size());

			/* insert the new empty cells */
			for (size_t i = 0; i < pGrid.size(); ++i)
				pGrid[i].insert(pGrid[i].begin() + where, detail::Cell{});

			/* apply the shift to all values */
			if (leftSide)
				((values += 1), ...);
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
			row.reserve(pGrid[0].size());
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
			if (numberOfRows == pGrid.size()) {
				detail::Row& row = pGrid.emplace_back();
				row.reserve(pGrid[0].size());
				fFillTraversals(row, slice, 0, slice.size());
			}
		}
		void fConstructGrid() const {
			pGrid.clear();

			/* compute the minimum width to give to the graph */
			size_t totalWidth = 0;
			for (const detail::Layer& layer : pLayers) {
				size_t width = layer.points.size();

				/* find the node in the points and update the total width accordingly and patch teh total width */
				for (size_t i = 0; i < layer.points.size(); ++i) {
					if (const detail::Node* node = layer.points[i].isNode(); node != nullptr)
						width += std::max<size_t>(node->start.size(), node->end.size()) - 1;
				}
				totalWidth = std::max<size_t>(totalWidth, width);
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
			bool endIsNode = pGrid[yOther][xOther].isNode();
			xEnd = xOther;
			if (endIsNode) for (size_t i = xEnd; xEnd != x && pGrid[yOther][i].isNode(); i = detail::Towards(i, x))
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
					valid = pGrid[yOther][target].isEmpty();

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
		void fFixDualCornerBlock(size_t xCorner, size_t yCorner, bool leftwards) const {
			/* check if the index lies right at the bounds, in which case
			*	the grid needs to be extended to provide space for a detour */
			if (leftwards ? xCorner == 0 : xCorner + 1 == pGrid[yCorner].size())
				fInsertEmptyColumn(leftwards, xCorner);

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
		bool fFixSingleCornerBlock(size_t xBase, size_t yBase, size_t xEnd, size_t yOther, size_t xBlock, size_t yBlock) const {
			size_t xSpacer = detail::AwayFrom(xBlock, xBase);
			bool baseLower = (yBase < yBlock), baseLeft = (xBase < xBlock);

			/* ensure that the base is not a node and check if the blocking corner points into the base row */
			if (pGrid[yBase][xBase].isNode())
				return false;
			if (baseLower ? !pGrid[yBlock][xBlock].cornerTop() : !pGrid[yBlock][xBlock].cornerBottom())
				return false;
			if (!detail::Inbetween(xBlock, xBase, xEnd) || xBlock == xBase)
				return false;

			/* ensure that the path between the potential spacer and the blocking
			*	transition is free, allowing the blocking transition to be moved around */
			for (size_t i : detail::MinMaxWalk{ yBase, yBlock, true }) {
				if (!pGrid[i][xSpacer].freeVertical())
					return false;
			}

			/* check if the base row can be crossed immediately */
			if (pGrid[yBase][xSpacer].freeVertical()) {
				/* insert the empty rows around the base row */
				fInsertNullRow(yBase++);
				fInsertNullRow(yBase + 1);
				if (baseLower)
					yBlock += 2;

				/* insert the shift on the outside of the base row */
				size_t yOutside = detail::AwayFrom(yBase, yBlock);
				pGrid[yOutside][xBlock].kind = pGrid[yBlock][xBlock].kind;
				pGrid[yOutside][xSpacer].kind = pGrid[yBlock][xBlock].mirrorFull();

				/* shift the crossing itself over */
				pGrid[yBase][xBlock].kind = detail::CellKind::horizontal;
				pGrid[yBase][xSpacer].kind = detail::CellKind::crossing;

				/* insert the shift on the inside of the base row */
				size_t yInside = detail::Towards(yBase, yBlock);
				pGrid[yInside][xBlock].kind = pGrid[yOutside][xBlock].mirrorHorizontal();
				pGrid[yInside][xSpacer].kind = pGrid[yOutside][xSpacer].mirrorHorizontal();
				return true;
			}

			/* ensure that the blocking cell is another corner of the base transition
			*	and that the base transition has a clear favour into the opposite direction */
			if (pGrid[yBase][xBase].mirrorFull() != pGrid[yBase][xSpacer].kind)
				return false;
			if (pGrid[yOther][xBase].kind != pGrid[yBlock][xBlock].kind)
				return false;

			/* check if an empty column needs to be inserted */
			if (xSpacer == 0 || xSpacer + 1 == pGrid[yBase].size())
				fInsertEmptyColumn(xSpacer == 0, xSpacer, xBlock, xBase);
			xSpacer = detail::AwayFrom(xSpacer, xBase);

			/* fetch the two corner kinds to be inserted into the detour */
			detail::CellKind kindDetour = pGrid[yOther][xBase].mirrorVertical();
			detail::CellKind kindCrossing = pGrid[yBase][xBase].mirrorVertical();

			/* check if the given cell is already empty or if an empty cell needs to be inserted */
			size_t ySpacer = yBase;
			if (!pGrid[yBase][xSpacer].isEmpty()) {
				auto values = fCreateEmptyCell(xSpacer, yBase, (xSpacer < xBase));
				xSpacer = values.first, ySpacer = values.second;
			}

			/* insert the empty rows around the base row and extract the y-coordinates for patching */
			fInsertNullRow(ySpacer++);
			fInsertNullRow(ySpacer + 1);
			size_t yCrossing = (baseLower ? ySpacer - 1 : ySpacer + 1), yDetour = (baseLower ? ySpacer + 1 : ySpacer - 1);
			size_t limit = (baseLeft ? 0 : pGrid[ySpacer].size());

			/* insert the new detour in the column with the newly created space */
			pGrid[yCrossing][xSpacer].kind = kindCrossing;
			pGrid[ySpacer][xSpacer].kind = pGrid[ySpacer][xSpacer].addVertical();
			pGrid[yDetour][xSpacer].kind = kindDetour;

			/* insert the intermediate column */
			xSpacer = detail::Towards(xSpacer, limit);
			pGrid[yCrossing][xSpacer].kind = pGrid[yCrossing][xSpacer].addHorizontal();
			pGrid[yDetour][xSpacer].kind = pGrid[yCrossing][xSpacer].addHorizontal();

			/* insert the final column, which connects the detour to the base row */
			xSpacer = detail::Towards(xSpacer, limit);
			pGrid[yCrossing][xSpacer].kind = detail::Cell{ kindCrossing }.mirrorFull();
			pGrid[ySpacer][xSpacer].kind = pGrid[ySpacer][xSpacer].removeVertical();
			pGrid[yDetour][xSpacer].kind = detail::Cell{ kindDetour }.mirrorFull();
			return true;
		}
		bool fStraightenGraph(Alignment align) const {
			bool changed = false;

			/* find the next corner to pursue (iterate until all possible corners
			*	have been found and return a change to indicate another sweep) */
			for (size_t y = 0; y < pGrid.size(); ++y) {
				detail::Row& row = pGrid[y];

				/* check if the width of the row needs to be determined (to find its center) */
				size_t halfRow = row.size();
				if (align == Alignment::outwards) {
					while (halfRow > 0 && row[halfRow - 1].isEmpty())
						--halfRow;
					if (halfRow > 0) for (size_t i = 0; row[i].isEmpty(); ++i)
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
					bool endIsNode = pGrid[yOther][xOther].isNode();

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
					changed = true;
				}
			}
			return changed;
		}
		bool fFixInterlocks(bool leftwards, GridPoint& point) const {
			/* find the next corner to pursue (iterate until one has been found or all have been checked) */
			for (; point.y < pGrid.size(); point.x = 0, ++point.y) {
				detail::Row& row = pGrid[point.y];
				for (; point.x < row.size(); ++point.x) {
					detail::Cell& cell = row[point.x];

					/* check if the corner is a potential indented corner for this iteration (must have a corner
					*	in the opposite direction in order to potentially want a change in the intended direction) */
					if (!(leftwards ? cell.cornerRight() : cell.cornerLeft()))
						continue;

					/* resolve the indentation and check if the current indentation is blocked by
					*	another indentation into the opposite direction and try to resolve them */
					size_t xOther = point.x, yOther = point.y, xEnd = 0, target = 0;
					if (!fMapIndentCorner(point.x, point.y, xOther, yOther, xEnd, target) || target != xOther)
						continue;

					/* check if there are any nodes in the way, in which case the interlocking cannot be removed
					*	and otherwise look for the blocking corners closest to base and the other corner */
					size_t xCorner = detail::Towards(xOther, point.x), yBaseCorner = pGrid.size(), yOtherCorner = pGrid.size();
					bool blockedTransition = false;
					for (size_t i : detail::MinMaxWalk{ point.y, yOther, true }) {
						if (pGrid[i][xCorner].isNode()) {
							blockedTransition = true;
							break;
						}
						if (!pGrid[i][xCorner].anyCorner())
							continue;
						if (yBaseCorner == pGrid.size())
							yBaseCorner = i;
						yOtherCorner = i;
					}
					if (blockedTransition)
						continue;

					/* check if no corner has been found, which can only be the case if the blocker spans the
					*	entire range of the original transition (might result in reduction of crossings) */
					if (yBaseCorner == pGrid.size()) {
						/* check if both end in the same node, in which case the transitions can be swapped */
						if (pGrid[yOther][xCorner].isNode() && pGrid[yOther][xOther].isNode()) {
							fInsertDualSwap(point.y, yOther, xOther, xCorner);
							return true;
						}

						/* check if the blocked transition crosses the blocking transition again as nothing can otherwise be done */
						if (pGrid[yOther][xCorner].kind != detail::CellKind::crossing)
							continue;

						/* the two transitions are crossing but lay in parallel next to each other, in which case they can just be
						*	swapped (first the higher positions, as the swap inserts new rows and may otherwise skew the other y position) */
						size_t yPrev = std::min<size_t>(point.y, yOther), yNext = std::max<size_t>(point.y, yOther);
						fInsertDualSwap(yNext, yPrev, xOther, xCorner);
						fInsertDualSwap(yPrev, yNext, xOther, xCorner);
						return true;
					}

					/* check if the first/last corner point into or out of the blocking
					*	transition (i.e. blocking each other or only one blocked by the other) */
					bool multiple = (yBaseCorner != yOtherCorner);
					if (multiple && point.y > yOther)
						std::swap(yBaseCorner, yOtherCorner);
					bool baseDualBlock = (leftwards ? pGrid[yBaseCorner][xCorner].cornerRight() : pGrid[yBaseCorner][xCorner].cornerLeft());
					bool otherDualBlock = multiple && (leftwards ? pGrid[yOtherCorner][xCorner].cornerRight() : pGrid[yOtherCorner][xCorner].cornerLeft());

					/* check if a corner points into the blocked transition (i.e. they block each other;
					*	only resolve one corner, as it will alter the graph and invalidate the indices) */
					if (baseDualBlock || otherDualBlock) {
						fFixDualCornerBlock(xCorner, (baseDualBlock ? yBaseCorner : yOtherCorner), leftwards);
						return true;
					}

					/* check if either one of the two blocking corners can be rotated outside of the current transition */
					if (fFixSingleCornerBlock(xOther, point.y, point.x, yOther, xCorner, yBaseCorner))
						return true;
					if (multiple && fFixSingleCornerBlock(xOther, yOther, xEnd, point.y, xCorner, yOtherCorner))
						return true;
				}
			}
			return false;
		}

	private:
		void fShrinkHorizontal() const {
			std::vector<bool> empty(pGrid[0].size(), true);

			/* iterate over all cells and mark all non-removable columns as not empty */
			for (size_t y = 0; y < pGrid.size(); ++y) {
				for (size_t x = 0; x < pGrid[y].size(); ++x) {
					if (!pGrid[y][x].simpleHorizontal())
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
					if (!cell.simpleVertical())
						removable = false;
					if (cell.isNode())
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
		bool fTestDisconnection(size_t x, size_t y) const {
			/* check if the column and the right neighbor are disconnected on the given row */
			return (!pGrid[y][x].anyRight() && (!pGrid[y][x].isNode() || x + 1 >= pGrid[y].size() || !pGrid[y][x + 1].isNode()));
		}
		std::pair<bool, size_t> fTestReductionSlot(std::vector<size_t>& strand, size_t x) const {
			/* check if the cell itself is necessary and cannot be removed */
			size_t y = strand[x];
			if (!pGrid[y][x].simpleVertical())
				return{ false, x };
			if (y > 0 && y + 1 < pGrid.size() && pGrid[y - 1][x].isNode() && pGrid[y + 1][x].isNode())
				return{ false, x };

			/* check if there exists any node conflicts within the reduction zone (i.e.
			*	reducing the column here would move two nodes into the same row) */
			for (size_t _x = x; _x > 0; --_x) {
				size_t o = strand[_x - 1];

				/* check if the current reduction is before the previous reduction, in which case any nodes on this
				*	column between the two reductions must not fall onto a row, in which there already is a node */
				if (y < o) for (size_t _y = y; _y < o; ++_y) {
					if (pGrid[_y][_x - 1].isNode() && pGrid[_y + 1][x].isNode())
						return{ false, x };
				}

				/* check if its the other way around */
				else if (y > o) for (size_t _y = o; _y < y; ++_y) {
					if (pGrid[_y + 1][_x - 1].isNode() && pGrid[_y][x].isNode())
						return{ false, _x - 1 };
				}
			}
			return{ true, 0 };
		}
		bool fFindRemovableStrand(std::vector<size_t>& strand) const {
			std::vector<std::pair<size_t, size_t>> ranges(pGrid[0].size() - 1, std::pair<size_t, size_t>{});
			bool endReached = false;

			/* iterate over the entire width and find all overlapping disconnected
			*	ranges and check if a reducible strand can be layed into it */
			for (size_t x = 0;;) {
				size_t prevEnd = (x == 0 ? pGrid.size() : ranges[x - 1].second);

				/* check if this is a re-visit due to no valid range being found in the upcoming
				*	neighbor in which case the previous range can just be used to continue */
				size_t begin = 0;
				if (endReached)
					begin = ranges[x].second;

				/* find the beginning of the non-overlapping range to the neighbor */
				else {
					begin = (x == 0 ? 0 : ranges[x - 1].first);
					if (fTestDisconnection(x, begin)) while (begin > 0 && fTestDisconnection(x, begin - 1))
						--begin;
				}

				/* skip any intermediate connections */
				while (begin < prevEnd && !fTestDisconnection(x, begin))
					++begin;

				/* check if the end has been reached, in which case the index needs to
				*	be reverted and the next range of the previous neighbor checked */
				if (endReached = (begin >= prevEnd)) {
					if (x == 0)
						return false;
					--x;
					continue;
				}

				/* find the end of the current range and set it up */
				size_t end = begin + 1;
				while (end < pGrid.size() && fTestDisconnection(x, end))
					++end;
				ranges[x] = { begin, end };

				/* visit the next neighbor */
				if (++x < ranges.size())
					continue;

				/* setup the current strand */
				for (size_t i = 0; i < strand.size(); ++i)
					strand[i] = std::max<size_t>((i == 0 ? 0 : ranges[i - 1].first), (i >= ranges.size() ? 0 : ranges[i].first));

				/* iterate over the strand and push it down until a valid combination has been found */
				x = 0;
				while (x < strand.size()) {
					/* check if the given strand position is valid */
					const auto [valid, badIndex] = fTestReductionSlot(strand, x);
					x = (valid ? x + 1 : badIndex);
					if (valid)
						continue;

					/* check if the strand still remains valid and otherwise update x to point to the failed range */
					size_t rPrev = (x == 0 ? pGrid.size() : ranges[x - 1].second), rNext = (x >= ranges.size() ? pGrid.size() : ranges[x].second);
					if (++strand[x] < std::min<size_t>(rPrev, rNext))
						continue;
					if (rPrev < rNext || x >= ranges.size())
						--x;
					break;
				}

				/* check if the entire strand has been validated and is therefore reducible
				*	and otherwise reset the ranges index to skip the failed range */
				if (x == strand.size())
					return true;
				endReached = true;
			}
		}
		void fCompressGrid() const {
			/* perform a full horizontal shrinking to remove any unnecessary horizontals (no need for in-depth
			*	merging and removing, as the straightening's and interlock-fixings already do so implicitly) */
			fShrinkHorizontal();

			/* check if the grid is only a strip, in which case the simple shrinking will already
			*	have been enough (will otherwise lead to edge-cases in the strand search) */
			if (pGrid[0].size() == 1)
				return;

			/* find all possible reduction strands and remove them from the grid */
			std::vector<size_t> reduction(pGrid[0].size(), 0);
			while (fFindRemovableStrand(reduction)) {
				/* find the last row, which will be the removed row */
				size_t drop = 0;
				for (size_t row : reduction)
					drop = std::max<size_t>(drop, row);

				/* remove the corresponding column in each row and shift it down
				*	until the row to be dropped and remove the row itself */
				for (size_t x = 0; x < reduction.size(); ++x) {
					for (size_t i = reduction[x]; i < drop; ++i)
						pGrid[i][x] = pGrid[i + 1][x];
				}
				pGrid.erase(pGrid.begin() + drop);
			}
		}

	private:
		int8_t fCheckShiftPreference(size_t x, size_t y, bool forwards) const {
			size_t last = (forwards ? pGrid.size() - 1 : 0);

			/* check if the transition actually exists and points towards the node */
			if (y == last)
				return 0;
			y = detail::Towards(y, last);
			if (forwards ? !pGrid[y][x].anyTop() : !pGrid[y][x].anyBottom())
				return 0;

			/* follow the strand until the first corner or other end-block is reached */
			while (pGrid[y][x].lineVertical()) {
				detail::Row& row = pGrid[y];
				y = detail::Towards(y, last);
			}

			/* evaluate the final pull direction */
			if (pGrid[y][x].cornerLeft())
				return -1;
			if (pGrid[y][x].cornerRight())
				return 1;
			return 0;
		}
		bool fShiftNode(std::map<NodePoint, intptr_t>& open, std::set<NodePoint>& closed, std::optional<NodePoint>& current, bool fastNodeShift) const {
			/* node placement is identified by its index and first cell position, as for each grid, the
			*	rows may be slightly shifted, but the overall starting horizontal position can be expected
			*	to be consistent, as any shifts in it are assumed to also invalidate the grid score */
			std::vector<size_t> nodeMap;

			/* iterate over the nodes and find all nodes, which can potentially be moved and select one to be shifted */
			for (size_t y = 0; y < pGrid.size(); ++y) {
				detail::Row& row = pGrid[y];

				/* check if the row contains a node */
				size_t first = std::ranges::find(row, detail::CellKind::node, &detail::Cell::kind) - row.begin(), last = 0;
				if (first >= row.size())
					continue;
				for (size_t i = first; i < row.size() && row[i].isNode(); ++i)
					last = i;
				NodePoint leftPoint{ .index = nodeMap.size(), .x = first - 1 }, rightPoint{ .index = nodeMap.size(), .x = first + 1 };
				nodeMap.push_back(y);

				/* check if the left or right direction can potentially be pursued and is not already queued for it */
				bool leftCheck = (first > 0 && !closed.contains(leftPoint) && !open.contains(leftPoint));
				bool rightCheck = (last + 1 < row.size() && !closed.contains(rightPoint) && !open.contains(rightPoint));
				if (!leftCheck && !rightCheck)
					continue;

				/* count the number of of potentially benefitting connections in each direction */
				intptr_t leftScore = 0, rightScore = 0, emptyScore = 0;
				for (size_t i = first; i <= last; ++i) {
					if (int8_t shift = fCheckShiftPreference(i, y, true); shift == 0)
						++emptyScore;
					else
						++(shift < 0 ? leftScore : rightScore);

					if (int8_t shift = fCheckShiftPreference(i, y, false); shift == 0)
						++emptyScore;
					else
						++(shift < 0 ? leftScore : rightScore);
				}

				/* insert the potential moves into the open set and update the scores accordingly (in fast
				*	mode, consider if the neighbor is empty, in which case fewer moves might be pursued) */
				if (leftCheck && leftScore > 0) {
					intptr_t spaceScore = 0;
					if (fastNodeShift)
						spaceScore = static_cast<intptr_t>(row[first - 1].isEmpty() ? pGrid.size() * 2 : 0);
					open.insert({ leftPoint, leftScore - rightScore - emptyScore + spaceScore });
				}
				if (rightCheck && rightScore > 0) {
					intptr_t spaceScore = 0;
					if (fastNodeShift)
						spaceScore = static_cast<intptr_t>(row[last + 1].isEmpty() ? pGrid.size() * 2 : 0);
					open.insert({ rightPoint, rightScore - leftScore - emptyScore + spaceScore });
				}
			}

			/* helper to evaluate a given nodepoint */
			const auto evaluatePoint = [this, &nodeMap](const NodePoint& pt, size_t& distance, size_t& emptySlot, bool& leftwards) -> bool {
				detail::Row& row = pGrid[nodeMap[pt.index]];

				/* check if the node position has already been reached */
				size_t first = std::ranges::find(row, detail::CellKind::node, &detail::Cell::kind) - row.begin(), last = 0;
				if (pt.x == first)
					return false;
				for (size_t i = first; i < row.size() && row[i].isNode(); ++i)
					last = i;
				leftwards = (pt.x < first);

				/* compute the points distance to its target and the next spot to mark as empty to move into the given direction */
				distance = (leftwards ? first - pt.x : pt.x - first);
				emptySlot = (leftwards ? last : first);
				return true;
			};

			/* check if a previous far away goal still exists and check if its still a
			*	relevant target, in which case it automatically becomes the best choice) */
			size_t bestDistance = 0, bestEmptySlot = 0;
			bool bestLeftwards = false;
			auto best = open.end();
			if (current.has_value()) {
				if (evaluatePoint(*current, bestDistance, bestEmptySlot, bestLeftwards))
					best = open.find(*current);
				else
					open.erase(*current);
			}

			/* if no target to follow exists, find the next entry in the open list to apply */
			if (best == open.end()) for (auto it = open.begin(); it != open.end();) {
				/* check if the entry has already been visited */
				if (closed.contains(it->first)) {
					it = open.erase(it);
					continue;
				}

				/* evaluate the point and check if the move is better than the current best
				*	option, as it is closer to the current grid-state or has a better score */
				size_t distance = 0, emptySlot = 0;
				bool leftwards = false;
				if (!evaluatePoint(it->first, distance, emptySlot, leftwards))
					it = open.erase(it);
				else if (best == open.end() || bestDistance > distance || best->second < it->second)
					best = it++, bestDistance = distance, bestEmptySlot = emptySlot, bestLeftwards = leftwards;
				else
					++it;
			}

			/* check if no next move has been found in which case the end has been reached */
			current = std::nullopt;
			if (best == open.end())
				return false;

			/* move the node over by ensuring that the spot next to it becomes empty and update the iterative state */
			fCreateEmptyCell(bestEmptySlot, nodeMap[best->first.index], bestLeftwards);
			closed.insert(best->first);
			if (bestDistance == 1)
				open.erase(best);
			else
				current = best->first;
			return true;
		}

	private:
		void fBuildStraighten(bool leftFirst) const {
			Alignment first = (leftFirst ? Alignment::leftwards : Alignment::rightwards);
			Alignment second = (leftFirst ? Alignment::rightwards : Alignment::leftwards);

			/* first straighten across the graph from each direction and then
			*	finally outwards and shrink the graph to remove unnecessary rows */
			for (auto align : { first, second, Alignment::outwards }) {
				while (fStraightenGraph(align));
				fShrinkVertical();
			}
		}
		void fBuildFixInterlocks() const {
			/* collect the current best grid state as basis for interlock fixing and
			*	cache the best state to be able to recover it on worse performances */
			std::vector<detail::Row> bestGrid = pGrid;
			GridScore bestScore = fMeasureGrid();

			/* iterate until no more interlock fixings reduce the overall grid dimensions */
			for (size_t i = 0;; ++i) {
				bool dirty = false, leftwards = (i % 2 == 0);

				/* forward-iterate the interlock fixing into the given
				*	direction until one results in a positive outcome */
				GridPoint point{};
				while (fFixInterlocks(leftwards, point)) {
					/* first straighten the graph into the given direction, in order for any fix-changes
					*	to propagate properly, and then perform a general straightening and shrinking */
					fBuildStraighten(leftwards);

					/* advance the point and check if a new best grid has been reached and reset the state accordingly */
					++point.x;
					if (fApplyBestGrid(bestGrid, bestScore))
						point = { .x = 0, .y = 0 }, dirty = true;
				}

				/* check if the fixing results in changes, in which
				*	case fixes into the other direction can be repeated */
				if (i > 0 && !dirty)
					break;
			}

			/* reset the grid to the best state (to capture minor best scores as well) */
			std::swap(pGrid, bestGrid);
		}

	public:
		std::vector<detail::Row> build(std::optional<size_t> iterations, bool fastNodeShift) const {
			/* check if the layers are empty, in which case nothing needs to be done, and
			*	otherwise create the initial grid and straighten it (cannot be empty anymore) */
			if (pLayers.empty())
				return {};
			fConstructGrid();
			fBuildStraighten(true);

			/* setup the currently best grid version (to be able to recover to it in case of worse visited graphs) */
			std::vector<detail::Row> bestGrid = pGrid;
			GridScore bestScore = fMeasureGrid();

			/* iterate until no more nodes need to be shifted or the iterations limit has been reached */
			std::map<NodePoint, intptr_t> nodeOpen;
			std::set<NodePoint> nodeClosed;
			std::optional<NodePoint> nodeCurrent;
			while (true) {
				/* fix any encountered interlocks (will automatically straighten the graph) and perform a full
				*	compression to ensure that the temporary representations do not block true better results */
				fBuildFixInterlocks();
				fCompressGrid();

				/* check if a new best graph has been encountered and reset all corresponding state */
				if (!nodeCurrent.has_value() && fApplyBestGrid(bestGrid, bestScore))
					nodeOpen.clear(), nodeClosed.clear(), nodeCurrent = std::nullopt;

				/* check if another shift can be performed or if the limit has been reached */
				if (iterations.has_value() && (*iterations)-- == 0)
					break;

				/* try to shift another node into a better place, in which
				*	case the straightening and measuring can be repeated */
				if (!fShiftNode(nodeOpen, nodeClosed, nodeCurrent, fastNodeShift))
					break;
				fBuildStraighten(true);
			}

			/* return the complete generated grid (use the best state to capture minor best scores as well) */
			return bestGrid;
		}
	};
}
