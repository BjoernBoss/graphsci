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
			size_t& base() {
				return pValue;
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
			size_t fShiftStrands(size_t strand, detail::Range range) {
				if (strand >= strands.size()) {
					strands.emplace_back();
					return 0;
				}
				std::vector<detail::Range>& s0 = strands[strand];

				/* find the affected indices */
				size_t first = 0, count = 0, index = 0;
				for (size_t i = s0.size(); i > 0; --i) {
					if (s0[i - 1].first >= range.last)
						continue;
					else if (s0[i - 1].last <= range.first) {
						index = i;
						break;
					}
					first = i - 1, ++count;
				}

				/* check if the space needs to be made available */
				if (count == 0)
					return index;
				size_t target = fShiftStrands(strand + 1, { .first = s0[first].first, .last = s0[first + count - 1].last });
				std::vector<detail::Range>& s1 = strands[strand];

				/* write the range into the next strand and remove them from the current strand */
				strands[strand + 1].insert(strands[strand + 1].begin() + target, s1.begin() + first, s1.begin() + first + count);
				s1.erase(s1.begin() + first, s1.begin() + first + count);
				return index;
			}
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
			void fInsert(detail::Range range, size_t beforeStrand) {
				/* check if the strand can also be added to previous strands */
				detail::OptNumber index;
				while (beforeStrand > 0) {
					detail::OptNumber temp = fFitsIntoStrand(strands[beforeStrand - 1], range);
					if (!temp.has())
						break;
					index = temp, --beforeStrand;
				}

				/* make space for the range and insert it */
				if (!index.has())
					index = fShiftStrands(beforeStrand, range);
				std::vector<detail::Range>& s = strands[beforeStrand];
				s.insert(s.begin() + index.get(), range);

				/* update the crossings to include the costs of all nodes separated due to the new range */
				for (size_t i = range.first + 1; i < range.last; ++i) {
					if (fPrevEdgesCut(beforeStrand, i).has() && fNextEdgesCut(beforeStrand, i, false).has())
						++conflicts;
				}

				/* check if the start of the node now results in edges above cutting the node (only if not already done by the neighbor) */
				if (index.get() == 0 || s[index.get() - 1].last < range.first) {
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
				size_t options = 1;

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
						bestBefore = i + 1, bestConflicting = conflicting, bestDistance = distance, bestMergeable = mergable, options = 1;
					else if (mergable == bestMergeable)
						++options;
				}
				str::PrintLn("Options: ", options);

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
		struct Sorted {
			bool operator()(std::pair<size_t, size_t> a, std::pair<size_t, size_t> b) const {
				if (a.first != b.first)
					return (a.first < b.first);
				return (a.second < b.second);
			}
		};

	private:
		struct Point {
			std::pair<size_t, size_t> range;
			size_t slotPrev = 0;
			size_t slotNext = 0;
			Point* prev = nullptr;
			Point* next = nullptr;
		};
		struct Layer {
			std::vector<Point> points;
			std::pair<size_t, size_t> node;
			size_t index = 0;
			size_t crossings = 0;
		};

	private:
		size_t fPeekSwap(Layer& layer) const {
			// fix: referencing into layers even if layers is still changing
		}
		void fSwapPoints(Point& prev, Point& next) const {
			/* rotate the references to previous and upcoming points around */
			if (prev.prev != nullptr)
				prev.prev->next = &next;
			if (prev.next != nullptr)
				prev.next->prev = &next;
			if (next.prev != nullptr)
				next.prev->next = &prev;
			if (next.next != nullptr)
				next.next->prev = &prev;

			/* check if the slot needs to be moved or can remain, as the points have been moved into the according slot */
			if (prev.next == nullptr || next.next == nullptr)
				(prev.next == nullptr ? prev : next).slotNext = (prev.next == nullptr ? next : prev).slotNext;
			if (prev.prev == nullptr || next.prev == nullptr)
				(prev.prev == nullptr ? prev : next).slotPrev = (prev.prev == nullptr ? next : prev).slotPrev;

			/* swap the points themselves */
			std::swap(prev.range, next.range);
			std::swap(prev.prev, next.prev);
			std::swap(prev.next, next.next);
		}
		void fPatchLayer(Layer& layer) const {
			/* iterate over all points and try to swap any neighboring points, which cross to the previous layer */
			for (size_t i = 1; i < layer.points.size(); ++i) {
				if (layer.points[i].prev == nullptr)
					continue;

				/* loop until the current point cannot be swapped with the previous point */
				size_t nextIndex = i;
				while (true) {
					if (layer.crossings == 0)
						return;
					Point& next = layer.points[nextIndex];

					/* find the last relevant point, which crosses the current point */
					if (next.slotPrev == 0)
						break;
					size_t prevIndex = nextIndex - 1;
					while (layer.points[prevIndex].prev == nullptr || layer.points[prevIndex].slotPrev == next.slotPrev)
						--prevIndex;
					Point& prev = layer.points[prevIndex];

					/* check if either both, or neither, belonged to the node, in which case they can just be swapped directly */
					bool nextNode = (nextIndex >= layer.node.first && nextIndex < layer.node.first + layer.node.second);
					bool prevNode = (prevIndex >= layer.node.first && prevIndex < layer.node.first + layer.node.second);
					if (nextNode == prevNode) {
						/* check if the points cross and can be swapped */
						if (prev.prev->slotNext <= next.prev->slotNext)
							break;

						/* cycle all points through and restart the procedure from the previous point */
						for (size_t j = prevIndex; j < nextIndex; ++j)
							fSwapPoints(layer.points[j], layer.points[j + 1]);
						--layer.crossings, nextIndex = prevIndex;
						continue;
					}

					/* count the number of points part of the node, which are mapped before and after
					*	the single point, as a swap of the node would also swap all of these around */
					size_t otherPoint = (nextNode ? prev : next).prev->slotNext, mappedBefore = 0, mappedAfter = 0;
					for (size_t j = layer.node.first; j < layer.node.first + layer.node.second; ++j) {
						if (layer.points[j].prev == nullptr)
							continue;
						if (layer.points[j].prev->slotNext < otherPoint)
							++mappedBefore;
						else if (layer.points[j].prev->slotNext > otherPoint)
							++mappedAfter;
					}

					/* handle the swap according to the direction of the node movement */
					if (nextNode) {
						/* check if the swap would reduce the overall number of crossings */
						if (mappedBefore <= mappedAfter)
							break;

						/* patch the slot indices the node, as they will be preserved by the point-swaps, and the swaps
						*	would only leave the first point of the node correct, but the remaining in the upper slot */
						for (size_t j = layer.node.first + 1; j < layer.node.first + layer.node.second; ++j)
							--layer.points[j].slotPrev;

						/* rotate the previous point to be placed after all points of the node and
						*	reset the index to repeat the process on the point at the new front */
						for (size_t j = prevIndex + 1; j < layer.node.first + layer.node.second; ++j)
							fSwapPoints(layer.points[j - 1], layer.points[j]);
						layer.crossings -= (mappedBefore - mappedAfter);
						--layer.node.first, nextIndex = prevIndex;
						continue;
					}

					/* check if the swap would reduce the overall number of crossings */
					if (mappedBefore >= mappedAfter)
						break;

					/* patch the slot indices of the node, as they will be preserved by the point-swaps, and the swaps
					*	would only leave the last point of the node correct, but the remaining in the lower slot */
					for (size_t j = layer.node.first + 1; j < layer.node.first + layer.node.second; ++j)
						++layer.points[j].slotPrev;

					/* rotate the next point to lie before all points of the node
					*	and reset the index accordingly to try to restart the process */
					for (size_t j = nextIndex; j > layer.node.first; --j)
						fSwapPoints(layer.points[j - 1], layer.points[j]);
					layer.crossings -= (mappedAfter - mappedBefore);
					nextIndex = layer.node.first++;
				}
			}
		}

	private:
		void fFinalize() const {
			/* setup the list of open ranges (sorted by their start) */
			std::set<std::pair<size_t, size_t>, Sorted> open(pRanges.begin(), pRanges.end());
			std::vector<Layer> layers;
			size_t crossings = 0;

			/* iterate over the open ranges until they and their endpoints have all been inserted into the layers,
			*	and the crossing count has been computed (no need to map previous for the first layer, as the first
			*	layer can only consist of a single node, in which case the ranges can be swapped arbitrarily) */
			std::map<std::pair<size_t, size_t>, size_t> previous;
			while (true) {
				/* insert all upcoming nodes, which need to be added to this layer */
				if (!open.empty()) {
					auto itNext = open.begin();

					/* check if a new layer needs to be pushed */
					if (layers.empty() || layers.back().index < itNext->first)
						layers.push_back(Layer{ .index = itNext->first });
					Layer& layer = layers.back();

					/* insert all ranges, which start before or at the current layer */
					while (!open.empty() && itNext->first <= layer.index) {
						layer.points.push_back({ .range = *itNext });
						layer.index = std::min<size_t>(itNext->first, layer.index);
						itNext = open.erase(itNext);
					}
				}
				Layer& layer = layers.back();

				/* sort the ranges in the layer to ensure ranges, which belong to this layer (i.e. start/end at
				*	this layer), are neighbors (must at least be one), and setup the node range accordingly */
				layer.node = { 0, 0 };
				for (size_t i = 0; i < layer.points.size(); ++i) {
					if (layer.points[i].range.first != layer.index && layer.points[i].range.second != layer.index)
						continue;
					if (layer.node.second != i)
						std::swap(layer.points[i], layer.points[layer.node.second]);
					++layer.node.second;
				}

				/* iterate over the ranges again (after sorting, as this rotates the ranges
				*	around) and setup the mappings and references to the previous layer */
				size_t nextSlotPrev = 0;
				for (size_t i = 0; i < layer.points.size(); ++i) {
					/* check if this range is part of the previous layer */
					Point& point = layer.points[i];
					if (point.range.first >= layer.index)
						continue;

					/* link the points together (references are stable, as the lists have already been
					*	populated with all points, and will not be reallocated anymore, just moved) */
					point.prev = &layers[layers.size() - 2].points[previous.find(point.range)->second];
					point.slotPrev = (i < layer.node.second ? 0 : ++nextSlotPrev);
					point.prev->next = &layer.points[i];

					/* update the crossing count by checking if this mapping crosses any previous mappings */
					if (i >= layer.node.second) for (size_t j = 0; j < i; ++j) {
						if (layer.points[j].prev != nullptr && layer.points[j].prev->slotNext > point.prev->slotNext)
							++layer.crossings;
					}
				}

				/* try to reduce the initial crossing count for the upcoming layer and update the total count */
				fPatchLayer(layer);
				crossings += layer.crossings;

				/* iterate over the ranges again (after the initial patch, as this rotates
				*	the ranges around) and setup the initial mapping for the upcoming layer */
				std::map<std::pair<size_t, size_t>, size_t> upcoming;
				size_t nextSlotNext = 0;
				for (size_t i = 0; i < layer.points.size(); ++i) {
					/* check if this range will be part of the upcoming layer */
					Point& point = layer.points[i];
					if (point.range.second <= layer.index)
						continue;

					/* allocate the slot index and add it to the upcoming mapping (node is not necessarily at slot 0 anymore) */
					point.slotNext = nextSlotNext;
					if (i < layer.node.first || i + 1 >= layer.node.first + layer.node.second)
						++nextSlotNext;
					upcoming.insert({ point.range, i });
				}
				previous = std::move(upcoming);

				/* iterate over the current layer and setup the
				*	extension of all open ranges into the next layer */
				Layer next{ .index = std::numeric_limits<size_t>::max() };
				for (const Point& point : layer.points) {
					if (point.range.second <= layer.index)
						continue;
					next.points.push_back({ .range = point.range });
					next.index = std::min<size_t>(next.index, point.range.second);
				}

				/* check if new ranges have been found to be inserted
				*	as the next layer or if the end has been reached */
				if (!next.points.empty())
					layers.emplace_back(std::move(next));
				else if (open.empty())
					break;
			}

			return;
		}

	private:
		struct Node {
			size_t stamp = 0;
			size_t iteration = 0;
			bool mapped = false;
			std::set<std::pair<size_t, size_t>>::iterator source;
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

			fFinalize();

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
				std::set<std::pair<size_t, size_t>, Sorter> used;
				for (size_t iteration = 0;; ++iteration) {
					bool dirty = false;

					/* take the next step through the graph */
					for (auto it = remaining.begin(); it != remaining.end(); ++it) {
						std::pair<size_t, size_t> r = *it;

						bool firstVisit = (nodes[r.first].mapped || nodes[r.first].stamp == stamp);
						bool secondVisit = (nodes[r.second].mapped || nodes[r.second].stamp == stamp);
						if (!firstVisit && !secondVisit)
							continue;

						if (used.contains(r))
							continue;
						used.insert(r);
						dirty = true;

						if (firstVisit != secondVisit) {
							size_t next = (firstVisit ? r.second : r.first), prev = (firstVisit ? r.first : r.second);

							if (nodes[next].stamp != stamp) {
								nodes[next].stamp = stamp;
								nodes[next].iteration = iteration;
								nodes[next].source = (nodes[prev].mapped ? it : nodes[prev].source);
							}
							else if (nodes[prev].mapped && Sorter{}(*it, *nodes[next].source))
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
