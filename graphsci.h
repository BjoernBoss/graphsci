#pragma once

#include <string>
#include <vector>
#include <cinttypes>
#include <algorithm>
#include <set>
#include <map>
#include <utility>
#include <limits>
#include <list>


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

	namespace detail {
		struct Point {
			size_t range = 0;
			size_t bestRange = 0;
			detail::Point* prev = nullptr;
			detail::Point* next = nullptr;
			bool node = false;

			size_t placed = 0;
		};
		struct Layer {
			std::vector<detail::Point> points;
			size_t* nextCrossings = nullptr;
			size_t prevCrossings = 0;
			size_t index = 0;

			size_t width = 0;
		};

		class LayerBuilder {
		private:
			struct Opened {
			public:
				std::pair<size_t, size_t> range;
				size_t origin = 0;

			public:
				bool operator<(const Opened& other) const {
					if (range.first != other.range.first)
						return (range.first < other.range.first);
					if (range.second != other.range.second)
						return (range.second < other.range.second);
					return (origin < other.origin);
				}
			};
			struct ForwardPatch {
				int8_t order(const detail::Point& a, const detail::Point& b) const {
					if (a.prev->node && b.prev->node)
						return 0;
					return ((a.prev < b.prev) ? -1 : 1);
				}
				bool mapped(const detail::Point& pt) const {
					return (pt.prev != nullptr);
				}
				size_t crossings(const detail::Layer& l) const {
					return l.prevCrossings;
				}
			};
			struct BackwardPatch {
				int8_t order(const detail::Point& a, const detail::Point& b) const {
					if (a.next->node && b.next->node)
						return 0;
					return ((a.next < b.next) ? -1 : 1);
				}
				bool mapped(const detail::Point& pt) const {
					return (pt.next != nullptr);
				}
				size_t crossings(const detail::Layer& l) const {
					return (l.nextCrossings == nullptr ? 0 : *l.nextCrossings);
				}
			};
			struct LayoutIdentifier {
			public:
				uint64_t hash = 0;
				size_t crossings = 0;

			public:
				bool operator<(const LayoutIdentifier& other) const {
					if (hash != other.hash)
						return (hash < other.hash);
					return (crossings < other.crossings);
				}
			};
			struct LayoutEdge {
				/* [index, forward] encodes the change to be performed for this edge */
				size_t index = 0;
				bool forward = false;
				std::optional<LayoutIdentifier> target;
			};
			struct LayoutState {
				std::vector<LayoutEdge> edges;
				size_t stamp;
			};

		public:
			const std::vector<std::pair<size_t, size_t>>& pRanges;

		public:
			LayerBuilder(std::vector<std::pair<size_t, size_t>>& ranges) : pRanges{ ranges } {}

		private:
			void fSwapPoints(detail::Point& prev, detail::Point& next, detail::Layer& layer) const {
				/* update the previous and next crossing count */
				if (!prev.node || !next.node) {
					if (prev.prev != nullptr && next.prev != nullptr && (!prev.prev->node || !next.prev->node)) {
						if (prev.prev < next.prev)
							++layer.prevCrossings;
						else
							--layer.prevCrossings;
					}
					if (layer.nextCrossings != nullptr && prev.next != nullptr && next.next != nullptr && (!prev.next->node || !next.next->node)) {
						if (prev.next < next.next)
							++(*layer.nextCrossings);
						else
							--(*layer.nextCrossings);
					}
				}

				/* rotate the references to previous and upcoming points around */
				if (prev.prev != nullptr)
					prev.prev->next = &next;
				if (prev.next != nullptr)
					prev.next->prev = &next;
				if (next.prev != nullptr)
					next.prev->next = &prev;
				if (next.next != nullptr)
					next.next->prev = &prev;

				/* swap the points themselves */
				std::swap(prev.range, next.range);
				std::swap(prev.prev, next.prev);
				std::swap(prev.next, next.next);
				std::swap(prev.node, next.node);
			}
			template <class MapType>
			void fPatchLayer(detail::Layer& layer) const {
				MapType map{};

				/* iterate over all points and try to swap any neighboring points, which cross to the previous layer */
				for (size_t i = 1; i < layer.points.size(); ++i) {
					if (!map.mapped(layer.points[i]))
						continue;

					/* loop until the current point cannot be swapped with the previous point */
					size_t nextIndex = i;
					while (true) {
						if (map.crossings(layer) == 0)
							return;
						detail::Point& next = layer.points[nextIndex];

						/* find the last relevant point, which could cross the current point */
						size_t prevIndex = nextIndex;
						while (prevIndex > 0 && (!map.mapped(layer.points[prevIndex - 1]) || (next.node && layer.points[prevIndex - 1].node)))
							--prevIndex;
						if (prevIndex == 0)
							break;
						detail::Point& prev = layer.points[--prevIndex];

						/* check if neither belong to the node, in which case they can just be swapped directly */
						if (!next.node && !prev.node) {
							/* check if the points cross and can be swapped */
							if (map.order(prev, next) <= 0)
								break;

							/* cycle all points through and restart the procedure from the previous point */
							for (size_t j = prevIndex; j < nextIndex; ++j)
								fSwapPoints(layer.points[j], layer.points[j + 1], layer);
							nextIndex = prevIndex;
							continue;
						}

						/* check if the next point is part of the node, in which case the previous point is not,
						*	but would cross all points of the node (otherwise, the opposite direction holds) */
						if (next.node) {
							/* count the number of points of the node, which are mapped before and after the
							*	single point and check if a swap would reduce the overall number of crossings */
							size_t mappedBefore = 0, mappedAfter = 0, endIndex = nextIndex;
							while (true) {
								if (map.mapped(layer.points[endIndex])) {
									if (int8_t res = map.order(prev, layer.points[endIndex]); res != 0)
										++(res < 0 ? mappedAfter : mappedBefore);
								}
								++endIndex;
								if (endIndex >= layer.points.size() || !layer.points[endIndex].node)
									break;
							}
							if (mappedBefore <= mappedAfter)
								break;

							/* rotate the previous point to be placed after all points of the node and
							*	reset the index to repeat the process on the point at the new front */
							for (size_t j = prevIndex + 1; j < endIndex; ++j)
								fSwapPoints(layer.points[j - 1], layer.points[j], layer);
							nextIndex = prevIndex;
							continue;
						}

						/* count the number of points of the node, which are mapped before and after the
						*	single point and check if a swap would reduce the overall number of crossings */
						size_t mappedBefore = 0, mappedAfter = 0, firstIndex = prevIndex;
						while (true) {
							if (map.mapped(layer.points[firstIndex])) {
								if (int8_t res = map.order(layer.points[firstIndex], next); res != 0)
									++(res < 0 ? mappedBefore : mappedAfter);
							}
							if (firstIndex == 0 || !layer.points[firstIndex - 1].node)
								break;
							--firstIndex;
						}
						if (mappedBefore >= mappedAfter)
							break;

						/* rotate the next point to lie before all points of the node
						*	and reset the index accordingly to try to restart the process */
						for (size_t j = nextIndex; j > firstIndex; --j)
							fSwapPoints(layer.points[j - 1], layer.points[j], layer);
						nextIndex = firstIndex;
					}
				}
			}
			void fConstructLayers(std::vector<detail::Layer>& layers) const {
				/* setup the map of all ranges to their indices (sorted by their start) */
				std::set<Opened> open;
				for (size_t i = 0; i < pRanges.size(); ++i)
					open.insert({ pRanges[i], i });

				/* setup the overall layers and phantom points */
				while (true) {
					/* insert all upcoming nodes, which need to be added to this layer */
					if (!open.empty()) {
						auto itNext = open.begin();

						/* check if a new layer needs to be pushed */
						if (layers.empty() || layers.back().index < itNext->range.first)
							layers.push_back(detail::Layer{ .index = itNext->range.first });
						detail::Layer& layer = layers.back();

						/* insert all ranges, which start before or at the current layer */
						while (!open.empty() && itNext->range.first <= layer.index) {
							layer.points.push_back({ .range = itNext->origin });
							layer.index = std::min<size_t>(itNext->range.first, layer.index);
							itNext = open.erase(itNext);
						}
					}
					detail::Layer& layer = layers.back();

					/* iterate over the current layer and setup the extension of all open ranges into the next layer */
					detail::Layer next{ .index = std::numeric_limits<size_t>::max() };
					for (const detail::Point& point : layer.points) {
						if (pRanges[point.range].second <= layer.index)
							continue;
						next.points.push_back({ .range = point.range });
						next.index = std::min<size_t>(next.index, pRanges[point.range].second);
					}

					/* check if new ranges have been found to be inserted
					*	as the next layer or if the end has been reached */
					if (!next.points.empty())
						layers.emplace_back(std::move(next));
					else if (open.empty())
						break;
				}

				/* iterate over the layers and link the points together (after
				*	setting up all layers, as it creates references between layers) */
				std::map<size_t, size_t> rangePrevious;
				for (size_t i = 0; i < layers.size(); ++i) {
					detail::Layer& layer = layers[i];

					/* link the crossings count up */
					if (i > 0)
						layers[i - 1].nextCrossings = &layer.prevCrossings;

					/* sort the ranges in the layer to ensure ranges, which belong to this layer (i.e. start/end at
					*	this layer), are neighbors (must at least be one), and setup the point flags accordingly */
					size_t belongCount = 0;
					for (size_t j = 0; j < layer.points.size(); ++j) {
						if (pRanges[layer.points[j].range].first != layer.index && pRanges[layer.points[j].range].second != layer.index)
							continue;
						if (belongCount != j)
							std::swap(layer.points[j].range, layer.points[belongCount].range);
						layer.points[belongCount++].node = true;
					}

					/* iterate over the ranges again (after sorting, as this rotates the ranges
					*	around) and setup the mappings and references to the previous layer */
					for (size_t j = 0; j < layer.points.size(); ++j) {
						/* check if this range is part of the previous layer */
						detail::Point& point = layer.points[j];
						if (pRanges[point.range].first >= layer.index)
							continue;

						/* link the points together (references are stable, as the lists have already been
						*	populated with all points, and will not be reallocated anymore, just moved) */
						point.prev = &layers[i - 1].points[rangePrevious.find(point.range)->second];
						point.prev->next = &layer.points[j];

						/* update the crossing count by checking if this mapping crosses any previous mappings */
						if (!point.node) for (size_t k = 0; k < j; ++k) {
							detail::Point* prev = layer.points[k].prev;
							if (prev != nullptr && prev > point.prev && (!prev->node || !point.prev->node))
								++layer.prevCrossings;
						}
					}

					/* try to reduce the initial crossing count */
					fPatchLayer<ForwardPatch>(layer);

					/* iterate over the ranges again (after the initial patch, as this rotates
					*	the ranges around) and setup the initial mapping for the upcoming layer */
					rangePrevious.clear();
					for (size_t j = 0; j < layer.points.size(); ++j) {
						if (detail::Point& point = layer.points[j]; pRanges[point.range].second > layer.index)
							rangePrevious.insert({ point.range, j });
					}
				}
			}

		private:
			/* use FNV-1a hashing to hash the point order (to simplify the algorihm, states are not
			*	identified by identical graph state, but rather by identical hash and edge count) */
			static constexpr uint64_t HashInit = 0xcbf29ce484222325;
			uint64_t fHashJoin(uint64_t hash, size_t value) const {
				for (size_t i = 0; i < sizeof(value); ++i) {
					hash = (hash ^ static_cast<uint8_t>(value)) * 0x00000100000001b3;
					value >>= 8;
				}
				return hash;
			}
			void fOptimizeLayers(std::vector<detail::Layer>& layers) const {
				std::map<LayoutIdentifier, LayoutState> states;
				std::optional<size_t> bestCrossings = std::nullopt;
				LayoutEdge* edgeTaken = nullptr;
				size_t nextStamp = 0;

				/* iterate until either all states have been visited or a null-costs instance has been found */
				size_t count = 0;
				while (true) {
					/* measure the current layout state by summing up the
					*	crossings and creating the hash over all points */
					LayoutIdentifier identifier{ .hash = HashInit, .crossings = 0 };
					for (const detail::Layer& layer : layers) {
						identifier.crossings += layer.prevCrossings;
						for (const detail::Point& point : layer.points)
							identifier.hash = fHashJoin(identifier.hash, point.range);
					}

					/* check if a better result has been found and store it as the best match */
					if (!bestCrossings.has_value() || identifier.crossings < *bestCrossings) {
						bestCrossings = identifier.crossings;
						for (detail::Layer& layer : layers) {
							for (detail::Point& point : layer.points)
								point.bestRange = point.range;
						}
					}
					if (*bestCrossings == 0)
						return;

					/* write the current state back as the target of the last operation */
					if (edgeTaken != nullptr)
						edgeTaken->target = identifier;

					/* check if a new state has been discovered and extract all possible edges into the map */
					if (!states.contains(identifier)) {
						LayoutState& state = states[identifier];
						for (size_t i = 0; i < layers.size(); ++i) {
							if (layers[i].prevCrossings > 0)
								state.edges.push_back(LayoutEdge{ .index = i, .forward = true, .target = std::nullopt });
							if (layers[i].nextCrossings != nullptr && *layers[i].nextCrossings > 0)
								state.edges.push_back(LayoutEdge{ .index = i, .forward = false, .target = std::nullopt });
						}
					}

					/* initialize the queue and allocate the next state-stamp to be used */
					std::list<std::pair<LayoutIdentifier, LayoutEdge*>> queue{ { identifier, nullptr } };
					size_t stamp = (states[identifier].stamp = ++nextStamp);
					edgeTaken = nullptr;

					/* find the next closest edge, which leads to an undiscovered target */
					while (!queue.empty() && edgeTaken == nullptr) {
						LayoutState& state = states[queue.front().first];
						LayoutEdge* source = queue.front().second;
						queue.pop_front();

						/* iterate over the edges in the top-state and check if one of them has not yet been evaluated */
						for (LayoutEdge& edge : state.edges) {
							if (!edge.target.has_value()) {
								edgeTaken = (source == nullptr ? &edge : source);
								break;
							}

							LayoutState& target = states[*edge.target];
							if (target.stamp >= stamp)
								continue;
							target.stamp = stamp;
							queue.push_back({ *edge.target, (source == nullptr ? &edge : source) });
						}
					}

					/* check if the entire graph has been fully searched (in which case the best ranges
					*	will have been written to the points) and otherwise apply the operation */
					if (edgeTaken == nullptr)
						return;
					if (edgeTaken->forward)
						fPatchLayer<ForwardPatch>(layers[edgeTaken->index]);
					else
						fPatchLayer<BackwardPatch>(layers[edgeTaken->index]);
				}
			}

		public:
			void build(std::vector<detail::Layer>& layers) {
				layers.clear();
				fConstructLayers(layers);
				fOptimizeLayers(layers);
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

	public:
		void _finalize() {
			std::vector<detail::Layer> layers;
			detail::LayerBuilder{ pRanges }.build(layers);
		}
	};
}
