#pragma once

#include <string>
#include <vector>
#include <cinttypes>
#include <algorithm>

namespace ascp {
	namespace detail {
		struct Node {
			size_t row = 0;
			size_t in = 0;
			size_t out = 0;
			bool added = false;
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

			/* translate the nodes to actual rows */
			first = pNodes[first].row;
			last = pNodes[last].row;

			/* iterate until the nodes have been connected */
			while (first + 2 < last) {
				std::string& next = pRows[++first];

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
						if (pRows[first][current + shift] != ' ') {
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
			}

			/* check if the nodes are direct row neighbors, in which case a filler-row needs to be added */
			if (++first == last)
				fInsertRow(last++);

			/* check if a simple connecting line is enough (if between two noes, there can at most be '-'
			*	or ' ' above, as any other nodes would add spacing, or no connections could be there) */
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
		ascp::Graph finalize() {
			ascp::Graph out{ .rows = pRows };
			for (size_t i = 0; i < pNodes.size(); ++i)
				out.map.push_back(pNodes[i].row);
			return out;
		}
	};

}
