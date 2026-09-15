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
/*
*	add self-loops?/double-edges?/directions?
*	update grid width to be max-width+number max(crossings) over all transitions
*
*	merge min-max-walk skip option?
*	can _Renderer algorithm introduce new crossings? (Prevent!)
*	update detail::LayoutGraph to default offset preferred positions to be centered
*
*	index/other of _Cell are currently not being updated! (remove them?)
*
*	shrink horizontally
*	shrink vertically
*	straighten up any lines
*/



namespace ascg {
	static constexpr size_t NoNode = static_cast<size_t>(-1);

	/*
	*	Describes a layered graph made up of: # | - , '
	*	The graph is bidirectional, and cannot contain self-loops.
	*	The the layouting will select an ordering of the transitions and lay it out as ascii art.
	*	The ordering of the nodes from top to bottom will not be modified and will be used as provided.
	*/
	struct Graph {
		/* separate rows of the ascii graph */
		std::vector<std::string> rows;

		/* mapping of each row to the node it belongs to with 'ascg::NoNode' for intermediate rows */
		std::vector<size_t> map;
	};


	namespace detail {
		struct MinMaxWalk {
		public:
			struct Iterator {
				size_t index = 0;
				size_t operator*() const { return index; }
				Iterator& operator++() { ++index; return *this; }
				bool operator==(Iterator other) const { return index == other.index; }
				bool operator!=(Iterator other) const { return !(*this == other); }
			};

		public:
			size_t first = 0;
			size_t last = 0;

		public:
			MinMaxWalk(size_t a, size_t b, bool skipFirst, bool skipLast) : first{ std::min(a, b) }, last{ std::max(a, b) } {
				if (skipFirst)
					++first;
				if (skipLast) {
					if (first < last)
						--last;
					else
						++first;
				}
			}
			Iterator begin() const { return Iterator{ first }; };
			Iterator end() const { return Iterator{ last + 1 }; };
		};
		inline size_t Towards(size_t value, size_t other, size_t step = 1) {
			return (value < other ? value + step : value - step);
		}

		struct Range {
			size_t first = 0;
			size_t last = 0;
		};

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

		static constexpr size_t SlotUnused = static_cast<size_t>(-1);
		enum class EntryKind : uint8_t { empty, node, traverse };
		struct Cell {
			size_t start = 0;
			size_t end = 0;
			detail::EntryKind kind = detail::EntryKind::empty;
			bool hasStart = false;
			bool hasEnd = false;
		};
		using Row = std::vector<detail::Cell>;

		class LayerGraph {
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
			LayerGraph() = default;

		private:
			void fConstructLayers(const std::vector<detail::Range>& ranges, std::vector<detail::Layer>& layers) const {
				if (ranges.empty())
					return;

				/* setup the map of all ranges to their indices (sorted by their start) */
				std::set<Opened> open;
				for (size_t i = 0; i < ranges.size(); ++i)
					open.insert(Opened{ .range = i, .start = ranges[i].first, .end = ranges[i].last });

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
						if (ranges[range].last == layer.index) {
							layer.points[0].node().end.push({ .range = range, .other = index });
							return 0;
						}
						layer.points.push_back(detail::Point{ .body = detail::Traverse{.range = range, .prev = index } });
						if (!layerNext.has_value() || ranges[range].last < *layerNext)
							layerNext = ranges[range].last;
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
			std::vector<detail::Layer> build(const std::vector<detail::Range>& ranges) const {
				std::vector<detail::Layer> layers;

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

				return layers;
			}
		};
		class Renderer {
			static constexpr size_t PreferStraightLinesScore = 2;
		private:

		public:
			Renderer() = default;

		private:
			void fMoveStart(std::vector<detail::Row>& grid, size_t index, size_t from, size_t to) const {
				detail::Cell& cell = grid[index][from];

				/* move the neighbor references themselves and patch the endpoints (other is expected to be empty) */
				if (cell.hasStart)
					grid[index + 1][cell.start].end = to;
				std::swap(grid[index][to].start, cell.start);
				std::swap(grid[index][to].hasStart, cell.hasStart);
			}
			void fMoveEnd(std::vector<detail::Row>& grid, size_t index, size_t from, size_t to) const {
				detail::Cell& cell = grid[index][from];

				/* move the neighbor references themselves and patch the endpoints (other is expected to be empty) */
				if (cell.hasEnd)
					grid[index - 1][cell.end].start = to;
				std::swap(grid[index][to].end, cell.end);
				std::swap(grid[index][to].hasEnd, cell.hasEnd);
			}
			void fSwapPositions(std::vector<detail::Row>& grid, size_t index, size_t from, size_t to) const {
				fMoveStart(grid, index, from, to);
				fMoveEnd(grid, index, from, to);

				/* move the kind over (other is expected to be empty) */
				std::swap(grid[index][to].kind, grid[index][from].kind);
			}

