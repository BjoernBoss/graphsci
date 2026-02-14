#pragma once

#include <string>
#include <vector>
#include <cinttypes>
#include <algorithm>

struct AsciiGraph {
	struct Node {
		size_t row = 0;
		size_t in = 0;
		size_t out = 0;
		bool added = false;
	};
	std::vector<std::string> rows;
	std::vector<Node> nodes;
	size_t width = 0;
};

static void EnsureWidth(AsciiGraph& g, size_t width) {
	if (g.width < width) {
		g.width = width;
		for (std::string& row : g.rows)
			row.resize(g.width, ' ');
	}
}
static void InsertRow(AsciiGraph& g, size_t index) {
	/* translate all nodes accordingly */
	for (AsciiGraph::Node& node : g.nodes) {
		if (node.row >= index)
			++node.row;
	}

	/* insert the filler line and extract the line below and above */
	std::string& line = *g.rows.emplace(g.rows.begin() + index, std::string(g.width, ' '));
	std::string_view prev{ index > 0 ? std::string_view{ g.rows[index - 1] } : std::string_view{} };
	std::string_view next{ index + 1 < g.rows.size() ? std::string_view{g.rows[index + 1]} : std::string_view{} };

	/* construct the added row such that all edges are preserved */
	for (size_t j = 0; j < prev.size(); ++j)
		if (prev[j] == '|' || prev[j] == '.') line[j] = '|';
	for (size_t j = 0; j < next.size(); ++j)
		if (next[j] == '|' || next[j] == '\'') line[j] = '|';
}
static void InsertCrossing(AsciiGraph& g, size_t row, size_t in, size_t out) {
	std::string& line = g.rows[row];

	/* add the entry and exit */
	line[in] = '\'';
	line[out] = '.';

	/* add the intermediate lines */
	size_t start = std::min<size_t>(in, out), end = std::max<size_t>(in, out);
	for (size_t i = start + 1; i < end; ++i) {
		if (line[i] == ' ')
			line[i] = '-';
	}
}
static void AddNode(AsciiGraph& g, size_t index, size_t edges) {
	/* check if new rows need to be added to the output */
	while (g.nodes.size() <= index) {
		g.nodes.push_back({ .row = g.rows.size(), .in = 0, .out = 0, .added = false });
		g.rows.emplace_back(g.width, ' ');
	}
	AsciiGraph::Node& node = g.nodes[index];

	/* check if the node has already been added */
	if (node.added)
		return;
	size_t offset = 0, length = (2 * edges - 1);

	/* find the place for the node within the row (must be axis-aligned to two characters) */
	std::string& row = g.rows[node.row];
	for (size_t match = 0;; match = 0) {
		for (; match < length; ++match) {
			if (offset + match < row.size() && row[offset + match] != ' ')
				break;
		}
		if (match >= length)
			break;
		offset = (offset + match + 2) & ~size_t(0x01);
	}
	EnsureWidth(g, offset + length);

	/* prepare the node and row */
	node.added = true;
	node.out = (node.in = offset);
	std::fill(row.begin() + offset, row.begin() + offset + length, '#');

	/* check if offset-rows need to be added above or beneath (in order to prevent nodes from clumping together) */
	for (size_t i = 0; i < 2; ++i) {
		if ((i == 0 ? node.row == 0 : node.row + 1 >= g.rows.size()))
			continue;

		/* check if the nodes touch and insert a filler-line accordingly */
		const std::string& other = g.rows[(i == 0 ? node.row - 1 : node.row + 1)];
		if (std::any_of(other.begin() + offset, other.begin() + offset + length, [](char c) { return (c == '#'); }))
			InsertRow(g, (i == 0 ? node.row : node.row + 1));
	}
}
static void ConnectNodes(AsciiGraph& g, size_t first, size_t last) {
	/* allocate the source and target columns */
	size_t current = g.nodes[first].out;
	g.nodes[first].out += 2;
	size_t target = g.nodes[last].in;
	g.nodes[last].in += 2;

	/* translate the nodes to actual rows */
	first = g.nodes[first].row;
	last = g.nodes[last].row;

	/* iterate until the nodes have been connected */
	while (first + 2 < last) {
		std::string& next = g.rows[++first];

		/* check if the path diretly above is blocked, in which case an intermediate row needs to be added */
		if (next[current] != ' ')
			InsertRow(g, last++);

		/* check if the path can just move up */
		else if (g.rows[first + 1][current] == ' ')
			next[current] = '|';
		else {
			/* check how many steps to the right are required until the top is free again */
			size_t shift = 2;
			while (current + shift < g.width && g.rows[first + 1][current + shift] != ' ') {
				if (g.rows[first][current + shift] != ' ') {
					shift -= 2;
					break;
				}
				shift += 2;
			}
			EnsureWidth(g, current + shift + 1);

			/* check if a shift can be performed, or if another empty row needs to be inserted */
			if (shift == 0)
				InsertRow(g, last++);
			else {
				InsertCrossing(g, first, current, current + shift);
				current += shift;
			}
		}
	}

	/* check if the nodes are direct row neighbors, in which case a filler-row needs to be added */
	if (++first == last)
		InsertRow(g, last++);

	/* check if a simple connecting line is enough (if between two noes, there can at most be '-'
	*	or ' ' above, as any other nodes would add spacing, or no connections could be there) */
	if (current == target) {
		g.rows[first][current] = '|';
		return;
	}

	/* check if the crossing can be performed or if another row needs to be inserted (if the row needs to
	*	be inserted, extend the current line by one, as it will otherwise be shy by one to the crossing) */
	size_t start = std::min<size_t>(current, target), finish = std::max<size_t>(current, target);
	if (std::any_of(g.rows[first].begin() + start, g.rows[first].begin() + finish, [](char c) { return (c != ' ' && c != '|'); })) {
		g.rows[first][current] = '|';
		InsertRow(g, ++first);
	}

	/* insert the crossing */
	InsertCrossing(g, first, current, target);
}
