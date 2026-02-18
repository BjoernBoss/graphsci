#pragma once

#include <string>
#include <vector>
#include <cinttypes>
#include <algorithm>
#include <set>
#include <map>
#include <utility>
#include <limits>


#include <ustring/ustring.h>

namespace ascg {
	namespace detail {
		struct Node {
			size_t row = 0;
			size_t in = 0;
			size_t out = 0;
			bool added = false;
		};

		struct RangeSort {
			bool operator()(const std::pair<size_t, size_t>& a, const std::pair<size_t, size_t>& b) const {
				if (a.first < b.first)
					return true;
				return (a.first == b.first) && (a.second < b.second);
			}
		};
	}

	struct Graph {
		std::vector<std::string> rows;
		std::vector<size_t> map;
	};

	class Builder {
	private:
		std::vector<std::string> pRows;
		std::vector<detail::Node> pNodes;
		size_t pWidth = 0;
		char pHalfIndexUp = 0;
		char pHalfIndexDown = 0;

	public:
		Builder(bool growDown) {
			pHalfIndexUp = (growDown ? '.' : '\'');
			pHalfIndexDown = (growDown ? '\'' : '.');
		}

	private:
		void fEnsureWidth(size_t width) {
			if (pWidth >= width)
				return;
			pWidth = width;
			for (std::string& row : pRows)
				row.resize(pWidth, ' ');
		}
		void fInsertRow(size_t index) {
			/* translate all nodes accordingly */
			for (detail::Node& node : pNodes) {
				if (node.row >= index)
					++node.row;
			}

			/* insert the filler line and extract the line below and above */
			std::string& line = *pRows.emplace(pRows.begin() + index, std::string(pWidth, ' '));
			std::string_view prev{ index > 0 ? std::string_view{ pRows[index - 1] } : std::string_view{} };
			std::string_view next{ index + 1 < pRows.size() ? std::string_view{pRows[index + 1]} : std::string_view{} };

			/* construct the added row such that all edges are preserved */
			for (size_t j = 0; j < prev.size(); ++j)
				if (prev[j] == '|' || prev[j] == pHalfIndexUp) line[j] = '|';
			for (size_t j = 0; j < next.size(); ++j)
				if (next[j] == '|' || next[j] == pHalfIndexDown) line[j] = '|';
		}
		void fInsertCrossing(size_t row, size_t in, size_t out) {
			std::string& line = pRows[row];

			/* add the entry and exit */
			line[in] = pHalfIndexDown;
			line[out] = pHalfIndexUp;

			/* add the intermediate lines */
			size_t start = std::min<size_t>(in, out), end = std::max<size_t>(in, out);
			for (size_t i = start + 1; i < end; ++i) {
				if (line[i] == ' ')
					line[i] = '-';
			}
		}
		void fEstablishNode(size_t index) {
			/* check if new rows need to be added to the output */
			while (pNodes.size() <= index) {
				pNodes.push_back({ .row = pRows.size(), .in = 0, .out = 0, .added = false });
				pRows.emplace_back(pWidth, ' ');
			}

			/* check if the node has already been added */
			detail::Node& node = pNodes[index];
			if (node.added)
				return;
		}

