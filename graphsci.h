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
#include <variant>
#include <optional>


#include <ustring/ustring.h>

namespace ascg {
	namespace detail {
		struct _Node {
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
		std::vector<detail::_Node> pNodes;
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
			for (detail::_Node& node : pNodes) {
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
			detail::_Node& node = pNodes[index];
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
			detail::_Node& node = pNodes[index];

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

	namespace detail {
		struct Endpoint {
			size_t range = 0;
			size_t other = 0;
		};
		struct Endpoints {
		private:
			union {
				detail::Endpoint value{};
				detail::Endpoint* list;
			} pValue;
			size_t pSize = 0;

		private:
			static constexpr size_t fCapacity(size_t size) {
				size_t capacity = 2;
				while (capacity < size)
					capacity <<= 1;
				return capacity;
			}

		public:
			Endpoints() = default;
			Endpoints(const detail::Endpoints& other) {
				if ((pSize = other.pSize) <= 1)
					pValue.value = other.pValue.value;
				else {
					pValue.list = new detail::Endpoint[fCapacity(pSize)];
					std::copy(other.pValue.list, other.pValue.list + pSize, pValue.list);
				}
			}
			Endpoints(detail::Endpoints&& other) noexcept {
				if (other.pSize > 1)
					std::swap(pValue.list, other.pValue.list);
				else
					std::swap(pValue.value, other.pValue.value);
				std::swap(pSize, other.pSize);
			}
			detail::Endpoints& operator=(const detail::Endpoints& other) {
				if (other.pSize <= 1) {
					if (pSize > 1)
						delete[] pValue.list;
					pValue.value = other.pValue.value;
				}
				else {
					size_t cOther = fCapacity(other.pSize);
					if (pSize <= 1 || fCapacity(pSize) != cOther) {
						if (pSize > 1)
							delete[] pValue.list;
						pValue.list = new detail::Endpoint[cOther];
					}
					std::copy(other.pValue.list, other.pValue.list + other.pSize, pValue.list);
				}
				pSize = other.pSize;
				return *this;
			}
			detail::Endpoints& operator=(detail::Endpoints&& other) noexcept {
				if (other.pSize > 1)
					std::swap(pValue.list, other.pValue.list);
				else
					std::swap(pValue.value, other.pValue.value);
				std::swap(pSize, other.pSize);
				return *this;
			}
			~Endpoints() {
				if (pSize > 1)
					free(pValue.list);
			}

		public:
			void push(detail::Endpoint value) {
				if (pSize == 0) {
					pValue.value = value;
					++pSize;
					return;
				}

				size_t cNext = fCapacity(pSize + 1);
				if (pSize == 1 || fCapacity(pSize) != cNext) {
					detail::Endpoint* list = new detail::Endpoint[cNext];
					if (pSize > 1) {
						std::copy(pValue.list, pValue.list + pSize, list);
						delete[] pValue.list;
					}
					else
						list[0] = pValue.value;
					pValue.list = list;
				}
				pValue.list[pSize++] = value;
			}
			const detail::Endpoint* begin() const {
				return (pSize == 1 ? &pValue.value : pValue.list);
			}
			detail::Endpoint* begin() {
				return (pSize == 1 ? &pValue.value : pValue.list);
			}
			const detail::Endpoint* end() const {
				return (pSize == 1 ? &pValue.value : pValue.list) + pSize;
			}
			detail::Endpoint* end() {
				return (pSize == 1 ? &pValue.value : pValue.list) + pSize;
			}
			size_t size() const {
				return pSize;
			}
		};
		struct Traverse {
			size_t range = 0;
			size_t prev = 0;
			size_t next = 0;
		};
		struct Node {
			detail::Endpoints end{};
			detail::Endpoints start{};
		};
		struct Point {
			std::variant<detail::Traverse, detail::Node> body;
			const detail::Node* isNode() const {
				return (std::holds_alternative<detail::Node>(body) ? &node() : nullptr);
			}
			const detail::Traverse* isTraverse() const {
				return (std::holds_alternative<detail::Traverse>(body) ? &traverse() : nullptr);
			}
			const detail::Traverse& traverse() const {
				return std::get<detail::Traverse>(body);
			}
			const detail::Node& node() const {
				return std::get<detail::Node>(body);
			}
			detail::Node* isNode() {
				return (std::holds_alternative<detail::Node>(body) ? &node() : nullptr);
			}
			detail::Traverse* isTraverse() {
				return (std::holds_alternative<detail::Traverse>(body) ? &traverse() : nullptr);
			}
			detail::Traverse& traverse() {
				return std::get<detail::Traverse>(body);
			}
			detail::Node& node() {
				return std::get<detail::Node>(body);
			}
		};
		struct Layer {
			std::vector<detail::Point> points;
			size_t index = 0;
		};

		class Builder {
		private:
			struct Opened {
				size_t range = 0;
				size_t start = 0;
				size_t end = 0;
				bool operator<(const Opened& other) const {
					if (start != other.start)
						return (start < other.start);
					if (end != other.end)
						return (end < other.end);
					return (range < other.range);
				}
			};
			struct Preferred {
				float average = 0.0f;
				size_t index = 0;
				bool operator<(const Preferred& other) const {
					if (average != other.average)
						return (average < other.average);
					return (index < other.index);
				}
			};

		public:
			Builder() = default;

		private:
			void fConstructLayers(const std::vector<std::pair<size_t, size_t>>& ranges, std::vector<detail::Layer>& layers) const {
				if (ranges.empty())
					return;

				/* setup the map of all ranges to their indices (sorted by their start) */
				std::set<Opened> open;
				for (size_t i = 0; i < ranges.size(); ++i)
					open.insert(Opened{ .range = i, .start = ranges[i].first, .end = ranges[i].second });

				/* setup the overall layers and nodes and traverse points */
				std::optional<size_t> layerNext = open.begin()->start;
				while (layerNext.has_value()) {
					/* setup the next layer with the node at index 0 */
					detail::Layer& layer = layers.emplace_back(detail::Layer{ .index = *layerNext });
					layer.points.push_back(detail::Point{ .body = detail::Node{} });
					layerNext = std::nullopt;

					/* insert all ranges which start at the current layer and update the next layer index */
					auto itNext = open.begin();
					while (!open.empty() && itNext->start == layer.index) {
						layer.points[0].node().start.push({ .range = itNext->range });
						if (!layerNext.has_value() || itNext->end < *layerNext)
							layerNext = itNext->end;
						itNext = open.erase(itNext);
					}

					/* check if the start of the next layer is lower than any end of the inserted ranges */
					if (!open.empty() && (!layerNext.has_value() || itNext->start < *layerNext))
						layerNext = itNext->start;

					/* check if the previous layer exists in order to be linked up */
					if (layers.size() <= 1)
						continue;

					/* helper to connect a range to the top layer and ensure proper linkage between the
					*	layers and to ensure the next assumed layer respects all currently open endings */
					const auto addRange = [&ranges, &layer, &layerNext](size_t range, size_t index) -> size_t {
						if (ranges[range].second == layer.index) {
							layer.points[0].node().end.push({ .range = range, .other = index });
							return 0;
						}
						layer.points.push_back(detail::Point{ .body = detail::Traverse{.range = range, .prev = index } });
						if (!layerNext.has_value() || ranges[range].second < *layerNext)
							layerNext = ranges[range].second;
						return layer.points.size() - 1;
					};

					/* iterate over the previous layer and extend it into the current layer */
					auto& previous = layers[layers.size() - 2].points;
					for (size_t i = 0; i < previous.size(); ++i) {
						if (auto* traverse = previous[i].isTraverse(); traverse != nullptr)
							traverse->next = addRange(traverse->range, i);
						else for (detail::Endpoint& ep : previous[i].node().start)
							ep.other = addRange(ep.range, i);
					}
				}
			}
			void fMovePoint(detail::Point& point, size_t to, std::vector<detail::Layer>& layers, size_t index) const {
				/* helper functions to patch the points in the neighboring layers */
				const auto updatePrevPoint = [](detail::Point& point, size_t range, size_t to) {
					if (detail::Traverse* traverse = point.isTraverse(); traverse != nullptr)
						traverse->next = to;
					else for (detail::Endpoint& ep : point.node().start) {
						if (ep.range != range) continue;
						ep.other = to;
						break;
					}
				};
				const auto updateNextPoint = [](detail::Point& point, size_t range, size_t to) {
					if (detail::Traverse* traverse = point.isTraverse(); traverse != nullptr)
						traverse->prev = to;
					else for (detail::Endpoint& ep : point.node().end) {
						if (ep.range != range) continue;
						ep.other = to;
						break;
					}
				};

				/* update the referenced points of the moved point */
				if (detail::Traverse* traverse = point.isTraverse(); traverse != nullptr) {
					updatePrevPoint(layers[index - 1].points[traverse->prev], traverse->range, to);
					updateNextPoint(layers[index + 1].points[traverse->next], traverse->range, to);
					return;
				}
				for (detail::Endpoint& ep : point.node().end)
					updatePrevPoint(layers[index - 1].points[ep.other], ep.range, to);
				for (detail::Endpoint& ep : point.node().start)
					updateNextPoint(layers[index + 1].points[ep.other], ep.range, to);
			}
			size_t fCountCrossings(std::vector<detail::Layer>& layers) const {
				size_t count = 0;

				for (size_t i = 1; i < layers.size(); ++i) {
					const detail::Layer& layer = layers[i];

					for (size_t j = 0; j < layer.points.size(); ++j) {
						const detail::Point& point = layer.points[j];
						const auto testAgainstPoint = [&point, &count](size_t index) -> void {
							if (const detail::Traverse* traverse = point.isTraverse(); traverse != nullptr) {
								if (index < traverse->prev)
									++count;
								return;
							}
							for (const detail::Endpoint& ep : point.node().end) {
								if (index < ep.other)
									++count;
							}
						};

						for (size_t k = j + 1; k < layer.points.size(); ++k) {
							const detail::Point& other = layer.points[k];
							if (const detail::Traverse* traverse = other.isTraverse(); traverse != nullptr)
								testAgainstPoint(traverse->prev);
							else for (const detail::Endpoint& ep : other.node().end)
								testAgainstPoint(ep.other);
						}
					}
				}
				return count;
			}
			void fOptimizeLayer(std::vector<detail::Layer>& layers, size_t index) const {
				std::set<Preferred> preferred;
				detail::Layer& layer = layers[index];

				/* calculate the average position of each point based on its connected neighbors,
				*	and sort the points new by this given target position (barycenter method) */
				bool changed = false;
				for (size_t i = 0; i < layer.points.size(); ++i) {
					float average = 0.0f;

					const detail::Point& pt = layer.points[i];
					if (const detail::Traverse* traverse = pt.isTraverse(); traverse != nullptr)
						average = static_cast<float>(traverse->prev + traverse->next) / 2.0f;
					else {
						const detail::Node& node = pt.node();
						for (const detail::Endpoint& ep : node.start)
							average += static_cast<float>(ep.other);
						for (const detail::Endpoint& ep : node.end)
							average += static_cast<float>(ep.other);
						average /= static_cast<float>(node.start.size() + node.end.size());
					}

					std::set<Preferred>::iterator it = preferred.insert(Preferred{ .average = average, .index = i }).first;
					if (++it != preferred.end())
						changed = true;
				}
				if (!changed)
					return;

				/* construct the new layer based on the new indices */
				std::vector<detail::Point> oldPoints{ layer.points.size(), detail::Point{} };
				std::swap(oldPoints, layer.points);
				size_t next = 0;
				for (auto it = preferred.begin(); it != preferred.end(); ++it) {
					if (it->index != next)
						fMovePoint(oldPoints[it->index], next, layers, index);
					layer.points[next++] = std::move(oldPoints[it->index]);
				}
			}

		public:
			void build(const std::vector<std::pair<size_t, size_t>>& ranges, std::vector<detail::Layer>& layers) const {
				layers.clear();
				fConstructLayers(ranges, layers);
				str::PrintLn("Crossings: ", fCountCrossings(layers));

				if (layers.size() > 2) for (size_t j = 0; j < 8; ++j) {
					for (size_t i = 1; i < layers.size(); ++i) {
						fOptimizeLayer(layers, i);
						str::PrintLn("Crossings: ", fCountCrossings(layers));
					}

					if (layers.size() < 3)
						continue;

					for (size_t i = layers.size() - 1; i > 0; --i) {
						fOptimizeLayer(layers, i - 1);
						str::PrintLn("Crossings: ", fCountCrossings(layers));
					}
				}
			}
		};
		class Renderer {
			static constexpr size_t PreferStraightLinesScore = 2;
		private:
			enum class EntryKind : uint8_t { empty, node, traverse, blocked };
			struct Position {
				size_t start = 0;
				size_t end = 0;
				size_t ordering = 0;
				EntryKind kind = EntryKind::empty;
				bool hasStart = false;
				bool hasEnd = false;
			};
			using Row = std::vector<Position>;

		public:
			Renderer() = default;

		private:
			void fMoveStart(std::vector<Row>& grid, size_t index, size_t from, size_t to) const {
				Position& pos = grid[index][from];

				/* move the neighbor references themselves and patch the endpoints (other is expected to be empty) */
				if (pos.hasStart)
					grid[index + 1][pos.start].end = to;
				std::swap(grid[index][to].start, pos.start);
				std::swap(grid[index][to].hasStart, pos.hasStart);
			}
			void fMoveEnd(std::vector<Row>& grid, size_t index, size_t from, size_t to) const {
				Position& pos = grid[index][from];

				/* move the neighbor references themselves and patch the endpoints (other is expected to be empty) */
				if (pos.hasEnd)
					grid[index - 1][pos.end].start = to;
				std::swap(grid[index][to].end, pos.end);
				std::swap(grid[index][to].hasEnd, pos.hasEnd);
			}
			void fSwapPositions(std::vector<Row>& grid, size_t index, size_t from, size_t to) const {
				fMoveStart(grid, index, from, to);
				fMoveEnd(grid, index, from, to);

				/* move the kind over (other is expected to be empty) */
				std::swap(grid[index][to].kind, grid[index][from].kind);
			}

			void fConstructBasicGrid(const std::vector<detail::Layer>& layers, std::vector<Row>& grid) const {
				std::map<size_t, size_t> map;
				size_t width = 0;

				/* construct the initial grid of plainly placed rows */
				for (const detail::Layer& layer : layers) {
					Row& row = grid.emplace_back();
					row.reserve(width);

					/* iterate over the points and expand them and patch up the indices of the previous row */
					std::map<size_t, size_t> tMap;
					for (const detail::Point& point : layer.points) {
						/* place all traversing ranges */
						if (const detail::Traverse* traverse = point.isTraverse(); traverse != nullptr) {
							size_t index = row.size();
							Position& pos = row.emplace_back();

							/* configure and link up the next position */
							pos.hasStart = true;
							pos.end = map[traverse->range];
							pos.hasEnd = true;
							pos.kind = EntryKind::traverse;
							grid[grid.size() - 2][pos.end].start = index;
							tMap[traverse->range] = index;
							continue;
						}

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

						/* place all starting ranges sorted by their other side */
						const detail::Node& node = point.node();
						size_t nodeIndex = row.size(), intoNode = 0;
						for (size_t range : sortEndpoints(node.start)) {
							Position& pos = row.emplace_back();

							/* configure the next position's start */
							pos.hasStart = true;
							pos.kind = EntryKind::node;
							tMap[range] = nodeIndex + intoNode++;
						}

						/* place all ending ranges sorted by their other side */
						for (size_t range : sortEndpoints(node.end)) {
							Position& pos = (nodeIndex < row.size() ? row[nodeIndex] : row.emplace_back());

							/* configure the next position's end */
							pos.end = map[range];
							pos.hasEnd = true;
							pos.kind = EntryKind::node;
							grid[grid.size() - 2][pos.end].start = nodeIndex++;
						}
					}
					map = std::move(tMap);

					/* update the expected min-width (to reduce the number of allocations per row) */
					width = std::max<size_t>(width, row.size());
				}
			}
			void fOrderCrossings(std::vector<Row>& grid, size_t index) const {
				Row& row = grid[index];
				std::vector<size_t> crossings(row.size(), 0);

				/* iterate over the row and count the number of crossings between the current row and the previous row */
				size_t totalCrossings = 0;
				for (size_t i = 1; i < row.size(); ++i) {
					if (!row[i].hasEnd)
						continue;
					for (size_t j = 0; j < i; ++j) {
						if (!row[j].hasEnd || row[j].end <= row[i].end)
							continue;
						++crossings[i]; ++crossings[j];
						++totalCrossings;
					}
				}
				if (totalCrossings == 0)
					return;
				Row& prev = grid[index - 1];

				/* iterate over the crossings and apply them largest to lowest */
				size_t ordering = 0;
				while (totalCrossings > 0) {
					/* look for the next crossing to resolve next, which crosses the largest number of other lines */
					size_t next = 0;
					for (size_t i = 1; i < crossings.size(); ++i)
						next = (crossings[next] < crossings[i] ? i : next);

					/* clear the crossings of the selected entry */
					totalCrossings -= crossings[next];
					crossings[next] = 0;
					for (size_t i = 0; i < row.size(); ++i) {
						if (!row[i].hasEnd) continue;
						size_t iOther = row[i].end, nOther = row[next].end;
						if ((i < next) ? iOther > nOther : iOther < nOther)
							--crossings[i];
					}

					/* check if an empty block needs to be inserted */
					if (row[row[next].end].kind != EntryKind::blocked) {
						row.push_back(Position{});
						for (size_t i = row.size() - 1; i > next; --i)
							fSwapPositions(grid, index, i - 1, i);
						row[next].kind = EntryKind::blocked;
					}
					if (prev[next].kind != EntryKind::blocked) {
						prev.push_back(Position{});
						for (size_t i = prev.size() - 1; i > next; --i)
							fSwapPositions(grid, index - 1, i - 1, i);
						prev[next].kind = EntryKind::blocked;
					}
				}
			}
			void fConstructGrid(const std::vector<detail::Layer>& layers, std::vector<Row>& grid) const {
				fConstructBasicGrid(layers, grid);

				/* order the crossings and insert the necessary breaks */
				for (size_t i = 0; i < grid.size(); ++i)
					fOrderCrossings(grid, i);

				/* flood the entire grid to be rectangular */
				size_t width = 0;
				for (size_t i = 0; i < grid.size(); ++i)
					width = std::max<size_t>(width, grid[i].size());
				for (Row& row : grid)
					row.resize(width);
			}
			void fLayoutRow(std::vector<Row>& grid, size_t index) const {
				Row& row = grid[index];

				/* helper to compute old and new score if the given position were to be moved */
				const auto computeScore = [&row](size_t oldIndex, size_t nextIndex) -> std::pair<size_t, size_t> {
					const auto distance = [](size_t s, size_t t) -> size_t { return (s < t ? (t - s) : (s - t)); };
					const auto pow2 = [](size_t v) -> size_t { return v * v; };

					size_t sOld = 0, eOld = 0, sNext = 0, eNext = 0;
					if (row[oldIndex].hasStart) {
						sOld += pow2(distance(oldIndex, row[oldIndex].start));
						sNext += pow2(distance(nextIndex, row[oldIndex].start));
						if (sOld == 0 || sNext == 0)
							(sOld == 0 ? sNext : sOld) += PreferStraightLinesScore;
					}
					if (row[oldIndex].hasEnd) {
						eOld += pow2(distance(oldIndex, row[oldIndex].end));
						eNext += pow2(distance(nextIndex, row[oldIndex].end));
						if (eOld == 0 || eNext == 0)
							(eOld == 0 ? eNext : eOld) += PreferStraightLinesScore;
					}
					return { sOld + eOld, sNext + eNext };
				};

				/* helper to determine which endpoint to move into the node slot */
				const auto computeNodeMove = [&row](size_t* lOther, size_t* rOther, size_t index) -> size_t {
					size_t left = ((lOther != nullptr && *lOther >= index) ? (*lOther - (index - 1)) : 0);
					size_t right = ((rOther != nullptr && *rOther <= index) ? ((index + 1) - *rOther) : 0);

					/* check which side would benefit more from being moved into the empty spot */
					if (left == 0 && right == 0)
						return index;
					if (left == right)
						(index * 2 <= row.size() ? left : right) = 0;
					return (left > right ? index - 1 : index + 1);
				};

				/* iterate until no more changes are applied to the row */
				bool dirty = true;
				while (dirty) {
					dirty = false;

					/* look for empty slots and check if it should be filled up from left or right */
					size_t firstNode = row.size();
					for (size_t i = 0; i < row.size(); ++i) {
						Position& pos = row[i];

						/* check if this is the first node index and otherwise check if its empty and can be consumed */
						if (pos.kind != EntryKind::empty) {
							if (pos.kind == EntryKind::node && firstNode >= row.size())
								firstNode = i;
							continue;
						}
						size_t bestDiff = 0, bestIndex = i;

						/* iterate over the right and compute their score if they were to be moved one down */
						size_t old = 0, next = 0;
						for (size_t j = i + 1; j < row.size(); ++j) {
							if (row[j].kind == EntryKind::empty) break;
							const auto [_old, _next] = computeScore(j, j - 1);
							old += _old, next += _next;

							/* check if the new move would be even more effective
							*	in reducing the score than the previous selection */
							if (next < old && (bestDiff == 0 || old - next > bestDiff) && (row[j].kind != EntryKind::node || j + 1 >= row.size() || row[j + 1].kind != EntryKind::node))
								bestDiff = (old - next), bestIndex = j;
						}

						/* iterate over the left and compute their score if they were to be moved one up */
						old = 0, next = 0;
						for (size_t j = i; j > 0; --j) {
							if (row[j - 1].kind == EntryKind::empty) break;
							const auto [_old, _next] = computeScore(j - 1, j);
							old += _old, next += _next;

							/* check if the new move would be even more effective
							*	in reducing the score than the previous selection */
							if (next < old && (bestDiff == 0 || old - next > bestDiff) && (row[j - 1].kind != EntryKind::node || j <= 1 || row[j - 2].kind != EntryKind::node))
								bestDiff = (old - next), bestIndex = j - 1;
						}

						/* check if a move was selected to fill the next spot with */
						if (bestDiff == 0)
							continue;

						/* rotate the new points into position */
						if (bestIndex < i) for (size_t j = i; j > bestIndex; --j)
							fSwapPositions(grid, index, j - 1, j);
						else for (size_t j = i + 1; j <= bestIndex; ++j)
							fSwapPositions(grid, index, j, j - 1);
						dirty = true;
						break;
					}
					if (dirty)
						continue;

					/* check if there is a vacant spot on the node, which can be filled by another
					*	connection (no need to check any contiguous neighbors, as the node outputs are
					*	sorted, hence if the neighbors profit from moving, so does the first entry) */
					for (size_t i = firstNode; i < row.size() && row[i].kind == EntryKind::node; ++i) {
						bool leftNode = (i > 0 && row[i - 1].kind == EntryKind::node);
						bool rightNode = (i + 1 < row.size() && row[i + 1].kind == EntryKind::node);
						if (!leftNode && !rightNode)
							continue;
						Position& pos = row[i];

						/* check if the start is vacant and check if neigbors would benefit from being placed here */
						if (!pos.hasStart) {
							size_t* lOther = (leftNode && row[i - 1].hasStart ? &row[i - 1].start : nullptr);
							size_t* rOther = (rightNode && row[i + 1].hasStart ? &row[i + 1].start : nullptr);
							if (size_t from = computeNodeMove(lOther, rOther, i); from != i) {
								fMoveStart(grid, index, from, i);
								dirty = true;
							}
						}

						/* check if the end is vacant and check if neigbors would benefit from being placed here */
						if (!pos.hasEnd) {
							size_t* lOther = (leftNode && row[i - 1].hasEnd ? &row[i - 1].end : nullptr);
							size_t* rOther = (rightNode && row[i + 1].hasEnd ? &row[i + 1].end : nullptr);
							if (size_t from = computeNodeMove(lOther, rOther, i); from != i) {
								fMoveEnd(grid, index, from, i);
								dirty = true;
							}
						}

						if (dirty)
							break;
					}
				}
			}

		public:
			void draw(const std::vector<detail::Layer>& layers) const {
				std::vector<Row> grid;

				fConstructGrid(layers, grid);

				for (size_t i = 0; i < 8; ++i) {
					for (size_t j = 0; j < grid.size(); ++j)
						fLayoutRow(grid, j);
					for (size_t j = grid.size(); j > 0; --j)
						fLayoutRow(grid, j - 1);
				}

				for (size_t i = 0; i < grid.size(); ++i) {
					const Row& row = grid[i];

					std::string start = "", line = "", end = "";
					for (size_t j = 0; j < row.size(); ++j) {
						if (row[j].hasStart && row[j].start != j)
							start.push_back((row[j].start < j) ? '<' : '>');
						else
							start.push_back(row[j].hasStart ? '\'' : ' ');

						if (row[j].hasEnd && row[j].end != j)
							end.push_back((row[j].end < j) ? '<' : '>');
						else
							end.push_back(row[j].hasEnd ? ',' : ' ');

						if (row[j].kind == EntryKind::node)
							line.push_back('#');
						else if (row[j].kind == EntryKind::blocked)
							line.push_back('X');
						else
							line.push_back(row[j].kind == EntryKind::traverse ? '|' : ' ');
					}
					str::PrintLn(end, '\n', line, '\n', start);
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

	public:
		void _finalize() const {
			std::vector<detail::Layer> temp;
			detail::Builder{}.build(pRanges, temp);
			detail::Renderer{}.draw(temp);
		}
	};
}