			void fConstructBasicGrid(const std::vector<detail::Layer>& layers, std::vector<detail::Row>& grid) const {
				std::map<size_t, size_t> map;
				size_t width = 0;

				/* construct the initial grid of plainly placed rows */
				for (const detail::Layer& layer : layers) {
					detail::Row& row = grid.emplace_back();
					row.reserve(width);

					/* iterate over the points and expand them and patch up the indices of the previous row */
					std::map<size_t, size_t> tMap;
					for (const detail::Point& point : layer.points) {
						/* place all traversing ranges */
						if (const detail::Traverse* traverse = point.isTraverse(); traverse != nullptr) {
							size_t index = row.size();
							detail::Cell& cell = row.emplace_back();

							/* configure and link up the next cell */
							cell.hasStart = true;
							cell.end = map[traverse->range];
							cell.hasEnd = true;
							cell.kind = detail::EntryKind::traverse;
							grid[grid.size() - 2][cell.end].start = index;
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
							detail::Cell& cell = row.emplace_back();

							/* configure the next cell's start */
							cell.hasStart = true;
							cell.kind = detail::EntryKind::node;
							tMap[range] = nodeIndex + intoNode++;
						}

						/* place all ending ranges sorted by their other side */
						for (size_t range : sortEndpoints(node.end)) {
							detail::Cell& cell = (nodeIndex < row.size() ? row[nodeIndex] : row.emplace_back());

							/* configure the next cell's end */
							cell.end = map[range];
							cell.hasEnd = true;
							cell.kind = detail::EntryKind::node;
							grid[grid.size() - 2][cell.end].start = nodeIndex++;
						}
					}
					map = std::move(tMap);

					/* update the expected min-width (to reduce the number of allocations per row) */
					width = std::max<size_t>(width, row.size());
				}
			}
			void fConstructGrid(const std::vector<detail::Layer>& layers, std::vector<detail::Row>& grid) const {
				fConstructBasicGrid(layers, grid);

				/* flood the entire grid to be rectangular (with one additional empty slot as swap space) */
				size_t width = 0;
				for (size_t i = 0; i < grid.size(); ++i)
					width = std::max<size_t>(width, grid[i].size() + 2);
				for (detail::Row& row : grid)
					row.resize(width);
			}
			void fLayoutRow(std::vector<detail::Row>& grid, size_t index) const {
				detail::Row& row = grid[index];

				/* helper to compute old and new score if the given cell were to be moved */
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
						detail::Cell& cell = row[i];

						/* check if this is the first node index and otherwise check if its empty and can be consumed */
						if (cell.kind != detail::EntryKind::empty) {
							if (cell.kind == detail::EntryKind::node && firstNode >= row.size())
								firstNode = i;
							continue;
						}
						size_t bestDiff = 0, bestIndex = i;

						/* iterate over the right and compute their score if they were to be moved one down */
						size_t old = 0, next = 0;
						for (size_t j = i + 1; j < row.size(); ++j) {
							if (row[j].kind == detail::EntryKind::empty) break;
							const auto [_old, _next] = computeScore(j, j - 1);
							old += _old, next += _next;

							/* check if the new move would be even more effective
							*	in reducing the score than the previous selection */
							if (next < old && (bestDiff == 0 || old - next > bestDiff) && (row[j].kind != detail::EntryKind::node || j + 1 >= row.size() || row[j + 1].kind != detail::EntryKind::node))
								bestDiff = (old - next), bestIndex = j;
						}

						/* iterate over the left and compute their score if they were to be moved one up */
						old = 0, next = 0;
						for (size_t j = i; j > 0; --j) {
							if (row[j - 1].kind == detail::EntryKind::empty) break;
							const auto [_old, _next] = computeScore(j - 1, j);
							old += _old, next += _next;

							/* check if the new move would be even more effective
							*	in reducing the score than the previous selection */
							if (next < old && (bestDiff == 0 || old - next > bestDiff) && (row[j - 1].kind != detail::EntryKind::node || j <= 1 || row[j - 2].kind != detail::EntryKind::node))
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
					for (size_t i = firstNode; i < row.size() && row[i].kind == detail::EntryKind::node; ++i) {
						bool leftNode = (i > 0 && row[i - 1].kind == detail::EntryKind::node);
						bool rightNode = (i + 1 < row.size() && row[i + 1].kind == detail::EntryKind::node);
						if (!leftNode && !rightNode)
							continue;
						detail::Cell& cell = row[i];

						/* check if the start is vacant and check if neigbors would benefit from being placed here */
						if (!cell.hasStart) {
							size_t* lOther = (leftNode && row[i - 1].hasStart ? &row[i - 1].start : nullptr);
							size_t* rOther = (rightNode && row[i + 1].hasStart ? &row[i + 1].start : nullptr);
							if (size_t from = computeNodeMove(lOther, rOther, i); from != i) {
								fMoveStart(grid, index, from, i);
								dirty = true;
							}
						}

						/* check if the end is vacant and check if neigbors would benefit from being placed here */
						if (!cell.hasEnd) {
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

			struct SwapMatch {
				size_t index = 0;
				size_t target = 0;
				size_t rotate = 0;
				bool leftRotate = false;
			};
			std::optional<SwapMatch> fFindNextSwap(std::vector<size_t>& slice) const {
				size_t bestIndex = 0, bestSkip = 0, bestTarget = 0;
				bool bestCanMove = false, bestMoveLeft = false;

				/* helper to compare moves and decide which one leaves the slice in a
				*	better state (assuming the can-move has already been decided) */
				const auto testNextOption = [&bestSkip, &bestIndex, &bestTarget, &slice](size_t skip, size_t index, size_t target) -> bool {
					if (skip != bestSkip)
						return (skip > bestSkip);
					size_t bDist = (slice[bestIndex] < bestTarget ? (bestTarget - slice[bestIndex]) : (slice[bestIndex] - bestTarget));
					size_t nDist = (slice[index] < target ? (target - slice[index]) : (slice[index] - target));
					return (nDist < bDist);
				};

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

					/* check if any of the two positions can already move */
					bool leftCanMove = (leftSkip > 0 && leftTarget > 0 && slice[leftTarget - 1] == detail::SlotUnused);
					bool rightCanMove = (rightSkip > 0 && rightTarget + 1 < slice.size() && slice[rightTarget + 1] == detail::SlotUnused);

					/* check if the left direction is better than the current best */
					if (leftSkip > 0 && ((leftCanMove && !bestCanMove) || (leftCanMove == bestCanMove && testNextOption(leftSkip, i, leftTarget)))) {
						bestIndex = i;
						bestSkip = leftSkip;
						bestTarget = leftTarget;
						bestCanMove = leftCanMove;
						bestMoveLeft = true;
					}

					/* check if the right direction is better than the current best */
					if (rightSkip > 0 && ((rightCanMove && !bestCanMove) || (rightCanMove == bestCanMove && testNextOption(rightSkip, i, rightTarget)))) {
						bestIndex = i;
						bestSkip = rightSkip;
						bestTarget = rightTarget;
						bestCanMove = rightCanMove;
						bestMoveLeft = false;
					}
				}

				/* check if no swap was found or if it can be moved immediately */
				if (bestSkip == 0)
					return std::nullopt;
				if (bestCanMove)
					return SwapMatch{ .index = bestIndex, .target = (bestMoveLeft ? bestTarget - 1 : bestTarget + 1) };

				/* shift the target for right movements, as this reduces
				*	the off-by-one patching for the upcoming loops */
				if (!bestMoveLeft)
					++bestTarget;

				/* find the next possible empty slot in either direction,
				*	and count the number of moves it would require */
				size_t leftMove = 0, rightMove = 0;
				for (size_t i = bestTarget + 1; i < slice.size(); ++i) {
					if (slice[i] != detail::SlotUnused) continue;
					rightMove = i - bestTarget;
					break;
				}
				for (size_t i = bestTarget; i > 0; --i) {
					if (slice[i - 1] != detail::SlotUnused) continue;
					leftMove = bestTarget - i;
					break;
				}

				/* configure the swap state accordingly to use the direction with least amount of rotation
				*	necessary (one must exist, as each row is guaranteed to have at least one empty spot) */
				SwapMatch output{ .index = bestIndex, .target = bestTarget };
				if ((leftMove > 0 && leftMove <= rightMove) || rightMove == 0) {
					output.rotate = leftMove;
					output.leftRotate = true;
					--output.target;
				}
				else {
					output.rotate = rightMove;
					output.leftRotate = false;
				}

				/* check if the rotation will also move the source index itself, in
				*	which case it must be updated to mark the source after the shift */
				if (output.target < output.index && !output.leftRotate && output.target + output.rotate > output.index)
					++output.index;
				else if (output.target > output.index && output.leftRotate && output.target - output.rotate < output.index)
					--output.index;
				return output;
			}
			std::vector<size_t> fMakeSlice(detail::Row& row, bool forward) const {
				std::vector<size_t> slice;
				slice.reserve(row.size());

				/* iterate over the entries of the row, and create the slice of positions
				*	representing the transitions moving away from one end of the row */
				for (size_t i = 0; i < row.size(); ++i) {
					if (forward && row[i].hasStart)
						slice.push_back(row[i].start);
					else if (!forward && row[i].hasEnd)
						slice.push_back(row[i].end);
					else
						slice.push_back(detail::SlotUnused);
				}
				return slice;
			}


			void fUpdateAlignment(std::vector<detail::Row>& grid, size_t index, bool forward) const {
				if (forward ? (index + 1 >= grid.size()) : (index == 0))
					return;
				detail::Row& row = grid[index];
				detail::Row& other = grid[forward ? index + 1 : index - 1];

				/* create the initial slice and apply all necessary swappings to the slice */
				std::vector<size_t> slice = fMakeSlice(row, forward);
				while (true) {
					/* find the next swap to be performed (if none has been found, the ordering is already correct) */
					std::optional<SwapMatch> swap = fFindNextSwap(slice);
					if (!swap.has_value())
						break;

					/* apply the rotation and the final swap-move */
					if (swap->leftRotate) for (size_t i = swap->target - swap->rotate; i < swap->target; ++i)
						std::swap(slice[i], slice[i + 1]);
					else for (size_t i = swap->target + swap->rotate; i > swap->target; --i)
						std::swap(slice[i], slice[i - 1]);
					std::swap(slice[swap->index], slice[swap->target]);
				}


				/* vvvv node-placement vvvv */
				size_t otherNodeStart = 0, otherNodeSize = 0;
				for (size_t i = 0; i < other.size(); ++i) {
					if (otherNodeSize > 0) {
						if (other[i].kind != detail::EntryKind::node)
							break;
						++otherNodeSize;
					}
					else if (other[i].kind == detail::EntryKind::node)
						otherNodeSize = 1;
					else if (forward ? other[i].hasEnd : other[i].hasStart)
						++otherNodeStart;
				}

				size_t isStart = 0, isEnd = 0;
				while (otherNodeStart > 0) {
					if (slice[isStart++] != detail::SlotUnused)
						--otherNodeStart;
				}
				isEnd = isStart;
				while (isStart > 0 && (slice[isStart - 1] == detail::SlotUnused || other[slice[isStart - 1]].kind == detail::EntryKind::node))
					--isStart;
				while (isEnd < slice.size() && (slice[isEnd] == detail::SlotUnused || other[slice[isEnd]].kind == detail::EntryKind::node))
					++isEnd;

				while (isEnd - isStart > otherNodeSize) {
					size_t leftUsed = 0, rightUsed = 0;
					while (slice[isStart + leftUsed] != detail::SlotUnused)
						++leftUsed;
					while (slice[isEnd - rightUsed - 1] != detail::SlotUnused)
						++rightUsed;

					if (leftUsed <= rightUsed) {
						for (size_t i = isStart + leftUsed; i > isStart; --i)
							std::swap(slice[i], slice[i - 1]);
						++isStart;
					}
					else {
						for (size_t i = isEnd - rightUsed; i < isEnd; ++i)
							std::swap(slice[i - 1], slice[i]);
						--isEnd;
					}
				}
				while (isEnd - isStart < otherNodeSize) {
					size_t leftMove = 0, rightMove = 0;
					for (size_t i = isStart; i > 0; --i) {
						if (slice[i - 1] != detail::SlotUnused) continue;
						leftMove = (isStart - i);
						break;
					}
					for (size_t i = isEnd; i < slice.size(); ++i) {
						if (slice[i] != detail::SlotUnused) continue;
						rightMove = (i - isEnd);
						break;
					}

					if ((leftMove > 0 && leftMove <= rightMove) || rightMove == 0) {
						for (size_t i = isStart - leftMove; i < isStart; ++i)
							std::swap(slice[i - 1], slice[i]);
						--isStart;
					}
					else {
						for (size_t i = isEnd + rightMove; i > isEnd; --i)
							std::swap(slice[i], slice[i - 1]);
						++isEnd;
					}
				}
				/* ^^^^ node-placement ^^^^ */

				/* vvvv layout-adjustments vvvv */
				for (size_t i = 0; i < slice.size(); ++i) {
					bool node = (i >= isStart && i < isEnd);
					bool used = (slice[i] != detail::SlotUnused);
					detail::EntryKind kind = (node ? detail::EntryKind::node : (used ? detail::EntryKind::traverse : detail::EntryKind::empty));

					if (other[i].kind == kind)
						continue;

					size_t source = i + 1;
					while (other[source].kind != kind)
						++source;
					while (source > i) {
						if (other[source].kind == detail::EntryKind::empty)
							fSwapPositions(grid, (forward ? index + 1 : index - 1), source - 1, source);
						else
							fSwapPositions(grid, (forward ? index + 1 : index - 1), source, source - 1);
						--source;
					}
				}
				/* ^^^^ layout-adjustments ^^^^ */
			}

			void fDrawRow(detail::Row& row, ascg::Graph& graph, size_t node) const {
				/* update the mapping and allocate the next string line (as the graph will end in a node, each
				*	node can just flush the map to the same size as the node, and the map will match in size) */
				while (graph.map.size() < graph.rows.size())
					graph.map.push_back(ascg::NoNode);
				graph.map.push_back(node);
				std::string& line = graph.rows.emplace_back();

				/* iterate over the slots of the row and draw them to the string
				*	line (with the double-extent to match the graph dimensions) */
				for (size_t i = 0; i < row.size(); ++i) {
					if (i > 0)
						line.push_back((row[i - 1].kind == detail::EntryKind::node && row[i].kind == detail::EntryKind::node) ? '#' : ' ');

					/* draw the node or connection itself */
					if (row[i].kind == detail::EntryKind::node)
						line.push_back('#');
					else if (row[i].kind == detail::EntryKind::traverse)
						line.push_back('|');
					else
						line.push_back(' ');
				}
			}
			void fDrawTransition(detail::Row& row, ascg::Graph& graph) const {
				size_t startOfLines = graph.rows.size();

				/* construct the slice to propagate the changes through */
				std::vector<size_t> slice = fMakeSlice(row, true);

				const auto addTransitions = [&slice](size_t begin, size_t end, std::string& line) -> void {
					for (size_t i = begin; i < end; ++i) {
						if (i > 0)
							line.push_back(' ');
						line.push_back(slice[i] == detail::SlotUnused ? ' ' : '|');
					}
				};

				/* add all rotations and swaps */
				while (true) {
					std::optional<SwapMatch> swap = fFindNextSwap(slice);
					if (!swap.has_value())
						break;

					size_t next = (swap->leftRotate ? (swap->target - swap->rotate) : (swap->target + swap->rotate - 1));
					for (size_t i = 0; i < swap->rotate; ++i) {
						std::string& line = graph.rows.emplace_back();

						addTransitions(0, next, line);

						if (next > 0)
							line.push_back(' ');
						line.append(swap->leftRotate ? ",-'" : "'-,");
						std::swap(slice[next], slice[next + 1]);

						addTransitions(next + 2, slice.size(), line);

						if (swap->leftRotate)
							++next;
						else
							--next;
					}

					std::string& line = graph.rows.emplace_back();
					addTransitions(0, std::min<size_t>(swap->index, swap->target), line);

					if (swap->index > 0 && swap->target > 0)
						line.push_back(' ');
					line.push_back((swap->index < swap->target) ? '\'' : ',');
					for (size_t i = std::min<size_t>(swap->index, swap->target) + 1; i < std::max<size_t>(swap->index, swap->target); ++i)
						line.append(slice[i] == detail::SlotUnused ? "--" : "-|");
					line.append((swap->index < swap->target) ? "-," : "-'");

					std::swap(slice[swap->index], slice[swap->target]);

					addTransitions(std::max<size_t>(swap->index, swap->target) + 1, slice.size(), line);
				}

				/* align all entries with their target */
				while (true) {
					/* find the next mis-placed entry, which can be moved to its new placement */
					size_t next = 0;
					for (; next < slice.size(); ++next) {
						if (slice[next] == detail::SlotUnused || slice[next] == next)
							continue;

						/* check if the entire range is usable */
						bool blocked = false;
						for (size_t i = std::min<size_t>(next, slice[next]); i <= std::max<size_t>(next, slice[next]); ++i) {
							if (i == next || slice[i] == detail::SlotUnused)
								continue;
							blocked = true;
							break;
						}
						if (!blocked)
							break;
					}

					/* check if nothing needs to be done (all slices are already well ordered) */
					if (next == slice.size()) {
						if (graph.rows.size() == startOfLines)
							addTransitions(0, slice.size(), graph.rows.emplace_back());
						break;
					}
					std::string& line = graph.rows.emplace_back();

					/* fill in the intermediate slots */
					size_t base = std::min<size_t>(next, slice[next]);
					size_t end = std::max<size_t>(next, slice[next]);
					addTransitions(0, base, line);

					/* add the move itself */
					if (base > 0)
						line.push_back(' ');
					line.push_back((base == next) ? '\'' : ',');
					for (size_t i = base + 1; i < end; ++i)
						line.append(slice[i] == detail::SlotUnused ? "--" : "-|");
					line.append((base == next) ? "-," : "-'");

					size_t other = slice[next];
					std::swap(slice[other], slice[next]);
					addTransitions(end + 1, slice.size(), line);
				}
			}

		public:
			ascg::Graph draw(const std::vector<detail::Layer>& layers) const {
				ascg::Graph graph{};

				std::vector<detail::Row> grid;

				fConstructGrid(layers, grid);

				//for (size_t i = 0; i < 40; ++i) {
				//	for (size_t j = 0; j < grid.size(); ++j)
				//		fUpdateAlignment(grid, j, true);
				//	for (size_t j = grid.size(); j > 0; --j)
				//		fUpdateAlignment(grid, j - 1, false);
				//}
				//for (size_t j = 0; j < grid.size(); ++j)
				//	fUpdateAlignment(grid, j, true);


				//for (size_t i = 0; i < 8; ++i) {
				//	for (size_t j = 0; j < grid.size(); ++j)
				//		fLayoutRow(grid, j);
				//	for (size_t j = grid.size(); j > 0; --j)
				//		fLayoutRow(grid, j - 1);
				//}

				for (size_t i = 0; i < grid.size(); ++i) {
					fDrawRow(grid[i], graph, layers[i].index);
					if (i + 1 < grid.size())
						fDrawTransition(grid[i], graph);
				}
				return graph;
			}
		};

		enum class _CellKind : uint8_t {
			empty,
			node,
			crossing,
			leftRight,
			topBottom,
			leftTop,
			leftBottom,
			rightTop,
			rightBottom
		};
		struct _Cell {
			size_t index = ascg::NoNode;
			size_t other = ascg::NoNode;
			detail::_CellKind kind = detail::_CellKind::empty;
			bool anyLeft() const {
				return (kind == detail::_CellKind::crossing || kind == detail::_CellKind::leftTop || kind == detail::_CellKind::leftRight || kind == detail::_CellKind::leftBottom);
			}
			bool cornerLeft() const {
				return (kind == detail::_CellKind::leftTop || kind == detail::_CellKind::leftBottom);
			}
			bool cornerRight() const {
				return (kind == detail::_CellKind::rightTop || kind == detail::_CellKind::rightBottom);
			}
			bool cornerTop() const {
				return (kind == detail::_CellKind::leftTop || kind == detail::_CellKind::rightTop);
			}
			bool cornerBottom() const {
				return (kind == detail::_CellKind::leftBottom || kind == detail::_CellKind::rightBottom);
			}
			bool lineHorizontal() const {
				return (kind == detail::_CellKind::crossing || kind == detail::_CellKind::leftRight);
			}
			bool lineVertical() const {
				return (kind == detail::_CellKind::crossing || kind == detail::_CellKind::topBottom);
			}
			bool freeHorizontal() const {
				return (kind == detail::_CellKind::empty || kind == detail::_CellKind::topBottom);
			}
			bool freeVertical() const {
				return (kind == detail::_CellKind::empty || kind == detail::_CellKind::leftRight);
			}
		};
		using _Row = std::vector<detail::_Cell>;

		class _Renderer {
		private:
			std::vector<size_t> fConstructNode(const std::vector<detail::Layer>& layers, size_t index, std::vector<detail::_Row>& grid) const {
				const detail::Layer& layer = layers[index];
				detail::_Row& row = grid.emplace_back();
				std::vector<size_t> slice;

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

				/* iterate over the nodes and write them out to the grid and update the slice
				*	simultaneously (initialize it with the range ids and resolve the indices later) */
				for (size_t i = 0; i < layer.points.size(); ++i) {
					const detail::Point& point = layer.points[i];

					/* check if its a simple traversal */
					if (const detail::Traverse* traverse = point.isTraverse(); traverse != nullptr) {
						row.push_back(detail::_Cell{ .index = traverse->range, .kind = detail::_CellKind::topBottom });
						slice.push_back(traverse->range);
						continue;
					}

					/* insert all of the node points to the row */
					const detail::Node& node = point.node();
					for (size_t i = std::max<size_t>(node.start.size(), node.end.size()); i > 0; --i)
						row.push_back(detail::_Cell{ .index = layer.index, .kind = detail::_CellKind::node });

					/* update the slice to contain the sorted ranges starting here and flush the remainder with empty spots */
					for (size_t range : sortEndpoints(node.start))
						slice.push_back(range);
					while (slice.size() < row.size())
						slice.push_back(ascg::NoNode);
				}

				/* check if the upcoming layer exists, in which case the ranges
				*	in the slice need to be converted to actual indices */
				if (index + 1 >= layers.size())
					return slice;
				const detail::Layer& next = layers[index + 1];

				/* setup the map of ranges to indices in the next layer */
				std::map<size_t, size_t> map;
				size_t nextIndex = 0;
				for (const detail::Point& point : next.points) {
					if (const detail::Traverse* traverse = point.isTraverse(); traverse != nullptr) {
						map[traverse->range] = nextIndex++;
						continue;
					}

					/* insert the nodes sorted by their range and skip the remaining
					*	mandatory spacing indices for ranges starting at the next layer */
					const detail::Node& node = point.node();
					for (size_t range : sortEndpoints(node.end))
						map[range] = nextIndex++;
					if (node.start.size() > node.end.size())
						nextIndex += (node.start.size() - node.end.size());
				}

				/* convert the ranges in the slice to the indices expected in the next row
				*	and ensure the slcie is large enough to hold at least the target indices */
				for (size_t& index : slice)
					index = (index == ascg::NoNode ? detail::SlotUnused : map[index]);
				while (slice.size() < nextIndex)
					slice.push_back(detail::SlotUnused);
				return slice;
			}
			void fConstructTransitions(std::vector<detail::_Row>& grid, std::vector<size_t>& slice) const {
				size_t numberOfRows = grid.size();

				/* helper method to write any basic traversals between [begin, end)
				*	out and to add rotations (begin being moved to target) */
				const auto addTraversals = [&slice, &grid](size_t begin, size_t end) -> void {
					detail::_Row& row = grid.back();

					for (size_t i = begin; i < end; ++i) {
						if (slice[i] == detail::SlotUnused)
							row.push_back(detail::_Cell{});
						else
							row.push_back(detail::_Cell{ .index = slice[i], .kind = detail::_CellKind::topBottom });
					}
				};
				const auto addRotation = [&slice, &grid, &addTraversals](size_t begin, size_t end, size_t target) -> void {
					if (begin == target) return;
					bool leftwards = (target < begin);
					size_t distance = (leftwards ? (begin - target) : (target - begin));

					/* iterate until no more entries need to be moved */
					while (begin != end) {
						detail::_Row& row = grid.emplace_back();

						/* add all traversals before the next rotation */
						addTraversals(0, (leftwards ? begin - distance : end - 1));

						/* add the next rotation itself */
						size_t range = slice[leftwards ? begin : end - 1];
						row.push_back(detail::_Cell{ .index = range, .kind = (leftwards ? detail::_CellKind::rightBottom : detail::_CellKind::rightTop) });
						for (size_t i = 1; i < distance; ++i)
							row.push_back(detail::_Cell{ .index = range, .kind = detail::_CellKind::leftRight });
						row.push_back(detail::_Cell{ .index = range, .kind = (leftwards ? detail::_CellKind::leftTop : detail::_CellKind::leftBottom) });

						/* add the traversals after the rotation and update the slice itself and the indices */
						addTraversals((leftwards ? begin + 1 : end + distance), slice.size());
						if (leftwards) {
							std::swap(slice[begin], slice[begin - distance]);
							++begin;
						}
						else {
							--end;
							std::swap(slice[end], slice[end + distance]);
						}
					}
				};
				const auto addCrossing = [&slice, &grid, &addTraversals](size_t index, size_t target) -> void {
					detail::_Row& row = grid.emplace_back();
					bool leftwards = (target < index);

					/* add the traversals before the crossing */
					addTraversals(0, std::min<size_t>(index, target));

					/* add the crossing itself */
					size_t range = slice[index];
					row.push_back(detail::_Cell{ .index = range, .kind = (leftwards ? detail::_CellKind::rightBottom : detail::_CellKind::rightTop) });
					for (size_t i : detail::MinMaxWalk{ index, target, true, true }) {
						if (slice[i] == detail::SlotUnused)
							row.push_back(detail::_Cell{ .index = range, .kind = detail::_CellKind::leftRight });
						else
							row.push_back(detail::_Cell{ .index = slice[i], .other = range, .kind = detail::_CellKind::crossing });
					}
					row.push_back(detail::_Cell{ .index = range, .kind = (leftwards ? detail::_CellKind::leftTop : detail::_CellKind::leftBottom) });

					/* add the traversals after the crossing and update the slice itself */
					addTraversals(std::max<size_t>(index, target) + 1, slice.size());
					std::swap(slice[index], slice[target]);
				};

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
						addCrossing(bestIndex, (bestMoveLeft ? bestTarget - 1 : bestTarget + 1));
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
						addRotation(bestTarget - leftMove, bestTarget, bestTarget - leftMove - 1);
						--bestTarget;
						if (bestTarget > bestIndex && bestTarget - leftMove < bestIndex)
							--bestIndex;
					}

					/* apply the rotation to the right and patch the indices accordingly (right side might require an implicit new entry) */
					else {
						if (bestTarget + rightMove == slice.size())
							slice.push_back(detail::SlotUnused);
						addRotation(bestTarget, bestTarget + rightMove, bestTarget + 1);
						if (bestTarget < bestIndex && bestTarget + rightMove > bestIndex)
							++bestIndex;
					}

					/* apply the final actual crossing */
					addCrossing(bestIndex, bestTarget);
				}

				/* find the next mis-placed entry, which can be moved to its new placement (loop until they have all been moved) */
				for (size_t i = 0; i < slice.size();) {
					if (slice[i] == detail::SlotUnused || slice[i] == i) {
						++i;
						continue;
					}

					/* check if the entire range is free for the slice slot to be rotated into place */
					bool blocked = false;
					for (size_t j : detail::MinMaxWalk{ i, slice[i], false, false }) {
						if (j == i || slice[j] == detail::SlotUnused)
							continue;
						blocked = true;
						break;
					}

					/* apply the rotation and update the indices */
					if (blocked)
						++i;
					else {
						addRotation(i, i + 1, slice[i]);
						i = 0;
					}
				}

				/* check if no more mis-placed entries have been found (all slices are already well ordered)
				*	and ensure that at least one row was added, as otherwise two nodes may directly touch */
				if (numberOfRows == grid.size())
					addTraversals(0, slice.size());
			}
			void fPrintGrid(const std::vector<detail::_Row>& grid) const {
				for (const detail::_Row& row : grid) {
					std::string line;
					for (size_t i = 0; i < row.size(); ++i) {
						const detail::_Cell& cell = row[i];

						if (i > 0) {
							if (cell.kind == detail::_CellKind::node && row[i - 1].kind == cell.kind)
								line.push_back('#');
							else if (cell.anyLeft())
								line.push_back('-');
							else
								line.push_back(' ');
						}

						if (cell.kind == detail::_CellKind::node)
							line.push_back('#');
						else if (cell.lineVertical())
							line.push_back('|');
						else if (cell.cornerTop())
							line.push_back('\'');
						else if (cell.cornerBottom())
							line.push_back(',');
						else if (cell.kind == detail::_CellKind::leftRight)
							line.push_back('-');
						else
							line.push_back(' ');
					}

					str::PrintLn('[', line, ']');
				}
			}

			bool fStraightenGraph(std::vector<detail::_Row>& grid) const {
				/* helper to remove a 'z' clipped corner rectangle */
				const auto removeCornerRectangle = [&grid](size_t x, size_t y, size_t xCorner, size_t yCorner) -> bool {
					/* check if the extension of the horizontal axis at the crossing to the new corner is free */
					for (size_t i : detail::MinMaxWalk{ x, detail::Towards(xCorner, x), false, false }) {
						if (!grid[yCorner][i].freeHorizontal())
							return false;
					}

					/* check if the extension of the original axis can freely get next to the corner */
					for (size_t i : detail::MinMaxWalk{ yCorner, detail::Towards(y, yCorner), false, false }) {
						if (!grid[i][x].freeVertical())
							return false;
					}

					/* erase the old horizontal and vertical lines and insert the new ones */
					for (size_t i : detail::MinMaxWalk{ xCorner, detail::Towards(x, xCorner), false, false }) {
						grid[y][i].kind = (grid[y][i].kind == detail::_CellKind::crossing ? detail::_CellKind::topBottom : detail::_CellKind::empty);
						grid[yCorner][i].kind = (grid[yCorner][i].kind == detail::_CellKind::topBottom ? detail::_CellKind::crossing : detail::_CellKind::leftRight);
					}
					for (size_t i : detail::MinMaxWalk{ y, yCorner, true, true }) {
						grid[i][xCorner].kind = (grid[i][xCorner].kind == detail::_CellKind::crossing ? detail::_CellKind::leftRight : detail::_CellKind::empty);
						grid[i][x].kind = (grid[i][x].kind == detail::_CellKind::leftRight ? detail::_CellKind::crossing : detail::_CellKind::topBottom);
					}

					/* move the new corner over */
					grid[yCorner][x] = grid[y][x];
					grid[y][x].kind = detail::_CellKind::topBottom;
					return true;
				};

				/* helper to move the endpoint position on a node */
				const auto moveNodeBend = [&grid](size_t x, size_t y, size_t xCorner, size_t yCorner) -> bool {
					size_t target = xCorner;

					/* count the number of empty spots by which the final line can be moved */
					while (target != x) {
						size_t next = detail::Towards(target, x);

						/* check if the entire strand is still part of the node and is free to be used */
						if (grid[yCorner][next].kind != detail::_CellKind::node)
							break;
						bool free = true;
						for (size_t i : detail::MinMaxWalk{ y, yCorner, true, true }) {
							if (!(free = grid[i][next].freeVertical()))
								break;
						}

						/* check if the entire range is free and usable and update the index */
						if (!free)
							break;
						target = next;
					}

					/* check if no actual suitable movement was found */
					if (target == xCorner)
						return false;

					/* erase the old vertical and horizontal lines and insert the new one */
					for (size_t i : detail::MinMaxWalk{ y, yCorner, true, true }) {
						grid[i][xCorner].kind = (grid[i][xCorner].kind == detail::_CellKind::crossing ? detail::_CellKind::leftRight : detail::_CellKind::empty);
						grid[i][target].kind = (grid[i][target].kind == detail::_CellKind::leftRight ? detail::_CellKind::crossing : detail::_CellKind::topBottom);
					}
					for (size_t i : detail::MinMaxWalk{ x, xCorner, true, true })
						grid[y][i].kind = (grid[y][i].kind == detail::_CellKind::crossing ? detail::_CellKind::topBottom : detail::_CellKind::empty);

					/* move the new corner over */
					grid[y][target].kind = (target == x ? detail::_CellKind::topBottom : grid[y][xCorner].kind);
					grid[y][xCorner].kind = detail::_CellKind::empty;
					return true;
				};

				/* find the next corner to pursue (iterate until one has been found or all have been checked */
				for (size_t y = 0; y < grid.size(); ++y) {
					detail::_Row& row = grid[y];
					for (size_t x = 0; x < row.size(); ++x) {
						detail::_Cell& cell = row[x];
						if (!cell.cornerLeft() && !cell.cornerRight())
							continue;

						/* ensure that the range forms the 'z' form and find the other end of the range corner */
						size_t xCorner = x, yCorner = y;
						if (cell.cornerLeft()) while (grid[yCorner][--xCorner].lineHorizontal());
						else while (grid[yCorner][++xCorner].lineHorizontal());
						if (cell.cornerTop() == grid[yCorner][xCorner].cornerTop())
							continue;
						if (cell.cornerTop()) while (grid[++yCorner][xCorner].lineVertical());
						else while (grid[--yCorner][xCorner].lineVertical());

						/* check if its a proper corner rectangle or if it reached a node and the endpoint can be moved */
						if (grid[yCorner][xCorner].kind == cell.kind && removeCornerRectangle(x, y, xCorner, yCorner))
							return true;
						else if (grid[yCorner][xCorner].kind == detail::_CellKind::node && moveNodeBend(x, y, xCorner, yCorner))
							return true;
					}
				}
				return false;
			}

			void fConstructGrid(const std::vector<detail::Layer>& layers, std::vector<detail::_Row>& grid) const {

				std::vector<size_t> slice;
				for (size_t i = 0; i < layers.size(); ++i) {
					if (i > 0)
						fConstructTransitions(grid, slice);
					slice = fConstructNode(layers, i, grid);
				}


				/* flood the entire grid to be rectangular (with one additional empty slot as swap space) */
				size_t width = 0;
				for (size_t i = 0; i < grid.size(); ++i)
					width = std::max<size_t>(width, grid[i].size());
				for (detail::_Row& row : grid)
					row.resize(width);
			}

		public:
			void draw(const std::vector<detail::Layer>& layers) const {
				std::vector<detail::_Row> grid;
				fConstructGrid(layers, grid);

				while (fStraightenGraph(grid));

				fPrintGrid(grid);
			}
		};
	}

	/* collect all edges of the graph and convert them to a final graph (ranges must not include
	*	ascg::NoNode as value and must not be identical, i.e. a connection to the same node) */
	class Builder {
	private:
		std::vector<detail::Range> pRanges;

	public:
		void connect(size_t a, size_t b) {
			/* ensure that the nodes are well ordered and considered valid */
			if (a > b)
				std::swap(a, b);
			else if (a == b)
				return;

			if (b < ascg::NoNode)
				pRanges.push_back(detail::Range{ .first = a,.last = b });
		}
		ascg::Graph finalize() const {
			std::vector<detail::Layer> layers = detail::LayerGraph{}.build(pRanges);
			detail::_Renderer{}.draw(layers);
			return detail::Renderer{}.draw(layers);
		}
	};
}