	public:
		void addNode(size_t index, size_t edges) {
			/* check if new rows need to be added to the output */
			while (pNodes.size() <= index) {
				pNodes.push_back({ .row = pRows.size(), .in = 0, .out = 0, .added = false });
				pRows.emplace_back(pWidth, ' ');
			}
			detail::Node& node = pNodes[index];

			/* check if the node has already been added */
			if (node.added)
				return;
			size_t offset = 0, length = (2 * edges - 1);

			/* find the place for the node within the row (must be axis-aligned to two characters) */
			std::string& row = pRows[node.row];
			for (size_t match = 0;; match = 0) {
				for (; match < length; ++match) {
					if (offset + match < row.size() && row[offset + match] != ' ')
						break;
				}
				if (match >= length)
					break;
				offset = (offset + match + 2) & ~size_t(0x01);
			}
			fEnsureWidth(offset + length);

			/* prepare the node and row */
			node.added = true;
			node.out = (node.in = offset);
			std::fill(row.begin() + offset, row.begin() + offset + length, '#');

			/* check if offset-rows need to be added above or beneath (in order to prevent nodes from clumping together) */
			for (size_t i = 0; i < 2; ++i) {
				if ((i == 0 ? node.row == 0 : node.row + 1 >= pRows.size()))
					continue;

				/* check if the nodes touch and insert a filler-line accordingly */
				const std::string& other = pRows[(i == 0 ? node.row - 1 : node.row + 1)];
				if (std::any_of(other.begin() + offset, other.begin() + offset + length, [](char c) { return (c == '#'); }))
					fInsertRow(i == 0 ? node.row : node.row + 1);
			}
		}
		void connect(size_t first, size_t last) {
			/* ensure the order of the nodes */
			if (first > last)
				std::swap(first, last);
			else if (first == last)
				return;

			/* allocate the source and target columns */
			size_t current = pNodes[first].out;
			pNodes[first].out += 2;
			size_t target = pNodes[last].in;
			pNodes[last].in += 2;
			fEnsureWidth(std::max<size_t>(current, target) + 1);

			/* translate the nodes to actual rows (first row to the actual first row to be modified) */
			first = pNodes[first].row + 1;
			last = pNodes[last].row;

			/* check if the row ahead is blocked by another component or if the two nodes
			*	are directly back to back, in which case another entry needs to be added */
			if (first == last || (pRows[first][current] != ' ' && pRows[first][current] != '-')) {
				fInsertRow(first);
				++last;
			}

			/* iterate until the nodes have been connected */
			while (first + 1 < last) {
				std::string& next = pRows[first];

				/* check if the path diretly above is blocked, in which case an intermediate row needs to be added */
				if (next[current] != ' ')
					fInsertRow(last++);

				/* check if the path can just move up */
				else if (pRows[first + 1][current] == ' ')
					next[current] = '|';
				else {
					/* check how many steps to the right are required until the top is free again */
					size_t shift = 2;
					while (current + shift < pWidth && pRows[first + 1][current + shift] != ' ') {
						if (next[current + shift] != ' ' && next[current + shift] != '|') {
							shift -= 2;
							break;
						}
						shift += 2;
					}
					fEnsureWidth(current + shift + 1);

					/* check if a shift can be performed, or if another empty row needs to be inserted */
					if (shift == 0)
						fInsertRow(last++);
					else {
						fInsertCrossing(first, current, current + shift);
						current += shift;
					}
				}

				/* advance to the next row */
				++first;
			}

			/* check if a simple connecting line is enough (between two nodes, there can at most be '-' or ' ' above) */
			if (current == target) {
				pRows[first][current] = '|';
				return;
			}

			/* check if the crossing can be performed or if another row needs to be inserted (if the row needs to
			*	be inserted, extend the current line by one, as it will otherwise be shy by one to the crossing) */
			size_t start = std::min<size_t>(current, target), finish = std::max<size_t>(current, target);
			if (std::any_of(pRows[first].begin() + start, pRows[first].begin() + finish, [](char c) { return (c != ' ' && c != '|'); })) {
				pRows[first][current] = '|';
				fInsertRow(++first);
			}

			/* insert the crossing */
			fInsertCrossing(first, current, target);
		}
		ascg::Graph finalize() {
			ascg::Graph out{ .rows = pRows };
			for (size_t i = 0; i < pNodes.size(); ++i)
				out.map.push_back(pNodes[i].row);
			return out;
		}
	};

	class _Render {
	private:
		enum class CellState : uint8_t {
			none,
			state,
			edge,
			cross
		};
		struct Cell {
			size_t edges[2] = { 0 };
			CellState state = CellState::none;
		};

	private:
		std::vector<std::vector<Cell>> pGrid;
		size_t pWidth = 0;
		size_t pStamp = 0;

	public:
		void connect(size_t a, size_t b) {

		}
		std::vector<std::string> finalize() const {
			std::vector<std::string> out;
			for (size_t i = 0; i < pGrid.size(); ++i) {
				std::string& row = out.emplace_back();
				for (size_t j = 0; j < pGrid[i].size(); ++j) {
					if (pGrid[i][j].state == CellState::none)
						row.push_back(' ');
					else if (pGrid[i][j].state == CellState::state)
						row.push_back('#');
					else if (pGrid[i][j].state == CellState::cross)
						row.push_back('|');
					else if ((j == 0 ? 0 : pGrid[i][j - 1].edges[0]) == pGrid[i][j].edges[0] && (j + 1 == pWidth ? 0 : pGrid[i][j + 1].edges[0]) == pGrid[i][j].edges[0])
						row.push_back('-');
					else if ((i == 0 ? 0 : pGrid[i - 1][j].edges[0]) == pGrid[i][j].edges[0] && (i + 1 == pGrid.size() ? 0 : pGrid[i + 1][j].edges[0]) == pGrid[i][j].edges[0])
						row.push_back('|');
					else
						row.push_back('+');
				}
			}
			return out;
		}
	};

