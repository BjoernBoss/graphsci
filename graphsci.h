#pragma once

#include <string>
#include <vector>
#include <cinttypes>
#include <algorithm>
#include <ranges>
#include <set>
#include <map>
#include <utility>
#include <limits>
#include <list>
#include <variant>
#include <optional>


#include <ustring/ustring.h>
/*
*	cleanup in includes
*	add self-loops?/double-edges?/directions?
*	finish layer-builder (cleanup)
*
*	only shift nodes with highest score
*	update detail::LayoutGraph to default offset preferred positions to be centered
*
*	shrink horizontally
*	shrink vertically
*	straighten up any lines
*
*	grid creation: dont account for number of crossings anymore for minimum size?
*/



namespace ascg {
	/* invalid value for normal node indices; used to identify intermediate rows in the 'ascg::Graph' */
	static constexpr size_t NoNode = std::numeric_limits<size_t>::max();

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
			MinMaxWalk(size_t a, size_t b, bool exclusive) : first{ std::min(a, b) }, last{ std::max(a, b) } {
				if (exclusive) {
					if (++first < last)
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
		inline bool Inbetween(size_t value, size_t a, size_t b) {
			if (a > b) std::swap(a, b);
			return (value >= a && value <= b);
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

		class LayerBuilder {
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

		private:
			const std::vector<detail::Range>& pRanges;
			mutable std::vector<detail::Layer> pLayers;

		public:
			LayerBuilder(const std::vector<detail::Range>& ranges) : pRanges{ ranges } {}

		private:
			void fConstructLayers() const {
				pLayers.clear();
				if (pRanges.empty())
					return;

				/* setup the map of all ranges to their indices (sorted by their start) */
				std::set<Opened> open;
				for (size_t i = 0; i < pRanges.size(); ++i)
					open.insert(Opened{ .range = i, .start = pRanges[i].first, .end = pRanges[i].last });

				/* setup the overall layers and nodes and traverse points */
				std::optional<size_t> layerNext = open.begin()->start;
				while (layerNext.has_value()) {
					/* setup the next layer with the node at index 0 */
					detail::Layer& layer = pLayers.emplace_back(detail::Layer{ .index = *layerNext });
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
					if (pLayers.size() <= 1)
						continue;

					/* helper to connect a range to the top layer and ensure proper linkage between the
					*	layers and to ensure the next assumed layer respects all currently open endings */
					const auto addRange = [this, &layer, &layerNext](size_t range, size_t index) -> size_t {
						if (pRanges[range].last == layer.index) {
							layer.points[0].node().end.push({ .range = range, .other = index });
							return 0;
						}
						layer.points.push_back(detail::Point{ .body = detail::Traverse{.range = range, .prev = index } });
						if (!layerNext.has_value() || pRanges[range].last < *layerNext)
							layerNext = pRanges[range].last;
						return layer.points.size() - 1;
					};

					/* iterate over the previous layer and extend it into the current layer */
					std::vector<detail::Point>& previous = pLayers[pLayers.size() - 2].points;
					for (size_t i = 0; i < previous.size(); ++i) {
						if (auto* traverse = previous[i].isTraverse(); traverse != nullptr)
							traverse->next = addRange(traverse->range, i);
						else for (detail::Endpoint& ep : previous[i].node().start)
							ep.other = addRange(ep.range, i);
					}
				}
			}
			void fMovePoint(detail::Point& point, size_t to, size_t index) const {
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
					updatePrevPoint(pLayers[index - 1].points[traverse->prev], traverse->range, to);
					updateNextPoint(pLayers[index + 1].points[traverse->next], traverse->range, to);
					return;
				}
				for (detail::Endpoint& ep : point.node().end)
					updatePrevPoint(pLayers[index - 1].points[ep.other], ep.range, to);
				for (detail::Endpoint& ep : point.node().start)
					updateNextPoint(pLayers[index + 1].points[ep.other], ep.range, to);
			}
			size_t fCountCrossings() const {
				size_t count = 0;

				for (size_t i = 1; i < pLayers.size(); ++i) {
					const detail::Layer& layer = pLayers[i];

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
			void fOptimizeLayer(size_t index) const {
				std::set<Preferred> preferred;
				detail::Layer& layer = pLayers[index];

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
						fMovePoint(oldPoints[it->index], next, index);
					layer.points[next++] = std::move(oldPoints[it->index]);
				}
			}

		public:
			std::vector<detail::Layer> build() const {
				fConstructLayers();
				str::PrintLn("Crossings: ", fCountCrossings());

				if (pLayers.size() > 2) for (size_t j = 0; j < 8; ++j) {
					for (size_t i = 1; i < pLayers.size(); ++i) {
						fOptimizeLayer(i);
						str::PrintLn("Crossings: ", fCountCrossings());
					}

					if (pLayers.size() < 3)
						continue;

					for (size_t i = pLayers.size() - 1; i > 0; --i) {
						fOptimizeLayer(i - 1);
						str::PrintLn("Crossings: ", fCountCrossings());
					}
				}

				std::vector<detail::Layer> layers;
				std::swap(pLayers, layers);
				return layers;
			}
		};

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

		/* temporary - should be moved after the GridBuilder */
		class _Renderer {
		private:
			const std::vector<detail::Layer>& pLayers;
			const std::vector<detail::Row>& pGrid;

		public:
			_Renderer(const std::vector<detail::Layer>& layers, const std::vector<detail::Row>& grid) : pLayers{ layers }, pGrid{ grid } {}

		private:
			std::string fDrawRow(const detail::Row& row) const {
				std::string line;

				/* iterate over the cells and convert them to the string */
				for (size_t i = 0; i < row.size(); ++i) {
					const detail::Cell& cell = row[i];

					/* add the character between the previous and current cell */
					if (i > 0) {
						if (cell.kind == detail::CellKind::node && row[i - 1].kind == cell.kind)
							line.push_back('#');
						else if (cell.anyLeft())
							line.push_back('-');
						else
							line.push_back(' ');
					}

					/* convert the cell itself to a character */
					if (cell.kind == detail::CellKind::node)
						line.push_back('#');
					else if (cell.lineVertical())
						line.push_back('|');
					else if (cell.cornerTop())
						line.push_back('\'');
					else if (cell.cornerBottom())
						line.push_back(',');
					else if (cell.kind == detail::CellKind::horizontal)
						line.push_back('-');
					else
						line.push_back(' ');
				}
				return line;
			}

		public:
			ascg::Graph draw() const {
				ascg::Graph graph;

				/* reserve the memory for the output graph */
				graph.rows.reserve(pGrid.size());
				graph.map.reserve(pGrid.size());

				/* render all of the rows and setup the map back to the layers (based on the preserved orderning of the nodes) */
				size_t node = 0;
				for (size_t i = 0; i < pGrid.size(); ++i) {
					graph.rows.push_back(fDrawRow(pGrid[i]));

					/* check if this row contained a node and otherwise mark it as an intermediate row */
					if (std::ranges::any_of(pGrid[i], [](auto val) -> bool { return val.kind == detail::CellKind::node; }))
						graph.map.push_back(pLayers[node++].index);
					else
						graph.map.push_back(ascg::NoNode);
				}
				return graph;
			}
		};

		inline void PrintGrid(const std::vector<detail::Row>& grid) {
			str::PrintLn();
			std::vector<detail::Layer> layers(grid.size());
			for (const std::string& line : detail::_Renderer{ layers, grid }.draw().rows)
				str::PrintLn('[', line, ']');
		}

		class GridBuilder {
		private:
			enum class ShiftResult : uint8_t {
				none,
				left,
				right,
				straight
			};

		private:
			const std::vector<detail::Layer>& pLayers;
			mutable std::vector<detail::Row> pGrid;

		public:
			GridBuilder(const std::vector<detail::Layer>& layers) : pLayers{ layers } {}

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
				if (pGrid[y][x].kind == detail::CellKind::empty)
					return{ x, y };

				/* check if the entire grid needs to be widened to accomodate the shifting of the affected
				*	cells (to ensure all upcoming operations can perform the shifts without checking bounds) */
				auto shiftIntoRange = std::ranges::subrange(pGrid[y].begin() + (leftwards ? 0 : x + 1), (leftwards ? pGrid[y].begin() + x : pGrid[y].end()));
				if (!std::ranges::any_of(shiftIntoRange, [](auto v) { return (v == detail::CellKind::empty); }, &detail::Cell::kind)) {
					size_t index = (leftwards ? 0 : pGrid[y].size());

					/* increase the grid accordingly and patch the index */
					for (size_t i = 0; i < pGrid.size(); ++i)
						pGrid[i].insert(pGrid[i].begin() + index, detail::Cell{});
					if (leftwards)
						++x;
				}

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
						else if (i <= x && cur[i - 1].kind == detail::CellKind::empty)
							break;
					}
					else for (size_t i = x; i < cur.size(); ++i) {
						if (slice[i] != detail::SlotUnused)
							++slice[i];
						else if (i > x && cur[i].kind == detail::CellKind::empty)
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
						if (current == detail::SlotUnused && i <= x && cur[i - 1].kind == detail::CellKind::empty)
							break;
					}
					else for (size_t i = x; i < cur.size(); ++i) {
						std::swap(current, slice[i]);
						if (current == detail::SlotUnused && i > x && cur[i].kind == detail::CellKind::empty)
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
					if (temp.kind == detail::CellKind::empty)
						break;
				}
				else for (size_t i = x; i < row.size(); ++i) {
					std::swap(temp, row[i]);
					if (temp.kind == detail::CellKind::empty)
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
				if (numberOfRows == pGrid.size())
					fFillTraversals(pGrid.emplace_back(), slice, 0, slice.size());
			}
			void fConstructGrid() const {
				pGrid.clear();
				if (pLayers.empty())
					return;

				/* compute the minimum width to give to the graph (minimum
				*	required plus maximum number of crossings between two layers) */
				size_t lastWidth = 0, totalWidth = 0;
				for (const detail::Layer& layer : pLayers) {
					size_t width = layer.points.size(), crossings = 0;

					/* helper to sum up the crossings of the current edge with the previous edges */
					const auto fetchCrossings = [&layer](size_t until, size_t target) -> size_t {
						size_t count = 0;
						for (size_t i = 0; i < until; ++i) {
							if (const detail::Traverse* traverse = layer.points[i].isTraverse(); traverse != nullptr)
								count += (traverse->prev > target ? 1 : 0);
							else for (const detail::Endpoint& ep : layer.points[i].node().end)
								count += (ep.other > target ? 1 : 0);
						}
						return count;
					};

					/* iterate over all points and update the width due to the node and compute the crossings */
					for (size_t i = 0; i < layer.points.size(); ++i) {
						if (const detail::Node* node = layer.points[i].isNode(); node != nullptr) {
							for (const detail::Endpoint& ep : node->end)
								crossings += fetchCrossings(i, ep.other);
							width += std::max<size_t>(node->start.size(), node->end.size()) - 1;
						}
						else
							crossings += fetchCrossings(i, layer.points[i].traverse().prev);
					}

					/* patch the total width to include the width plus the crossings for both this and the previous layer */
					totalWidth = std::max<size_t>({ totalWidth, width + crossings, lastWidth + crossings });
					lastWidth = width;
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
			bool fStraightenBlockedIndentRectangle(size_t x, size_t y, size_t xCorner, size_t yCorner, size_t blocker) const {

				return false;
			}
			bool fStraightenGraph() const {
				/* find the next corner to pursue (iterate until one has been found or all have been checked) */
				for (size_t y = 0; y < pGrid.size(); ++y) {
					detail::Row& row = pGrid[y];
					for (size_t x = 0; x < row.size(); ++x) {
						detail::Cell& cell = row[x];
						if (!cell.anyCorner())
							continue;

						/* find the other corner of this construct (i.e. after moving horizontal and vertical again, the endpoint) */
						size_t xOther = x, yOther = y;
						if (cell.cornerLeft()) while (pGrid[yOther][--xOther].lineHorizontal());
						else while (pGrid[yOther][++xOther].lineHorizontal());
						if (cell.cornerTop()) while (pGrid[++yOther][xOther].lineVertical());
						else while (pGrid[--yOther][xOther].lineVertical());

						/* find the end of the next horizontal axis walking towards the original horizontal position */
						bool endIsNode = (pGrid[yOther][xOther].kind == detail::CellKind::node);
						size_t xEnd = xOther;
						if (endIsNode) for (size_t i = xEnd; pGrid[yOther][i].kind == detail::CellKind::node && xEnd != x; i = detail::Towards(i, x))
							xEnd = i;
						else if (pGrid[yOther][xOther].kind != cell.kind) {
							size_t limit = (xOther < x ? row.size() : 0);
							while (pGrid[yOther][xEnd = detail::Towards(xEnd, limit)].lineHorizontal());

							/* check if the other side is larger than the current horizontal, in which case the corner [x,y]
							*	is not the primary corner, and it can be ignored until the primary corner is inspected again */
							if (!detail::Inbetween(xEnd, x, xOther))
								continue;
						}

						/* find the target axis to reduce the indentation by */
						size_t target = (endIsNode ? xEnd : x);
						while (target != xOther) {
							bool valid = true;

							/* check if the other corner is free */
							if (!endIsNode && !detail::Inbetween(target, xOther, xEnd))
								valid = (pGrid[yOther][target].kind == detail::CellKind::empty);

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
								if (!endIsNode)
									current += fCountPathCrossings(xOther, yOther, (detail::Inbetween(target, xOther, xEnd) ? target : xEnd), true, false);
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

						/* check if the end has been reached without finding a valid alternative
						*	path, in which case the current indentation cannot be reduced */
						if (target == xOther)
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
						return true;
					}
				}
				return false;
			}

		private:
			ShiftResult fShiftCheckPreference(size_t y, size_t x, bool forwards) const {
				size_t last = (forwards ? pGrid.size() - 1 : 0);

				/* check if the transition actually exists and points towards the node */
				if (y == last)
					return ShiftResult::none;
				y = detail::Towards(y, last);
				if (forwards ? !pGrid[y][x].anyTop() : !pGrid[y][x].anyBottom())
					return ShiftResult::none;

				/* follow the strand until the first shift to the left or right
				*	is encountered or until the next node has been passed */
				while (pGrid[y][x].lineVertical()) {
					if (std::ranges::any_of(pGrid[y], [](auto v) { return v == detail::CellKind::node; }, &detail::Cell::kind))
						return ShiftResult::straight;
					y = detail::Towards(y, last);
				}
				if (pGrid[y][x].cornerLeft())
					return ShiftResult::left;
				return (pGrid[y][x].cornerRight() ? ShiftResult::right : ShiftResult::straight);
			}
			bool fShiftNodes() const {
				/* find the next node and check if it should be shifted (iterate until one has been found or all have been checked;
				*	Important: two rows back-to-back must not both contain nodes, there must always exist an intermediate row) */
				for (size_t y = 0; y < pGrid.size(); ++y) {
					detail::Row& row = pGrid[y];

					/* check if the row contains a node */
					size_t first = std::ranges::find(row, detail::CellKind::node, &detail::Cell::kind) - row.begin();
					if (first >= row.size())
						continue;

					/* count the preferences of all transitions going out of the node (until the next node has been reached) */
					size_t leftShift = 0, rightShift = 0, straight = 0, last = 0;
					for (size_t i = first; (i < row.size() && row[i].kind == detail::CellKind::node); ++i) {
						last = i;
						if (ShiftResult shift = fShiftCheckPreference(y, i, false); shift != ShiftResult::none)
							++(shift == ShiftResult::straight ? straight : (shift == ShiftResult::left ? leftShift : rightShift));
						if (ShiftResult shift = fShiftCheckPreference(y, i, true); shift != ShiftResult::none)
							++(shift == ShiftResult::straight ? straight : (shift == ShiftResult::left ? leftShift : rightShift));
					}

					/* check if there is a potential beenfit in shifting the node */
					if (leftShift < 1 + rightShift + straight && rightShift < 1 + leftShift + straight)
						continue;

					/* check if the neighboring slot is free to be used */
					bool leftwards = (leftShift >= 1 + rightShift + straight);
					if (leftwards && first > 0 && row[first - 1].kind != detail::CellKind::empty)
						continue;
					else if (!leftwards && last + 1 < row.size() && row[last + 1].kind != detail::CellKind::empty)
						continue;

					/* move the node over by ensuring that the spot next to it becomes empty */
					fCreateEmptyCell((leftwards ? last : first), y, leftwards);
					return true;
				}
				return false;
			}

		private:
			void fShrinkVertical() const {
				size_t sizeReduced = 0, afterNode = 0;
				bool lastRemovable = false;

				/* iterate over the rows and remove all removable rows, while ensuring
				*	that there is at least one row between two neighboring nodes */
				for (size_t i = 0; i < pGrid.size(); ++i) {
					bool removable = true, node = false;

					/* check if the current row either is a node or fully removable */
					for (const detail::Cell& cell : pGrid[i]) {
						if (cell.kind != detail::CellKind::empty && cell.kind != detail::CellKind::vertical)
							removable = false;
						if (cell.kind == detail::CellKind::node)
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

		public:
			std::vector<detail::Row> build() const {
				fConstructGrid();

				for (size_t i = 0;; ++i) {
					while (fStraightenGraph());
					fShrinkVertical();

					if (i >= 40)
						break;

					detail::PrintGrid(pGrid);

					//if (fRotateCrossings())
					//	continue;

					/* shifting nodes requires intermediate rows between any two neighboring nodes */
					if (!fShiftNodes())
						break;
				}

				detail::PrintGrid(pGrid);


				std::vector<detail::Row> grid;
				std::swap(pGrid, grid);
				return grid;
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
				pRanges.push_back(detail::Range{ .first = a, .last = b });
		}
		ascg::Graph finalize() const {
			std::vector<detail::Layer> layers = detail::LayerBuilder{ pRanges }.build();
			std::vector<detail::Row> grid = detail::GridBuilder{ layers }.build();
			return detail::_Renderer{ layers, grid }.draw();
		}
	};
}
