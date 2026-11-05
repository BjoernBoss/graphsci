#pragma once

#include "ascg-common.h"
#include "ascg-layer.h"

namespace ascg::detail {
	static constexpr size_t InterlockWindow = 2;

	enum class CellKind : uint8_t {
		empty,
		node,
		crossing,
		horizontal,
		vertical,
		leftTop,
		leftBottom,
		rightTop,
		rightBottom,
		arrowTop,
		arrowBottom
	};
	struct Cell {
		detail::CellKind kind = detail::CellKind::empty;
		bool isEmpty() const {
			return (kind == detail::CellKind::empty);
		}
		bool isNode() const {
			return (kind == detail::CellKind::node);
		}
		bool isArrow() const {
			return (kind == detail::CellKind::arrowTop || kind == detail::CellKind::arrowBottom);
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
			return (kind == detail::CellKind::crossing || kind == detail::CellKind::vertical || isArrow());
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

	/* build a grid representation from the layers and optimizes it by its dimensions and visual complexity:
	*	1.	construct the grid row by row and straighten it by removing unnecessary dents and corners
	*	2.	repeat: fix interlocks by trial (apply a fix, straighten, keep the result if the score improves and otherwise revert
	*		it and remember the failed interlock), compress the grid, and shift nodes sideways to unblock further straightenings
	*	All loops are bounded: a straightening step strictly shortens a transition, the fix scan only restarts on strict
	*	score improvements, failed fixes are not retried, and every node shift is tried at most once per best grid */
	class GridBuilder {
	private:
		enum class Alignment : uint8_t {
			leftwards,
			rightwards,
			outwards
		};
		struct Slot {
			size_t target = 0;
			bool used = false;
			bool top = false;
			bool bottom = false;
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
		struct ShiftTarget {
			NodePoint point;
			size_t distance = 0;
		};
		struct GridPoint {
			size_t x = 0;
			size_t y = 0;
		};
		struct GridRect {
			size_t left = 0;
			size_t top = 0;
			size_t right = 0;
			size_t bottom = 0;
			bool empty() const {
				return (left == right && top == bottom);
			}
			bool overlap(const GridRect& o) const {
				return (left <= o.right && right >= o.left && top <= o.bottom && bottom >= o.top);
			}
			GridRect merge(const GridRect& o) const {
				return GridRect{
					.left = std::min<size_t>(left, o.left),
					.top = std::min<size_t>(top, o.top),
					.right = std::max<size_t>(right, o.right),
					.bottom = std::max<size_t>(bottom, o.bottom)
				};
			}
		};
		struct TrackedRect {
			GridRect rect;
			size_t age = 0;
		};
		struct StraightenCorner {
			TrackedRect tracked;
			size_t x = 0;
		};
		struct StraightenRow {
			TrackedRect tracked;
			std::vector<StraightenCorner> corners;
		};

	private:
		const std::vector<detail::Range>& pRanges;
		const std::vector<detail::Layer>& pLayers;
		std::vector<detail::Row> pGrid;
		std::unordered_set<size_t> pFailedFixes;
		std::vector<StraightenRow> pStraightenRows;
		std::vector<GridRect> pStraightenList;

	public:
		GridBuilder(const std::vector<detail::Range>& ranges, const std::vector<detail::Layer>& layers) : pRanges{ ranges }, pLayers{ layers } {}

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
		intptr_t fApplyBestGrid(std::vector<detail::Row>& bestGrid, GridScore& bestScore) {
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
			return cmp;
		}
		size_t fMeasureInterlock(size_t x, size_t y, size_t xOther, size_t yOther, bool leftwards) const {
			detail::Hasher hasher;

			/* compute the bounding box of the interlocked indentation including the corresponding overlap-window */
			size_t yLo = std::min<size_t>(y, yOther), yHi = std::min<size_t>(std::max<size_t>(y, yOther) + detail::InterlockWindow, pGrid.size() - 1);
			size_t xLo = std::min<size_t>(x, xOther), xHi = std::min<size_t>(std::max<size_t>(x, xOther) + detail::InterlockWindow, pGrid[0].size() - 1);
			yLo = (yLo > detail::InterlockWindow ? yLo - detail::InterlockWindow : 0);
			xLo = (xLo > detail::InterlockWindow ? xLo - detail::InterlockWindow : 0);

			/* hash the cells in the bounding box (including the direction) to
			*	identify the state again and prevent repeated interlock-fixing */
			hasher.add(leftwards ? 0x01 : 0x00);
			for (size_t i = yLo; i <= yHi; ++i) {
				for (size_t j = xLo; j <= xHi; ++j)
					hasher.add(static_cast<size_t>(pGrid[i][j].kind));
			}
			return hasher.digest();
		}

	private:
		void fFillTraversals(detail::Row& row, std::vector<Slot>& slice, size_t begin, size_t end) const {
			for (size_t i = begin; i < end; ++i)
				row.push_back(detail::Cell{ slice[i].used ? detail::CellKind::vertical : detail::CellKind::empty });
		}
		size_t fFlushArrows(size_t where, std::vector<Slot>& slice, bool top) {
			/* check if the slice contains any errors in the given direction and clear the flags
			*	(caller is responsible for ensuring that the node is the immediate neighbor) */
			bool hasArrows = false;
			for (size_t i = 0; i < slice.size() && !hasArrows; ++i)
				hasArrows = (slice[i].used && (top ? slice[i].top : slice[i].bottom));
			if (!hasArrows)
				return where;

			/* fetch the row to contain the arrow heads (and reserve memory for them) */
			detail::Row& row = *pGrid.emplace(pGrid.begin() + where++);
			row.reserve(slice.size());

			/* iterate over the slice and add the arrow heads */
			for (size_t i = 0; i < slice.size(); ++i) {
				if (!slice[i].used)
					row.push_back(detail::Cell{ detail::CellKind::empty });
				else if (top) {
					row.push_back(detail::Cell{ slice[i].top ? detail::CellKind::arrowTop : detail::CellKind::vertical });
					slice[i].top = false;
				}
				else {
					row.push_back(detail::Cell{ slice[i].bottom ? detail::CellKind::arrowBottom : detail::CellKind::vertical });
					slice[i].bottom = false;
				}
			}
			return where;
		}
		size_t fInsertRotation(std::vector<Slot>& slice, size_t begin, size_t end, size_t target, size_t where) {
			/* add the rotation of the range [begin, end) with begin finally being placed at target */
			if (begin == target) return where;
			bool leftwards = (target < begin);
			size_t distance = (leftwards ? (begin - target) : (target - begin));

			/* iterate until no more entries need to be moved */
			while (begin != end) {
				detail::Row& row = *pGrid.emplace(pGrid.begin() + where++);
				row.reserve(slice.size());

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
		size_t fInsertSliceCorrections(std::vector<Slot>& slice, size_t where) {
			/* flush all of the initial arrows */
			where = fFlushArrows(where, slice, true);

			/* find the next misplaced entry, which can be moved to its new placement (loop until they have all
			*	been moved; must always find one, as the caller guarantees that the slice targets are possible) */
			for (size_t i = 0; i < slice.size();) {
				if (!slice[i].used || slice[i].target == i) {
					++i;
					continue;
				}

				/* check if the entire range is free for the slice slot to be rotated into place */
				bool blocked = false;
				for (size_t j : detail::MinMaxWalk{ i, slice[i].target, false }) {
					if (j == i || !slice[j].used)
						continue;
					blocked = true;
					break;
				}

				/* apply the rotation, if its not blocked, and update the indices */
				if (blocked)
					++i;
				else {
					where = fInsertRotation(slice, i, i + 1, slice[i].target, where);
					i = 0;
				}
			}

			/* flush all of the bottom arrows */
			return fFlushArrows(where, slice, false);
		}
		void fInsertNullRow(size_t index) {
			detail::Row& row = *pGrid.emplace(pGrid.begin() + index);
			detail::Row& prev = pGrid[index - 1];
			detail::Row& next = pGrid[index + 1];
			row.reserve(prev.size());

			/* insert a null row at index, which only consists of vertical edges or empty space
			*	(arrow-heads potentially need to be moved from the neighbors into this null-row) */
			for (size_t i = 0; i < prev.size(); ++i) {
				if (prev[i].kind == detail::CellKind::arrowBottom) {
					row.push_back(detail::Cell{ prev[i].kind });
					prev[i].kind = detail::CellKind::vertical;
				}
				else if (next[i].kind == detail::CellKind::arrowTop) {
					row.push_back(detail::Cell{ next[i].kind });
					next[i].kind = detail::CellKind::vertical;
				}
				else
					row.push_back(detail::Cell{ (prev[i].anyBottom() || next[i].anyTop()) ? detail::CellKind::vertical : detail::CellKind::empty });
			}
		}
		void fInsertEmptyColumn(bool leftSide, auto&... values) {
			size_t where = (leftSide ? 0 : pGrid[0].size());

			/* insert the new empty cells */
			for (size_t i = 0; i < pGrid.size(); ++i)
				pGrid[i].insert(pGrid[i].begin() + where, detail::Cell{});

			/* apply the shift to all values */
			if (leftSide)
				((values += 1), ...);
		}
		std::pair<size_t, size_t> fCreateEmptyCell(size_t x, size_t y, bool leftwards) {
			if (pGrid[y][x].isEmpty())
				return{ x, y };

			/* check if the entire grid needs to be widened to accomodate the shifting of the affected
			*	cells (to ensure all upcoming operations can perform the shifts without checking bounds) */
			auto shiftIntoRange = std::ranges::subrange(pGrid[y].begin() + (leftwards ? 0 : x + 1), (leftwards ? pGrid[y].begin() + x : pGrid[y].end()));
			if (!std::ranges::any_of(shiftIntoRange, [](auto v) { return (v == detail::CellKind::empty); }, &detail::Cell::kind))
				fInsertEmptyColumn(leftwards, x);

			/* helper to relocate the arrow-heads between the shifted row and its neighbor */
			const auto relocateArrows = [](std::vector<Slot>& slice, detail::Row& upper, detail::Row& lower, bool columnIsTarget) {
				/* check if the slice will result in any changes and otherwise the arrows dont need to be updated */
				bool rotating = false;
				for (size_t i = 0; i < slice.size() && !rotating; ++i)
					rotating = (slice[i].used && slice[i].target != i);
				if (!rotating)
					return;

				/* remove the arrow head state from the grid and cache it into the slice, for it to be re-applied once finished */
				for (size_t i = 0; i < slice.size(); ++i) {
					if (!slice[i].used)
						continue;
					size_t column = (columnIsTarget ? slice[i].target : i);
					if (upper[column].kind == detail::CellKind::arrowBottom)
						slice[i].bottom = true, upper[column].kind = detail::CellKind::vertical;
					if (lower[column].kind == detail::CellKind::arrowTop)
						slice[i].top = true, lower[column].kind = detail::CellKind::vertical;
				}
			};

			/* setup the slice to move the previous entries (may invalidate references) */
			std::vector<Slot> slice(pGrid[y].size(), Slot{});
			if (y > 0) {
				detail::Row& prev = pGrid[y - 1];
				detail::Row& cur = pGrid[y];

				/* setup the initial identity mapping (look at both neighbors to track transitions coming out of nodes) */
				for (size_t i = 0; i < cur.size(); ++i)
					slice[i] = Slot{ .target = i, .used = (prev[i].anyBottom() || cur[i].anyTop()), .top = false, .bottom = false };

				/* iterate again and apply the shift to the slice such that they all target their new shifted position */
				if (leftwards) for (size_t i = x + 1; i > 0; --i) {
					if (slice[i - 1].used)
						--slice[i - 1].target;
					else if (i <= x && cur[i - 1].isEmpty())
						break;
				}
				else for (size_t i = x; i < cur.size(); ++i) {
					if (slice[i].used)
						++slice[i].target;
					else if (i > x && cur[i].isEmpty())
						break;
				}

				/* relocate the arrow-heads and insert the shifting */
				relocateArrows(slice, prev, cur, false);
				y = fInsertSliceCorrections(slice, y);
			}

			/* setup the slice to move the next entries (may invalidate references) */
			if (y + 1 < pGrid.size()) {
				detail::Row& cur = pGrid[y];
				detail::Row& next = pGrid[y + 1];

				/* setup the initial identity mapping (look at both neighbors to track transitions coming out of nodes) */
				for (size_t i = 0; i < cur.size(); ++i)
					slice[i] = Slot{ .target = i, .used = (cur[i].anyBottom() || next[i].anyTop()), .top = false, .bottom = false };

				/* iterate again and apply the shift to the slice such that they each start at their new shifted position */
				Slot current = {};
				if (leftwards) for (size_t i = x + 1; i > 0; --i) {
					std::swap(current, slice[i - 1]);
					if (!current.used && i <= x && cur[i - 1].isEmpty())
						break;
				}
				else for (size_t i = x; i < cur.size(); ++i) {
					std::swap(current, slice[i]);
					if (!current.used && i > x && cur[i].isEmpty())
						break;
				}

				/* relocate the arrow-heads and insert the shifting */
				relocateArrows(slice, cur, next, true);
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
		std::vector<Slot> fConstructNode(size_t index, size_t width, std::map<size_t, size_t>& mapping) {
			const detail::Layer& layer = pLayers[index];

			/* setup the initial slice and row */
			detail::Row& row = pGrid.emplace_back(width, detail::Cell{});
			std::vector<Slot> slice(width, Slot{});

			/* setup the mapping of the next node (necessary to populate the slice) */
			std::map<size_t, size_t> upcoming = (index + 1 >= pLayers.size() ? std::map<size_t, size_t>{} : fSetupNodeMap(index + 1, width));

			/* iterate over the nodes and write them out to the grid and update the slice simultaneously */
			for (const detail::Point& point : layer.points) {
				/* check if its a simple traversal */
				if (const detail::Traverse* traverse = point.isTraverse(); traverse != nullptr) {
					size_t where = mapping[traverse->range];
					row[where].kind = detail::CellKind::vertical;
					slice[where] = Slot{
						.target = upcoming[traverse->range],
						.used = true,
						.top = false,
						.bottom = (pRanges[traverse->range].headLast && pLayers[index + 1].points[traverse->next].isNode() != nullptr)
					};
					continue;
				}
				const detail::Node& node = point.node();

				/* insert all of the node points to the row (iterate over the larger of the nodes to ensure all cells are filled) */
				for (const detail::Endpoint& ep : (node.start.size() > node.end.size() ? node.start : node.end))
					row[mapping[ep.range]].kind = detail::CellKind::node;

				/* update the slice to contain the ranges starting here */
				for (const detail::Endpoint& ep : node.start) {
					slice[mapping[ep.range]] = Slot{
						.target = upcoming[ep.range],
						.used = true,
						.top = pRanges[ep.range].headFirst,
						.bottom = (pRanges[ep.range].headLast && pLayers[index + 1].points[ep.other].isNode() != nullptr)
					};
				}
			}

			/* replace the mapping with the mapping of the upcoming node and return the slice */
			mapping = std::move(upcoming);
			return slice;
		}
		void fConstructAddCrossing(std::vector<Slot>& slice, size_t index, size_t target) {
			detail::Row& row = pGrid.emplace_back();
			row.reserve(pGrid[0].size());
			bool leftwards = (target < index);

			/* add the traversals before the crossing */
			fFillTraversals(row, slice, 0, std::min<size_t>(index, target));

			/* add the crossing itself */
			row.push_back(detail::Cell{ leftwards ? detail::CellKind::rightBottom : detail::CellKind::rightTop });
			for (size_t i : detail::MinMaxWalk{ index, target, true })
				row.push_back(detail::Cell{ slice[i].used ? detail::CellKind::crossing : detail::CellKind::horizontal });
			row.push_back(detail::Cell{ leftwards ? detail::CellKind::leftTop : detail::CellKind::leftBottom });

			/* add the traversals after the crossing and update the slice itself */
			fFillTraversals(row, slice, std::max<size_t>(index, target) + 1, slice.size());
			std::swap(slice[index], slice[target]);
		}
		void fConstructTransitions(std::vector<Slot>& slice) {
			size_t numberOfRows = pGrid.size();

			/* flush all of the initial arrows (before any crossings are inserted or applied) */
			fFlushArrows(pGrid.size(), slice, true);

			/* add all rotations and swaps to ensure the slice is in order */
			while (true) {
				size_t bestIndex = 0, bestSkip = 0, bestTarget = 0;
				bool bestCanMove = false, bestMoveLeft = false;

				/* find the next entry which can already move or which has to move the furthest */
				for (size_t i = 0; i < slice.size(); ++i) {
					if (!slice[i].used)
						continue;
					size_t leftSkip = 0, leftTarget = 0, rightSkip = 0, rightTarget = 0;

					/* count the number of connections to skip to move it in order
					*	and check if the entry is already positioned correctly */
					for (size_t j = i; j > 0; --j) {
						if (!slice[j - 1].used) continue;
						if (slice[j - 1].target < slice[i].target) break;
						++leftSkip;
						leftTarget = j - 1;
					}
					for (size_t j = i + 1; j < slice.size(); ++j) {
						if (!slice[j].used) continue;
						if (slice[j].target > slice[i].target) break;
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
				bestCanMove = (bestMoveLeft ? (bestTarget > 0 && !slice[bestTarget - 1].used) : (bestTarget + 1 < slice.size() && !slice[bestTarget + 1].used));
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
					if (i < slice.size() && slice[i].used) continue;
					rightMove = i - bestTarget;
					break;
				}
				for (size_t i = bestTarget; i > 0; --i) {
					if (slice[i - 1].used) continue;
					leftMove = bestTarget - i;
					break;
				}

				/* check if the left empty slot should be used and add the necessary rotations and patch the indices accordingly */
				if (leftMove > 0 && (leftMove <= rightMove || rightMove == 0)) {
					fInsertRotation(slice, bestTarget - leftMove, bestTarget, bestTarget - leftMove - 1, pGrid.size());
					--bestTarget;
					if (bestTarget > bestIndex && bestTarget - leftMove < bestIndex)
						--bestIndex;
				}

				/* apply the rotation to the right and patch the indices accordingly (right side might require an implicit new entry) */
				else {
					if (bestTarget + rightMove == slice.size())
						slice.push_back(Slot{});
					fInsertRotation(slice, bestTarget, bestTarget + rightMove, bestTarget + 1, pGrid.size());
					if (bestTarget < bestIndex && bestTarget + rightMove > bestIndex)
						++bestIndex;
				}

				/* apply the final actual crossing */
				fConstructAddCrossing(slice, bestIndex, bestTarget);
			}

			/* insert the remaining slice corrections until the slice is well-placed (must be possible, as
			*	they are well-ordered and can all fit into the row, and any misplacement implies that there
			*	must be empty spots to use for shifting; will automatically add any missing arrow heads) */
			fInsertSliceCorrections(slice, pGrid.size());

			/* ensure that at least one row was added, as otherwise two nodes may directly touch */
			if (numberOfRows == pGrid.size()) {
				detail::Row& row = pGrid.emplace_back();
				row.reserve(pGrid[0].size());
				fFillTraversals(row, slice, 0, slice.size());
			}
		}
		void fConstructGrid() {
			pGrid.clear();
			pFailedFixes.clear();

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
				std::vector<Slot> slice = fConstructNode(i, totalWidth, mapping);
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

			/* check if the vertical run already sits on the node cell closest
			*	to the primary corner and can therefore not be moved further */
			if (endIsNode && xEnd == xOther)
				return false;

			/* find the target axis to reduce the indentation by */
			target = (endIsNode ? xEnd : x);
			while (target != xOther) {
				/* the primary run must be a plain horizontal at the target, to allow a crossing, or, if the target
				*	lies beyond the second run, the other row must be empty there to provide space for the new corner */
				bool valid = (target == x || pGrid[y][target].kind == detail::CellKind::horizontal);
				if (valid && !endIsNode && !detail::Inbetween(target, xOther, xEnd))
					valid = pGrid[yOther][target].isEmpty();

				/* check the vertical axis from the primary corner to the height of the other corner (an arrow-head
				*	of the moved run can only be moved into an empty cell, as it can not be merged into a crossing) */
				if (valid) for (size_t i : detail::MinMaxWalk{ y, yOther, true }) {
					if (!(valid = (pGrid[i][xOther].isArrow() ? pGrid[i][target].isEmpty() : pGrid[i][target].freeVertical())))
						break;
				}

				/* incase of the primary corner lying further than the other corner,
				*	check if the horizontal to the secondary corner is free as well */
				if (valid && !detail::Inbetween(target, xOther, xEnd)) for (size_t i : detail::MinMaxWalk{ xEnd, target, true }) {
					if (!(valid = pGrid[yOther][i].freeHorizontal()))
						break;
				}

				/* ensure that the future new path would not introduce any new
				*	crossings (i.e. at most just as many as the path currently crosses) */
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
		bool fChangeCheckOutdated(TrackedRect& tracked) const {
			/* check if the rectangle itself is not yet valid */
			if (tracked.rect.empty())
				return true;

			/* check if any changes since the last check overlap with the testing rectangle */
			for (size_t i = tracked.age; i < pStraightenList.size(); ++i) {
				if (pStraightenList[i].overlap(tracked.rect))
					return true;
			}

			/* update the age to indicate that the tracked rectangle is still up to date */
			tracked.age = pStraightenList.size();
			return false;
		}
		void fChangeUpdateRow(size_t y, std::optional<size_t> added) {
			std::vector<StraightenCorner>& corners = pStraightenRows[y].corners;
			detail::Row& row = pGrid[y];
			size_t size = 0;

			/* iterate over the current corners and check if they actually still map onto corners */
			for (size_t i = 0; i < corners.size(); ++i) {
				if (!row[corners[i].x].anyCorner())
					continue;
				else if (size != i)
					corners[size] = corners[i];
				++size;
			}

			/* update the size to the new reduced list size to remove all dropped slots */
			corners.resize(size);

			/* insert the new corner into the sorted list (needs to be sorted in order to continue sweep properly) */
			if (!added.has_value())
				return;
			auto it = std::lower_bound(corners.begin(), corners.end(), *added, [](const StraightenCorner& c, size_t x) { return (c.x < x); });
			corners.insert(it, StraightenCorner{ .tracked = {}, .x = *added });
		}
		bool fStraightenGrid(Alignment align, size_t halfGrid) {
			bool dirty = false;

			/* find the next corner to pursue (iterate until all possible corners have been evaluated and return a change to
			*	indicate another sweep; only re-check corners, which might have become unblocked by changes since the last iteration) */
			for (size_t y = 0; y < pGrid.size(); ++y) {
				StraightenRow& row = pStraightenRows[y];

				/* check if the row is considered dirty and restart its iteration */
				if (row.corners.empty() || !fChangeCheckOutdated(row.tracked))
					continue;
				row.tracked = { .rect = GridRect{}, .age = pStraightenList.size() };

				/* iterate over the corners of the row and check if they are potential indented corners for this pass */
				for (size_t i = 0; i < row.corners.size();) {
					StraightenCorner& corner = row.corners[i++];
					size_t x = corner.x;

					/* check if the corner is a corner in the opposite direction in
					*	order to potentially want a change in the intended direction */
					bool dirLeftwards = (align == Alignment::outwards ? (x <= halfGrid) : (align == Alignment::leftwards));
					if (!(dirLeftwards ? pGrid[y][x].cornerRight() : pGrid[y][x].cornerLeft()))
						continue;

					/* check if the corner is outdated and otherwise just update the row-rect (after
					*	checking the direction to ensure the row-rect only contains relevant corners) */
					if (!fChangeCheckOutdated(corner.tracked)) {
						row.tracked.rect = (row.tracked.rect.empty() ? corner.tracked.rect : row.tracked.rect.merge(corner.tracked.rect));
						continue;
					}

					/* resolve the indentation corner and setup the rectangle of all
					*	cells inspected by the given indentation-check operation */
					size_t xOther = x, yOther = y, xEnd = x, target = 0;
					bool valid = fMapIndentCorner(x, y, xOther, yOther, xEnd, target);
					corner.tracked = {
						.rect = GridRect{
							.left = std::min<size_t>({ x, xOther, xEnd }), .top = std::min<size_t>(y, yOther),
							.right = std::max<size_t>({ x, xOther, xEnd }), .bottom = std::max<size_t>(y, yOther)
						},
						.age = pStraightenList.size()
					};
					row.tracked.rect = (row.tracked.rect.empty() ? corner.tracked.rect : row.tracked.rect.merge(corner.tracked.rect));

					/* check if a target axis to be reduced by was found */
					if (!valid || target == xOther)
						continue;
					bool endIsNode = pGrid[yOther][xOther].isNode();

					/* check if this is an outwards movement and the other side sits on the other side of
					*	the center, in which case the move needs to be skipped to prevent oscillations */
					if (align == Alignment::outwards && (xOther <= halfGrid) != (x <= halfGrid))
						continue;

					/* apply the shortening: the vertical run moves from xOther to target, the primary run is cut at target and the
					*	second run is either shortened, if the target inside it, or extended up to target, if the target is beyond xEnd */
					for (size_t i : detail::MinMaxWalk{ y, yOther, true }) {
						if (pGrid[i][xOther].isArrow()) {
							pGrid[i][target].kind = pGrid[i][xOther].kind;
							pGrid[i][xOther].kind = detail::CellKind::empty;
						}
						else {
							pGrid[i][target].kind = pGrid[i][target].addVertical();
							pGrid[i][xOther].kind = pGrid[i][xOther].removeVertical();
						}
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

					/* add the rect as change (before updating the corner lists as it invalidates the corner reference) */
					pStraightenList.push_back(corner.tracked.rect);
					dirty = true;

					/* update the corners of the two modified rows (only the end-rows are affected by the corner
					*	changes) and invalidate the other row, as it has now been modified and requires a re-scan */
					fChangeUpdateRow(y, (target != x ? std::optional<size_t>{ target } : std::nullopt));
					fChangeUpdateRow(yOther, (!endIsNode && target != xEnd ? std::optional<size_t>{ target } : std::nullopt));
					pStraightenRows[yOther].tracked.rect = GridRect{};

					/* continue the straightening after the current corner (due to the list being ordered, it matches the next in line) */
					for (i = 0; i < row.corners.size() && row.corners[i].x <= x; ++i);
				}
			}
			return dirty;
		}
		void fInsertDualSwap(size_t y, size_t yOther, size_t xCorner, size_t xCross) {
			/* insert the empty row to be used (insert it away from the y-other) */
			size_t yMove = (yOther > y ? y++ : y + 1);
			fInsertNullRow(yMove);

			/* move the corner over to the other column (the blocking transition now ends in the original corner) */
			pGrid[y][xCross].kind = pGrid[y][xCorner].kind;

			/* reconnect the original column through the new row into the other column */
			pGrid[yMove][xCorner].kind = pGrid[y][xCorner].kind;
			pGrid[y][xCorner].kind = detail::CellKind::vertical;
			pGrid[yMove][xCross].kind = pGrid[y][xCross].mirrorFull();
		}
		void fFixDualCornerBlock(size_t xCorner, size_t yCorner, bool leftwards) {
			/* check if the index lies right at the bounds, in which case
			*	the grid needs to be extended to provide space for a detour */
			if (leftwards ? xCorner == 0 : xCorner + 1 == pGrid[yCorner].size())
				fInsertEmptyColumn(leftwards, xCorner);

			/* ensure an empty space exists on the other side of the corner to bend around (caller
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
		bool fFixIsSingleCornerBlock(size_t xBase, size_t yBase, size_t xEnd, size_t yOther, size_t xBlock, size_t yBlock) const {
			size_t xSpacer = detail::AwayFrom(xBlock, xBase);
			bool baseLower = (yBase < yBlock);

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
			if (pGrid[yBase][xSpacer].freeVertical())
				return true;

			/* ensure that the base transition itself turns at the spacer (mirrored corner of the base) and that its other
			*	corner has the orientation of the blocking corner, as the detour reconnects both with mirrored kinds */
			if (pGrid[yBase][xBase].mirrorFull() != pGrid[yBase][xSpacer].kind)
				return false;
			if (pGrid[yOther][xBase].kind != pGrid[yBlock][xBlock].kind)
				return false;
			return true;
		}
		void fFixSingleCornerBlock(size_t xBase, size_t yBase, size_t xEnd, size_t yOther, size_t xBlock, size_t yBlock) {
			/* apply the single corner fix (requires the preconditions of fFixIsSingleCornerBlock to hold and the grid to be unmodified) */
			size_t xSpacer = detail::AwayFrom(xBlock, xBase);
			bool baseLower = (yBase < yBlock), baseLeft = (xBase < xBlock);

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
				return;
			}

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
		}
		bool fFixInterlocks(bool leftwards, GridPoint& point, size_t& appliedInterlockHash) {
			/* find the next corner to pursue (iterate until one has been found or all have been checked) */
			for (; point.y < pGrid.size(); point.x = 0, ++point.y) {
				detail::Row& row = pGrid[point.y];
				for (; point.x < row.size(); ++point.x) {
					detail::Cell& cell = row[point.x];

					/* check if the corner is a potential indented corner for this iteration (must have a corner
					*	in the opposite direction in order to potentially want a change in the intended direction) */
					if (!(leftwards ? cell.cornerRight() : cell.cornerLeft()))
						continue;

					/* resolve the surrounding indentation corner and check that no relevant target was found */
					size_t xOther = point.x, yOther = point.y, xEnd = 0, target = 0;
					if (!fMapIndentCorner(point.x, point.y, xOther, yOther, xEnd, target) || target != xOther)
						continue;

					/* check if there are any nodes in the way, in which case the interlocking cannot be removed and
					*	otherwise look for the blocking corners closest to base and the other corner (arrow-heads are
					*	not considered blocks, as fixes only interact with corners and will preserve the arrows) */
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

					/* helper to determine if the given fix has already been applied before, but failed, in which
					*	case it can be skipped again (lazily evaluated to prevent unnecessary hash computations) */
					const auto isFixBlocked = [this, point, xOther, yOther, leftwards, &appliedInterlockHash]() -> bool {
						appliedInterlockHash = fMeasureInterlock(point.x, point.y, xOther, yOther, leftwards);
						return pFailedFixes.contains(appliedInterlockHash);
					};

					/* check if no corner has been found, i.e. the blocker runs along the entire vertical run, in which case
					*	the two lines can only be swapped, which is possible if they end in the same node or cross each other */
					if (yBaseCorner == pGrid.size()) {
						/* check if both end in the same node, in which case the transitions can be swapped (if the transitions head
						*	into the node differs, it needs to be swapped as well, to ensure each transitions gets its correct head back) */
						if (pGrid[yOther][xCorner].isNode() && pGrid[yOther][xOther].isNode()) {
							if (isFixBlocked())
								continue;
							if (size_t yAdjacent = detail::Towards(yOther, point.y); yAdjacent != point.y && pGrid[yAdjacent][xOther].isArrow() != pGrid[yAdjacent][xCorner].isArrow())
								std::swap(pGrid[yAdjacent][xOther].kind, pGrid[yAdjacent][xCorner].kind);
							fInsertDualSwap(point.y, yOther, xOther, xCorner);
							return true;
						}

						/* check if the blocked transition crosses the blocking transition again as nothing can otherwise be done */
						if (pGrid[yOther][xCorner].kind != detail::CellKind::crossing)
							continue;

						/* check if this fix has already been applied once before and failed */
						if (isFixBlocked())
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
						if (isFixBlocked())
							continue;
						fFixDualCornerBlock(xCorner, (baseDualBlock ? yBaseCorner : yOtherCorner), leftwards);
						return true;
					}

					/* check if either one of the two blocking corners can be rotated outside of the current transition */
					if (fFixIsSingleCornerBlock(xOther, point.y, point.x, yOther, xCorner, yBaseCorner)) {
						if (isFixBlocked())
							continue;
						fFixSingleCornerBlock(xOther, point.y, point.x, yOther, xCorner, yBaseCorner);
						return true;
					}
					if (multiple && fFixIsSingleCornerBlock(xOther, yOther, xEnd, point.y, xCorner, yOtherCorner)) {
						if (isFixBlocked())
							continue;
						fFixSingleCornerBlock(xOther, yOther, xEnd, point.y, xCorner, yOtherCorner);
						return true;
					}
				}
			}
			return false;
		}

	private:
		void fShrinkHorizontal() {
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
		void fShrinkVertical() {
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
		std::pair<bool, size_t> fTestReductionSlot(const std::vector<size_t>& strand, size_t x) const {
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
		bool fFindRemovableStrand(std::vector<size_t>& strand) {
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
				if ((endReached = (begin >= prevEnd))) {
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
		void fCompressGrid() {
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
			while (pGrid[y][x].lineVertical())
				y = detail::Towards(y, last);

			/* evaluate the final pull direction */
			if (pGrid[y][x].cornerLeft())
				return -1;
			if (pGrid[y][x].cornerRight())
				return 1;
			return 0;
		}
		bool fShiftNode(std::map<NodePoint, intptr_t>& open, std::set<NodePoint>& closed, std::optional<ShiftTarget>& current) {
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

				/* insert the potential moves into the open set and update the scores
				*	accordingly (grid does not benefit from preferring free neighbor slots) */
				if (leftCheck && leftScore > 0)
					open.insert({ leftPoint, leftScore - rightScore - emptyScore });
				if (rightCheck && rightScore > 0)
					open.insert({ rightPoint, rightScore - leftScore - emptyScore });
			}

			/* helper to evaluate a given nodepoint */
			const auto evaluatePoint = [this, &nodeMap](const NodePoint& pt, size_t& distance, size_t& emptySlot, bool& leftwards) -> bool {
				detail::Row& row = pGrid[nodeMap[pt.index]];

				/* check if the node position has already been reached or if the point is stale
				*	(due to being a far away outdated target, which is not reachable anymore) */
				size_t first = std::ranges::find(row, detail::CellKind::node, &detail::Cell::kind) - row.begin(), last = 0;
				if (pt.x == first || pt.x >= row.size())
					return false;
				for (size_t i = first; i < row.size() && row[i].isNode(); ++i)
					last = i;
				leftwards = (pt.x < first);

				/* compute the points distance to its target and the next spot to mark as empty to move into the given direction */
				distance = (leftwards ? first - pt.x : pt.x - first);
				emptySlot = (leftwards ? last : first);
				return true;
			};

			/* check if a previous far away goal still exists and check if its still a relevant target, in which case it automatically
			*	becomes the best choice (must have come closer as the iteration may otherwise move the grid over and result in a loop) */
			size_t bestDistance = 0, bestEmptySlot = 0;
			bool bestLeftwards = false;
			auto best = open.end();
			if (current.has_value()) {
				if (evaluatePoint(current->point, bestDistance, bestEmptySlot, bestLeftwards) && bestDistance < current->distance)
					best = open.find(current->point);
				else
					open.erase(current->point);
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
				current = ShiftTarget{ .point = best->first, .distance = bestDistance };
			return true;
		}

	private:
		void fBuildStraighten(bool leftFirst) {
			Alignment first = (leftFirst ? Alignment::leftwards : Alignment::rightwards);
			Alignment second = (leftFirst ? Alignment::rightwards : Alignment::leftwards);

			/* reset the straighten state (keep across straightenings to prevent repeated allocations) */
			pStraightenList.clear();
			if (pStraightenRows.size() < pGrid.size())
				pStraightenRows.resize(pGrid.size());

			/* collect all of the corners into the change cache */
			for (size_t y = 0; y < pGrid.size(); ++y) {
				const detail::Row& gridRow = pGrid[y];
				StraightenRow& changeRow = pStraightenRows[y];
				changeRow.corners.clear();
				changeRow.tracked = { .rect = GridRect{}, .age = 0 };

				for (size_t x = 0; x < gridRow.size(); ++x) {
					if (gridRow[x].anyCorner())
						changeRow.corners.push_back(StraightenCorner{ .tracked = {}, .x = x });
				};
			}

			/* first straighten into the direction of the last fix, then into the opposite direction and finally
			*	outwards from the center (each pass unblocks corners for the next and is necessary for better results) */
			for (auto align : { first, second, Alignment::outwards }) {
				size_t halfGrid = 0;

				/* check if this is the outwards move, in which case the center axis is needed to determine
				*	the direction (fixed for the entire pass, to prevent drift or oscillations of corners) */
				if (align == Alignment::outwards) {
					size_t left = pGrid[0].size() - 1, right = 0;

					/* find the leftmost and rightmost edges over all columns (there
					*	must be at least one non-empty row and hence left <= right) */
					for (const detail::Row& row : pGrid) {
						size_t l = 0, r = row.size() - 1;
						while (l < left && row[l].isEmpty())
							++l;
						while (r > right && row[r].isEmpty())
							--r;
						left = std::min<size_t>(left, l);
						right = std::max<size_t>(right, r);
					}
					halfGrid = (left + right) / 2;
				}

				/* invalidate the row rectangles as they depend on the orientation of the straightening
				*	and apply the straightening until no more corners are found into the given direction */
				for (StraightenRow& row : pStraightenRows)
					row.tracked.rect = GridRect{};
				while (fStraightenGrid(align, halfGrid));
			}

			/* perform a shrinking to ensure irrelevant rows opened by the
			*	straightening do not strain the remainder of the algorithm */
			fShrinkVertical();
		}
		bool fBuildFixInterlocks(ascg::IterationStop auto&& stopIteration) const {
			/* collect the current best grid state as basis for interlock fixing and
			*	cache the best state to be able to recover it on worse performances */
			std::vector<detail::Row> bestGrid = pGrid;
			GridScore bestScore = fMeasureGrid();
			bool stopped = false;

			/* alternate the fix direction until a pass yields no improvement
			*	(at least one pass per direction) or the stop has been requested */
			for (size_t i = 0; !stopped; ++i) {
				bool dirty = false, leftwards = (i % 2 == 0);

				/* forward-iterate the interlock fixing into the given
				*	direction until one results in a positive outcome */
				GridPoint point{};
				size_t fixInterlockHash = 0;
				while (!(stopped = stopIteration()) && fFixInterlocks(leftwards, point, fixInterlockHash)) {
					/* first straighten the graph into the given direction, in order for any fix-changes
					*	to propagate properly, and then perform a general straightening and shrinking */
					fBuildStraighten(leftwards);

					/* advance the point and check if a new best grid has been reached and reset the state accordingly (if the
					*	fix worsens the grid, preserve the interlock-identification to prevent repeated fixing; keep the new
					*	state, if the results do not change, as the interlock fixing may allow for future fixes to be applied) */
					++point.x;
					if (intptr_t cmp = fApplyBestGrid(bestGrid, bestScore); cmp > 0)
						point = { .x = 0, .y = 0 }, dirty = true;
					else if (cmp < 0)
						pFailedFixes.insert(fixInterlockHash);
				}

				/* check if the fixing results in changes, in which
				*	case fixes into the other direction can be repeated */
				if (i > 0 && !dirty)
					break;
			}

			/* reset the grid to the best state (to capture minor best scores as well) */
			std::swap(pGrid, bestGrid);
			return !stopped;
		}

	public:
		std::vector<detail::Row> build(ascg::IterationStop auto&& stopIteration) {
			/* check if the layers are empty, in which case nothing needs to be done, and
			*	otherwise create the initial grid and straighten it (cannot be empty anymore) */
			if (pLayers.empty())
				return {};
			fConstructGrid();
			fBuildStraighten(true);

			/* setup the currently best grid version (to be able to recover to it in case of worse visited graphs) */
			std::vector<detail::Row> bestGrid = pGrid;
			GridScore bestScore = fMeasureGrid();

			/* iterate until no more nodes need to be shifted or the stop has been requested */
			std::optional<ShiftTarget> nodeCurrent;
			std::map<NodePoint, intptr_t> nodeOpen;
			std::set<NodePoint> nodeClosed;
			while (true) {
				/* fix any encountered interlocks (will automatically straighten the graph) and compress the grid, as the
				*	fixes leave rows behind which only the strand removal can collapse and which would skew the score */
				bool proceed = fBuildFixInterlocks(stopIteration);
				fCompressGrid();

				/* check if a new best graph has been encountered and reset all corresponding state */
				if (!nodeCurrent.has_value() && fApplyBestGrid(bestGrid, bestScore) > 0)
					nodeOpen.clear(), nodeClosed.clear(), nodeCurrent = std::nullopt;

				/* check if the stop has been requested while fixing the interlocks,
				*	in which case the current best grid can be returned */
				if (!proceed)
					break;

				/* try to shift another node into a better place, in which
				*	case the straightening and measuring can be repeated */
				if (!fShiftNode(nodeOpen, nodeClosed, nodeCurrent))
					break;
				fBuildStraighten(true);
			}

			/* return the complete generated grid (use the best state to capture minor best scores as well) */
			return bestGrid;
		}
	};
}