	namespace detail {
		enum class StrandHit : uint8_t {
			none,
			node,
			edge
		};
		struct Range {
			size_t first = 0;
			size_t last = 0;
		};
		class OptNumber {
		private:
			size_t pValue = std::numeric_limits<size_t>::max();

		public:
			OptNumber() = default;
			OptNumber(size_t v) : pValue{ v } {}

		public:
			bool has() const {
				return (pValue != std::numeric_limits<size_t>::max());
			}
			size_t get() const {
				return (pValue == std::numeric_limits<size_t>::max() ? 0 : pValue);
			}
		};

		class Strands {
		public:
			std::vector<std::vector<detail::Range>> strands;
			size_t conflicts = 0;

		private:
			detail::StrandHit fCheckStrand(size_t strand, size_t node) const {
				const std::vector<detail::Range>& s = strands[strand];

				/* check if any of the ranges of the given strand start
				*	or end at the given node or if it lies within a range */
				for (size_t i = s.size(); i > 0; --i) {
					if (s[i - 1].first == node || s[i - 1].last == node)
						return detail::StrandHit::node;
					if (s[i - 1].last < node)
						break;
					if (s[i - 1].first < node)
						return detail::StrandHit::edge;
				}
				return detail::StrandHit::none;
			}
			detail::OptNumber fPrevEdgesCut(size_t strand, size_t node) const {
				size_t count = 0;
				for (; strand > 0; --strand) {
					if (detail::StrandHit hit = fCheckStrand(strand - 1, node); hit == detail::StrandHit::node)
						return count;
					else if (hit == detail::StrandHit::edge)
						++count;
				}
				return detail::OptNumber{};
			}
			detail::OptNumber fNextEdgesCut(size_t strand, size_t node, bool self) const {
				size_t count = 0;
				for (strand += (self ? 0 : 1); strand < strands.size(); ++strand) {
					if (detail::StrandHit hit = fCheckStrand(strand, node); hit == detail::StrandHit::node)
						return count;
					else if (hit == detail::StrandHit::edge)
						++count;
				}
				return detail::OptNumber{};
			}
			detail::OptNumber fNextNodeDistance(size_t strand, size_t node, bool self) const {
				size_t count = 0;
				for (strand += (self ? 0 : 1); strand < strands.size(); ++strand) {
					if (fCheckStrand(strand, node) == detail::StrandHit::node)
						return count;
					++count;
				}
				return detail::OptNumber{};
			}
			void fInsert(detail::Range range, size_t beforeStrand) {
				/* check if the range can be appended to existing strands or insert it */
				if (beforeStrand > 0 && strands[beforeStrand - 1].back().last <= range.first)
					strands[--beforeStrand].push_back(range);
				else if (beforeStrand < strands.size() && strands[beforeStrand].back().last <= range.first)
					strands[beforeStrand].push_back(range);
				else
					strands.insert(strands.begin() + beforeStrand, { range });

				/* update the crossings to include the costs of all nodes separated due to the new range */
				for (size_t i = range.first + 1; i < range.last; ++i) {
					if (fPrevEdgesCut(beforeStrand, i).has() && fNextEdgesCut(beforeStrand, i, false).has())
						++conflicts;
				}

				/* check if the start of the node now results in edges above cutting the node (only if not already done by the neighbor) */
				const std::vector<detail::Range>& thisStrand = strands[beforeStrand];
				if (thisStrand.size() == 1 || thisStrand[thisStrand.size() - 2].last < range.first) {
					detail::OptNumber firstPrev = fPrevEdgesCut(beforeStrand, range.first), firstNext = fNextEdgesCut(beforeStrand, range.first, false);
					if (firstPrev.has() != firstNext.has())
						conflicts += firstPrev.get() + firstNext.get();
				}

				/* check if the end of the node now results in edges below cutting the node */
				detail::OptNumber lastPrev = fPrevEdgesCut(beforeStrand, range.last), lastNext = fNextEdgesCut(beforeStrand, range.last, false);
				if (lastPrev.has() != lastNext.has())
					conflicts += lastPrev.get() + lastNext.get();
			}
			void fReduce(size_t strand) {
				std::vector<detail::Range>& thisStrand = strands[strand];
				detail::Range range = thisStrand.back();

				/* check if the start of the node previously resulted in cut edges, which
				*	are now resolved (only if this is not still held alive by the neighbor) */
				if (thisStrand.size() == 1 || thisStrand[thisStrand.size() - 2].last < range.first) {
					detail::OptNumber firstPrev = fPrevEdgesCut(strand, range.first), firstNext = fNextEdgesCut(strand, range.first, false);
					if (firstPrev.has() != firstNext.has())
						conflicts -= firstPrev.get() + firstNext.get();
				}

				/* check if the end of the node previously resulted in cut edges, which are now resolved */
				detail::OptNumber lastPrev = fPrevEdgesCut(strand, range.last), lastNext = fNextEdgesCut(strand, range.last, false);
				if (lastPrev.has() != lastNext.has())
					conflicts -= lastPrev.get() + lastNext.get();

				/* update the crossings to remove the costs of all nodes previously separated due to the range */
				for (size_t i = range.first + 1; i < range.last; ++i) {
					if (fPrevEdgesCut(strand, i).has() && fNextEdgesCut(strand, i, false).has())
						--conflicts;
				}

				/* check if the entire strand can be removed and otherwise just remove the range */
				if (thisStrand.size() == 1)
					strands.erase(strands.begin() + strand);
				else {
					thisStrand.pop_back();
					++strand;
				}
			}

