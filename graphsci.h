#pragma once

#include <string>
#include <vector>
#include <cinttypes>
#include <algorithm>
#include <set>
#include <utility>


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
			if (a == b)
				return;
			a = std::min<size_t>(a, b), b = std::max<size_t>(a, b);

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
	};
}