		public:
			size_t insert(detail::Range range) {
				/* initialize the count as if the range was inserted before the first stran */
				size_t conflicting = fNextEdgesCut(0, range.first, true).get() + fNextEdgesCut(0, range.last, true).get();
				detail::OptNumber distFirstPrev = {}, distFirstNext = fNextNodeDistance(0, range.first, true);
				detail::OptNumber distLastPrev = {}, distLastNext = fNextNodeDistance(0, range.last, true);

				/* setup insertion before the first strand as the current best pick (for two strands
				*	of equal conflicts, choose the one, which does not require a new strand to be added) */
				size_t bestBefore = 0, bestConflicting = conflicting, bestDistance = (distFirstNext.get() + distLastNext.get());
				bool bestMergeable = (bestBefore < strands.size() && strands[bestBefore].back().last <= range.first);

				/* iterate over the strands, and update the conflict count, if the range were
				*	inserted after the strand, and find the least expensive insertion strand */
				for (size_t i = 0; i < strands.size(); ++i) {
					const std::vector<detail::Range>& strand = strands[i];

					/* iterate over the skipped ranges within the strand */
					for (size_t j = strand.size(); j > 0; --j) {
						const detail::Range& test = strand[j - 1];
						if (test.last <= range.first)
							break;
						if (test.first >= range.last)
							continue;

						/* check if the beginning of the [test] range passed through the inserted range,
						*	or vice versa, and update the new conflicting counts down and upwards */
						if (test.first < range.first) {
							if (fPrevEdgesCut(i, range.first).has())
								++conflicting;
							if (fNextEdgesCut(i, range.first, false).has())
								--conflicting;
						}
						else if (test.first > range.first) {
							if (fNextEdgesCut(i, test.first, false).has())
								++conflicting;
							if (fPrevEdgesCut(i, test.first).has())
								--conflicting;
						}

						/* check if the end of the range to be inserted passed through the [test] range, or vice versa,
						*	and update the new conflicting counts down and upwards (for a test-range within the range
						*	to be inserted, dont update the counts, if its direct neighbor lies at the stame node, as
						*	this might otherwise lead to double reductions for single values) */
						if (test.last > range.last) {
							if (fPrevEdgesCut(i, range.last).has())
								++conflicting;
							if (fNextEdgesCut(i, range.last, false).has())
								--conflicting;
						}
						else if (test.last < range.last && (j == strand.size() || strand[j].first > test.last)) {
							if (fNextEdgesCut(i, test.last, false).has())
								++conflicting;
							if (fPrevEdgesCut(i, test.last).has())
								--conflicting;
						}
					}

					/* update the distance counters and compute the new distance */
					if (distFirstPrev.has())
						distFirstPrev = distFirstPrev.get() + 1;
					if (distLastPrev.has())
						distLastPrev = distLastPrev.get() + 1;
					if (distFirstNext.has()) {
						if (distFirstNext.get() > 0)
							distFirstNext = distFirstNext.get() - 1;
						else
							distFirstPrev = 0, distFirstNext = fNextNodeDistance(i, range.first, false);
					}
					if (distLastNext.has()) {
						if (distLastNext.get() > 0)
							distLastNext = distLastNext.get() - 1;
						else
							distLastPrev = 0, distLastNext = fNextNodeDistance(i, range.last, false);
					}
					size_t distance = (distFirstPrev.get() + distLastPrev.get() + distFirstNext.get() + distLastNext.get());

					/* check if this insertion spot is better suited than the current best slot */
					if (conflicting > bestConflicting)
						continue;
					if (conflicting == bestConflicting && distance > bestDistance)
						continue;
					bool mergable = ((i > 0 && strands[i - 1].back().last <= range.first) || (strand.back().last <= range.first));
					if (conflicting < bestConflicting || distance < bestDistance || (mergable && !bestMergeable))
						bestBefore = i + 1, bestConflicting = conflicting, bestDistance = distance, bestMergeable = mergable;
				}

				/* insert the new range at the given position */
				fInsert(range, bestBefore);
				return bestConflicting;
			}
			std::vector<detail::Range> dropOverlap(detail::Range range) {
				std::vector<detail::Range> dropped;

				/* iterate over the strands and remove all ranges, which overlap, or pass, the given range
				*	(back to front, as the reduction may merge strands and may move ranges further down) */
				for (size_t i = strands.size(); i > 0; --i) {
					std::vector<detail::Range>& strand = strands[i - 1];
					if (strand.back().first == range.first && strand.back().last == range.last)
						continue;

					/* check if the last strand can be removed */
					if (strand.back().last <= range.first)
						continue;

					/* store the dropped range and remove it */
					dropped.push_back(strand.back());
					fReduce(i - 1);
				}
				return dropped;
			}


		private:
			detail::OptNumber fFitsIntoStrand(const std::vector<detail::Range>& strand, const detail::Range& range) const {
				/* iterate over the ranges of the strand and check if the slot is vacant */
				for (size_t i = strand.size(); i > 0; --i) {
					if (strand[i - 1].last <= range.first)
						return i;
					if (strand[i - 1].first < range.last)
						return detail::OptNumber{};
				}
				return 0;
			}
		public:
			void print() const {
				std::vector<std::vector<detail::Range>> reduced;
				size_t lastNode = 0;

				/* iterate over the ranges and insert them to the
				*	reduced strand set and find the last node index */
				for (size_t i = 0; i < strands.size(); ++i) {
					for (size_t j = 0; j < strands[i].size(); ++j) {
						lastNode = std::max<size_t>(lastNode, strands[i][j].last);

						/* find the reduced slot to add the range to */
						detail::OptNumber index{};
						size_t target = reduced.size();
						while (target > 0) {
							detail::OptNumber temp = fFitsIntoStrand(reduced[target - 1], strands[i][j]);
							if (!temp.has())
								break;
							index = temp, --target;
						}
						if (index.has())
							reduced[target].insert(reduced[target].begin() + index.get(), strands[i][j]);
						else
							reduced.push_back({ strands[i][j] });
					}
				}

				/* print the numbers */
				for (size_t i = 0; i <= lastNode; ++i)
					str::Fmt("{: <4}", i);
				str::PrintLn(" (Conflicts: ", conflicts, ')');

				/* print the separate strands */
				//for (size_t i = 0; i < reduced.size(); ++i) {
				//	std::string line;
				//	for (const auto& r : reduced[i]) {
				//		line.append(r.first * 4 - line.size(), ' ');
				//		line.append(1, '[').append((r.last - r.first) * 4 - 2, '-').append(1, ']');
				//	}
				//	str::PrintLn(line);
				//}

				for (size_t i = 0; i < strands.size(); ++i) {
					std::string line;
					for (const auto& r : strands[i]) {
						line.append(r.first * 4 - line.size(), ' ');
						line.append(1, '[').append((r.last - r.first) * 4 - 2, '-').append(1, ']');
					}
					str::PrintLn(line);
				}
			}
		};
	}

	/* add self-loops?/double-edges?/remove 'std::pair'/add directions? */
	/* make use of [pFirst], [pLast] */
	class _Builder {
	private:
		std::vector<std::pair<size_t, size_t>> pRanges;
		size_t pFirst = 0;
		size_t pLast = 0;

	private:
		size_t fDistance(size_t x, size_t y) const {
			return (x < y ? (y - x) : (x - y));
		}
		bool fHasPrev(const std::vector<std::vector<std::pair<size_t, size_t>>>& strands, size_t index, size_t node) const {
			while (true) {
				if (strands[index].back().first == node || strands[index].back().second == node)
					return true;
				if (index-- == 0)
					return false;
			}
		}
		bool fHasNext(const std::vector<std::vector<std::pair<size_t, size_t>>>& strands, size_t index, size_t node) const {
			for (; index < strands.size(); ++index) {
				if (strands[index].back().first == node || strands[index].back().second == node)
					return true;
			}
			return false;
		}
		size_t fCountNext(const std::vector<std::vector<std::pair<size_t, size_t>>>& strands, size_t index, size_t node) const {
			size_t count = 0;
			for (; index < strands.size(); ++index) {
				if (strands[index].back().first == node || strands[index].back().second == node)
					return count;
				if (strands[index].back().second > node)
					++count;
			}
			return 0;
		}

	public:
		void connect(size_t a, size_t b) {
			if (a > b)
				std::swap(a, b);
			else if (a == b)
				return;

			/* update the overall limits and insert the range */
			if (pRanges.empty())
				pFirst = a, pLast = b;
			else
				pFirst = std::min<size_t>(pFirst, a), pLast = std::max<size_t>(pLast, b);
			pRanges.emplace_back(a, b);
		}
		std::vector<std::vector<std::pair<size_t, size_t>>> finalize() {
			std::vector<std::vector<std::pair<size_t, size_t>>> strands;

			/* insert all ranges to the active set (sorted by start index, and then length) */
			std::set<std::pair<size_t, size_t>, detail::RangeSort> open(pRanges.begin(), pRanges.end());

			/* assign the ranges to the separate strands */
			while (!open.empty()) {
				std::pair<size_t, size_t> next = *open.begin();
				open.erase(open.begin());

				/* initialize the costs */
				size_t costs = fCountNext(strands, 0, next.first) + fCountNext(strands, 0, next.second);
				std::vector<size_t> bestBefore{ 0 };
				size_t bestCosts = costs;

				/* iterate over the strands and compute the costs for the strand lying inbetween */
				for (size_t i = 0; i < strands.size(); ++i) {
					const std::pair<size_t, size_t>& test = strands[i].back();

					/* check if the end of [test] lies within [next], in which case its costs need to shift */
					if (test.second > next.first && test.second < next.second) {
						if (i > 0 && fHasPrev(strands, i - 1, test.second))
							--costs;
						if (fHasNext(strands, i + 1, test.second))
							++costs;
					}

					/* check if the start of [next] lies within [test], in which case its costs need to shift */
					if (next.first > test.first && next.first < test.second) {
						if (fHasNext(strands, i + 1, next.first))
							--costs;
						if (i > 0 && fHasPrev(strands, i - 1, next.first))
							++costs;
					}

					/* check if the end of [next] lies within [test], in which case its costs need to shift */
					if (next.second < test.second) {
						if (fHasNext(strands, i + 1, next.second))
							--costs;
						if (i > 0 && fHasPrev(strands, i - 1, next.second))
							++costs;
					}

					/* check if the new row would cost less or can also be considered */
					if (costs > bestCosts)
						continue;
					if (costs < bestCosts)
						bestBefore.clear();
					bestBefore.push_back(i + 1);
					bestCosts = costs;
				}

				/* check if one of the solutions does not require a row to be added */
				bool added = false;
				for (size_t before : bestBefore) {
					if (before > 0 && strands[before - 1].back().second <= next.first)
						strands[before - 1].push_back(next);
					else if (before < strands.size() && strands[before].back().second <= next.first)
						strands[before].push_back(next);
					else
						continue;
					added = true;
					break;
				}

				/* check if the entry has not yet been added, in which case the entry closest to the
				*	edge can be chosen (to ensure the larger entries for same start move outward) */
				if (!added)
					strands.insert(strands.begin() + bestBefore.back(), { next });


				for (size_t i = 0; i < 6; ++i)
					str::Fmt("{: >4}", i + 1);
				str::PrintLn();
				for (size_t i = 0; i < strands.size(); ++i) {
					std::string line;
					for (const auto& r : strands[i]) {
						line.append(r.first * 4 - line.size(), ' ');
						line.append(1, '[').append((r.second - r.first) * 4 - 2, '-').append(1, ']');
					}
					str::PrintLn(line);
				}
				str::PrintLn();
			}
			return {};
		}

	private:
		struct Node {
			std::set<std::pair<size_t, size_t>>::iterator source;
			size_t stamp = 0;
			size_t iteration = 0;
			bool mapped = false;
		};
		struct Sorter {
			bool operator()(std::pair<size_t, size_t> a, std::pair<size_t, size_t> b) const {
				size_t la = (a.second - a.first), lb = (b.second - b.first);
				if (la != lb)
					return (la < lb);
				return (a.first < b.first);
			}
		};

	public:
		void _finalize() {
			detail::Strands strands;

			std::map<size_t, Node> nodes;
			std::set<std::pair<size_t, size_t>, Sorter> remaining;
			for (const std::pair<size_t, size_t>& r : pRanges) {
				nodes[r.first] = Node{};
				nodes[r.second] = Node{};
				remaining.insert(r);
			}
			size_t stamp = 0;

			while (!remaining.empty()) {
				++stamp;
				std::set<std::pair<size_t, size_t>>::iterator next = remaining.end();

				/* find the next best match */
				for (size_t iteration = 0;; ++iteration) {
					bool dirty = false;

					/* take the next step through the graph */
					for (auto it = remaining.begin(); it != remaining.end(); ++it) {
						std::pair<size_t, size_t> r = *it;

						bool firstVisit = (nodes[r.first].mapped || (nodes[r.first].stamp == stamp && nodes[r.first].iteration < iteration));
						bool secondVisit = (nodes[r.second].mapped || (nodes[r.second].stamp == stamp && nodes[r.second].iteration < iteration));
						if (!firstVisit && !secondVisit)
							continue;
						dirty = true;

						if (firstVisit != secondVisit) {
							size_t next = (firstVisit ? r.second : r.first), prev = (firstVisit ? r.first : r.second);

							if (nodes[next].stamp != stamp) {
								nodes[next].stamp = stamp;
								nodes[next].iteration = iteration;
								nodes[next].source = (iteration == 0 ? it : nodes[prev].source);
							}
							else if (Sorter{}(*it, *nodes[next].source))
								nodes[next].source = it;
							continue;
						}

						std::set<std::pair<size_t, size_t>>::iterator source = it;
						if (!nodes[r.first].mapped && !nodes[r.second].mapped)
							source = (Sorter{}(*nodes[r.first].source, *nodes[r.second].source) ? nodes[r.first] : nodes[r.second]).source;

						if (next == remaining.end() || Sorter{}(*source, *next))
							next = source;
					}
					if (!dirty || next != remaining.end())
						break;
				}

				/* if no range was found, select the first range */
				if (next == remaining.end())
					next = remaining.begin();

				/* mark the range as processed and the nodes as visited */
				std::pair<size_t, size_t> range = *next;
				remaining.erase(next);
				nodes[range.first].mapped = true;
				nodes[range.second].mapped = true;

				/* insert the range */
				strands.insert({ range.first, range.second });
				strands.print();
				str::PrintLn();
			}

			return;





			/* insert all ranges to the active set (sorted by start index, and then length) */
			std::set<std::pair<size_t, size_t>, detail::RangeSort> open(pRanges.begin(), pRanges.end());

			/* assign the ranges to the separate strands */
			while (!open.empty()) {
				std::pair<size_t, size_t> next = *open.begin();
				open.erase(open.begin());

				/* try each insertion and pick the best */
				size_t costs = strands.insert({ next.first, next.second });
				strands.print();
				str::PrintLn();

				if (costs > 0) {
					auto dropped = strands.dropOverlap({ next.first, next.second });

					strands.print();
					str::PrintLn();
					for (const auto& r : dropped)
						open.insert({ r.first, r.last });

					//for (const auto& range : ranges)
					//	_insert(strands, { range.first, range.second }, nullptr);
					//strands.print();
					//str::PrintLn();
				}
			}
		}
	};
}
