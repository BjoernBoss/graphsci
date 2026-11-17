/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026 Bjoern Boss Henrichsen */
#pragma once

#include "ascg-common.h"

namespace ascg::detail {
	template <class Type>
	struct SmallList {
	private:
		union {
			Type value{};
			Type* list;
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
		SmallList() = default;
		SmallList(const detail::SmallList<Type>& other) {
			if ((pSize = other.pSize) <= 1)
				pValue.value = other.pValue.value;
			else {
				pValue.list = new Type[fCapacity(pSize)];
				std::copy(other.pValue.list, other.pValue.list + pSize, pValue.list);
			}
		}
		SmallList(detail::SmallList<Type>&& other) noexcept {
			if (other.pSize > 1)
				std::swap(pValue.list, other.pValue.list);
			else
				std::swap(pValue.value, other.pValue.value);
			std::swap(pSize, other.pSize);
		}
		detail::SmallList<Type>& operator=(const detail::SmallList<Type>& other) {
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
					pValue.list = new Type[cOther];
				}
				std::copy(other.pValue.list, other.pValue.list + other.pSize, pValue.list);
			}
			pSize = other.pSize;
			return *this;
		}
		detail::SmallList<Type>& operator=(detail::SmallList<Type>&& other) noexcept {
			if (other.pSize > 1)
				std::swap(pValue.list, other.pValue.list);
			else
				std::swap(pValue.value, other.pValue.value);
			std::swap(pSize, other.pSize);
			return *this;
		}
		~SmallList() {
			if (pSize > 1)
				delete[] pValue.list;
		}

	public:
		void push(const Type& value) {
			if (pSize == 0) {
				pValue.value = value;
				++pSize;
				return;
			}

			size_t cNext = fCapacity(pSize + 1);
			if (pSize == 1 || fCapacity(pSize) != cNext) {
				Type* list = new Type[cNext];
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
		const Type* begin() const {
			return (pSize == 1 ? &pValue.value : pValue.list);
		}
		Type* begin() {
			return (pSize == 1 ? &pValue.value : pValue.list);
		}
		const Type* end() const {
			return (pSize == 1 ? &pValue.value : pValue.list) + pSize;
		}
		Type* end() {
			return (pSize == 1 ? &pValue.value : pValue.list) + pSize;
		}
		size_t size() const {
			return pSize;
		}
		const Type& at(size_t index) const {
			return *(begin() + index);
		}
		Type& at(size_t index) {
			return *(begin() + index);
		}
	};

	struct Endpoint {
		size_t edge = 0;
		size_t other = 0;
	};
	using Endpoints = detail::SmallList<detail::Endpoint>;
	struct Traverse {
		size_t edge = 0;
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
	using Points = std::vector<detail::Point>;
	struct Layer {
		detail::Points points;
		size_t index = 0;
		size_t selfEdge = 0;
	};
	struct Edge {
		bool headFirst = false;
		bool headLast = false;
	};
	struct LayerState {
		std::vector<detail::Layer> layers;
		std::vector<detail::Edge> edges;
	};

	/* Order the points of every layer to minimize the crossings between neighboring layers:
	*	1.	construct the initial layers by sorting the edges onto either side of the
	*		node (while trying to reduce potential crossings between overlapping edges)
	*	2.	barycenter sweep down and up the layers, then shift every point to its crossing-minimal position,
	*		until the order settles or repeats itself, while keeping the best order seen (given that barycenter
	*		is not monotonic, the states are recorded to detect cycles and abort ocilating movements)
	*	3.	use dynamic programming to optimally place all strands better along the current alignment and if
	*		this changes the layers, repeat from step 2
	*	4.	reset some nodes towards one side to either end of their layer, propagate the change outwards and
	*		re-run steps 2-3, keeping the result only if it is strictly better */
	class LayerBuilder {
	private:
		enum class Direction : uint8_t {
			bidirectional,
			forwards,
			backwards
		};
		struct Strand {
			size_t first = 0;
			size_t last = 0;
			bool operator<(const Strand& other) const {
				if (first != other.first)
					return (first < other.first);
				return (last < other.last);
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
		struct BuildEdge {
			size_t first = 0;
			size_t last = 0;
			size_t stamp = 0;
		};
		struct StrandPath {
			size_t crossings = 0;
			float drift = 0.0f;
		};

	private:
		const std::set<size_t>& pNodes;
		const detail::Ranges& pRanges;
		detail::LayerState pLayerGraph;
		std::vector<BuildEdge> pEdges;

	public:
		LayerBuilder(const std::set<size_t>& nodes, const detail::Ranges& ranges) : pNodes{ nodes }, pRanges{ ranges } {}

	private:
		void fUpdatePrevPoint(detail::Point& point, size_t edge, size_t to) const {
			/* update the endpoint into the given direction to reference the new target */
			if (detail::Traverse* traverse = point.isTraverse(); traverse != nullptr)
				traverse->next = to;
			else for (detail::Endpoint& ep : point.node().start) {
				if (ep.edge != edge) continue;
				ep.other = to;
				break;
			}
		}
		void fUpdateNextPoint(detail::Point& point, size_t edge, size_t to) const {
			/* update the endpoint into the given direction to reference the new target */
			if (detail::Traverse* traverse = point.isTraverse(); traverse != nullptr)
				traverse->prev = to;
			else for (detail::Endpoint& ep : point.node().end) {
				if (ep.edge != edge) continue;
				ep.other = to;
				break;
			}
		}
		void fMovePoint(detail::Point& point, size_t to, size_t index) {
			/* notify the neighbors on the other layers of the point moving (the point itself is not updated) */
			if (detail::Traverse* traverse = point.isTraverse(); traverse != nullptr) {
				fUpdatePrevPoint(pLayerGraph.layers[index - 1].points[traverse->prev], traverse->edge, to);
				fUpdateNextPoint(pLayerGraph.layers[index + 1].points[traverse->next], traverse->edge, to);
				return;
			}
			for (detail::Endpoint& ep : point.node().end)
				fUpdatePrevPoint(pLayerGraph.layers[index - 1].points[ep.other], ep.edge, to);
			for (detail::Endpoint& ep : point.node().start)
				fUpdateNextPoint(pLayerGraph.layers[index + 1].points[ep.other], ep.edge, to);
		}
		size_t fFindNode(size_t index) const {
			const detail::Points& points = pLayerGraph.layers[index].points;

			/* find the node (every layer contains exactly one) */
			size_t where = 0;
			while (points[where].isNode() == nullptr)
				++where;
			return where;
		}
		intptr_t fGetSwapDelta(const detail::Point& l, const detail::Point& r) const {
			/* helper to compute the crossing delta between two single endpoints or a list of endpoints */
			const auto swapPair = [](size_t lOther, size_t rOther) -> intptr_t {
				if (lOther == rOther)
					return 0;
				return (lOther < rOther ? 1 : -1);
			};
			const auto swapEndpoints = [swapPair](size_t lOther, const detail::Endpoints& rEps) -> intptr_t {
				intptr_t delta = 0;
				for (const detail::Endpoint& ep : rEps)
					delta += swapPair(lOther, ep.other);
				return delta;
			};

			/* check if at least one of the points is a transition, which makes the calculations easier */
			if (const detail::Traverse* left = l.isTraverse(); left != nullptr) {
				if (const detail::Traverse* right = r.isTraverse(); right != nullptr)
					return swapPair(left->prev, right->prev) + swapPair(left->next, right->next);
				const detail::Node& right = r.node();
				return swapEndpoints(left->prev, right.end) + swapEndpoints(left->next, right.start);
			}
			else if (const detail::Traverse* right = r.isTraverse(); right != nullptr) {
				const detail::Node& left = l.node();
				return -(swapEndpoints(right->prev, left.end) + swapEndpoints(right->next, left.start));
			}

			/* compute the delta over the two nodes */
			const detail::Node& left = l.node();
			const detail::Node& right = r.node();
			intptr_t delta = 0;
			for (const detail::Endpoint& ep : left.end)
				delta += swapEndpoints(ep.other, right.end);
			for (const detail::Endpoint& ep : left.start)
				delta += swapEndpoints(ep.other, right.start);
			return delta;
		}
		std::pair<size_t, size_t> fMeasureLayers() const {
			detail::Hasher hasher;
			size_t crossings = 0;

			/* helper to count the crossings of the given left point with the single right edge */
			const auto addCrossings = [&crossings](const detail::Point& lPoint, size_t rOther) -> void {
				if (const detail::Traverse* traverse = lPoint.isTraverse(); traverse != nullptr) {
					if (rOther < traverse->prev)
						++crossings;
				}
				else for (const detail::Endpoint& ep : lPoint.node().end) {
					if (rOther < ep.other)
						++crossings;
				}
			};

			/* hash the order of the points of all layers and compute the crossings count of the layers */
			for (const detail::Layer& layer : pLayerGraph.layers) {
				for (size_t i = 0; i < layer.points.size(); ++i) {
					const detail::Point& point = layer.points[i];

					/* extend the hash (use the edge + 1 to identify traversals and 0 for the node) */
					hasher.add(point.isTraverse() != nullptr ? point.traverse().edge + 1 : 0);

					/* iterate over the upcoming points and compute the crossings with this point */
					for (size_t j = i + 1; j < layer.points.size(); ++j) {
						const detail::Point& other = layer.points[j];
						if (const detail::Traverse* traverse = other.isTraverse(); traverse != nullptr)
							addCrossings(point, traverse->prev);
						else for (const detail::Endpoint& ep : other.node().end)
							addCrossings(point, ep.other);
					}
				}

				/* add the layer delimiter (max-int) to the hash */
				hasher.add(std::numeric_limits<size_t>::max());
			}
			return { hasher.digest(), crossings };
		}

	private:
		void fSetupPaths(size_t edge, std::vector<StrandPath>& paths, size_t& actual) const {
			const BuildEdge& buildEdge = pEdges[edge];
			const detail::Points& prev = pLayerGraph.layers[buildEdge.first].points;
			const detail::Points& next = pLayerGraph.layers[buildEdge.first + 1].points;

			/* helper to compute the absolute and normalized drift and to determine if a
			*	point is the current edge and to update the crossings count for a moved point */
			const auto getDrift = [&prev, &next](size_t iStart, size_t iNext) -> float {
				float dPrev = (static_cast<float>(iStart) + 0.5f) / prev.size();
				float dNext = static_cast<float>(iNext) / next.size();
				return (dPrev < dNext ? (dNext - dPrev) : (dPrev - dNext));
			};
			const auto isCurrent = [edge](const detail::Point& point) -> bool {
				return (point.isTraverse() != nullptr && point.traverse().edge == edge);
			};
			const auto getMoved = [](size_t self, size_t other, size_t crossings) -> size_t {
				if (other == self)
					return crossings;
				return (other < self ? crossings - 1 : crossings + 1);
			};

			/* count the initial crossings from the left start to the edge-node */
			size_t crossings = 0, start = 0;
			while (prev[start].isTraverse() != nullptr)
				++crossings, ++start;

			/* initialize the list of paths */
			paths.resize(next.size() + 1);
			for (size_t i = 0;; ++i) {
				paths[i].crossings = crossings;
				paths[i].drift = getDrift(start, i);

				/* check if the end has been reached and otherwise update the actual current crossings */
				if (i >= next.size())
					break;
				if (isCurrent(next[i]))
					actual += crossings;

				/* update the crossing count to consider the right point skipped */
				if (const detail::Node* node = next[i].isNode(); node != nullptr) for (const detail::Endpoint& ep : node->end)
					crossings = getMoved(start, ep.other, crossings);
				else if (next[i].traverse().edge != edge)
					crossings = getMoved(start, next[i].traverse().prev, crossings);
			}
		}
		void fAdvancePaths(size_t index, size_t edge, const std::vector<StrandPath>& current, std::vector<StrandPath>& paths, std::vector<size_t>& predecessors, size_t& actual) const {
			const detail::Points& prev = pLayerGraph.layers[index].points;
			const detail::Points& next = pLayerGraph.layers[index + 1].points;

			/* helper to compute the absolute and normalized drift and to determine if a point is the current edge */
			const auto getDrift = [&prev, &next](size_t iPrev, size_t iNext) -> float {
				float dPrev = static_cast<float>(iPrev) / prev.size();
				float dNext = static_cast<float>(iNext) / next.size();
				return (dPrev < dNext ? (dNext - dPrev) : (dPrev - dNext));
			};
			const auto isCurrent = [edge](const detail::Point& point) -> bool {
				return (point.isTraverse() != nullptr && point.traverse().edge == edge);
			};

			/* initialize the crossings map, which maps prev-position to crossings
			*	between it and the current position, which starts at zero */
			std::vector<size_t> crossings(prev.size() + 1, 0);
			for (size_t i = 0; i < prev.size(); ++i) {
				if (const detail::Node* node = prev[i].isNode(); node != nullptr)
					crossings[i + 1] = crossings[i] + node->start.size();
				else
					crossings[i + 1] = crossings[i] + (isCurrent(prev[i]) ? 0 : 1);
			}

			/* iterate over the potential new endpoint paths and find
			*	the cheapest step from the current paths to the position */
			paths.resize(next.size() + 1);
			predecessors.resize(next.size() + 1);
			for (size_t i = 0;; ++i) {
				StrandPath& path = paths[i];

				/* select the cheapest of the current paths with the lowest drift to reach the current point */
				size_t best = 0, bestCrossings = 0;
				float bestDrift = 0;
				for (size_t j = 0; j <= prev.size(); ++j) {
					size_t curCrossings = current[j].crossings + crossings[j];
					float curDrift = current[j].drift + getDrift(j, i);
					if (j == 0 || curCrossings < bestCrossings || (bestCrossings == curCrossings && curDrift < bestDrift))
						best = j, bestCrossings = curCrossings, bestDrift = curDrift;
				}

				/* setup the new best path and write the predecessor back */
				path.crossings = bestCrossings;
				path.drift = bestDrift;
				predecessors[i] = best;

				/* check if the end has been reached and otherwise update the actual current crossings */
				if (i >= next.size())
					break;
				if (isCurrent(next[i]))
					actual += crossings[next[i].traverse().prev];

				/* iterate other the crossings and update them based on the moved point being pushed around */
				if (const detail::Node* node = next[i].isNode(); node != nullptr) {
					for (size_t j = 0; j < crossings.size(); ++j) {
						for (const detail::Endpoint& ep : node->end)
							crossings[j] = (ep.other < j ? crossings[j] - 1 : crossings[j] + 1);
					}
				}
				else if (next[i].traverse().edge != edge) {
					size_t where = next[i].traverse().prev;
					for (size_t j = 0; j < crossings.size(); ++j)
						crossings[j] = (where < j ? crossings[j] - 1 : crossings[j] + 1);
				}
			}
		}
		size_t fFinalizePaths(size_t edge, std::vector<StrandPath>& paths, size_t& actual) const {
			const BuildEdge& buildEdge = pEdges[edge];
			const detail::Points& prev = pLayerGraph.layers[buildEdge.last - 1].points;
			const detail::Points& next = pLayerGraph.layers[buildEdge.last].points;

			/* helper to compute the absolute and normalized drift and to determine if a
			*	point is the current edge and to update the crossings count for a moved point */
			const auto getDrift = [&prev, &next](size_t iPrev, size_t iEnd) -> float {
				float dPrev = static_cast<float>(iPrev) / prev.size();
				float dNext = (static_cast<float>(iEnd) + 0.5f) / next.size();
				return (dPrev < dNext ? (dNext - dPrev) : (dPrev - dNext));
			};
			const auto isCurrent = [edge](const detail::Point& point) -> bool {
				return (point.isTraverse() != nullptr && point.traverse().edge == edge);
			};
			const auto getMoved = [](size_t self, size_t other, size_t crossings) -> size_t {
				if (other == self)
					return crossings;
				return (other < self ? crossings - 1 : crossings + 1);
			};

			/* count the initial crossings from the left start to the edge-node */
			size_t crossings = 0, end = 0;
			while (next[end].isTraverse() != nullptr)
				++crossings, ++end;

			/* update each of the paths towards the target position and check if the best path has been found */
			size_t best = 0;
			for (size_t i = 0;; ++i) {
				StrandPath& path = paths[i];
				path.crossings += crossings;
				path.drift += getDrift(i, end);

				/* check if the path is better than the last best (for same crossings, select the path with the lowest normalized drift) */
				StrandPath& pathBest = paths[best];
				if (i == 0 || path.crossings < pathBest.crossings || (path.crossings == pathBest.crossings && path.drift < pathBest.drift))
					best = i;

				/* check if the end has been reached and otherwise update the actual current crossings */
				if (i >= prev.size())
					break;
				if (isCurrent(prev[i]))
					actual += crossings;

				/* update the crossing count to consider the right point skipped */
				if (const detail::Node* node = prev[i].isNode(); node != nullptr) for (const detail::Endpoint& ep : node->start)
					crossings = getMoved(end, ep.other, crossings);
				else if (prev[i].traverse().edge != edge)
					crossings = getMoved(end, prev[i].traverse().next, crossings);
			}
			return best;
		}

	private:
		void fConstructLayers() {
			pLayerGraph.layers.clear();
			pLayerGraph.edges.clear();
			pEdges.clear();

			/* iterate over the nodes and create the layers for each node */
			std::unordered_map<size_t, size_t> nodes;
			for (size_t node : pNodes) {
				size_t layer = pLayerGraph.layers.size();
				nodes.insert({ node, layer });
				pLayerGraph.layers.push_back(detail::Layer{ .index = node, .selfEdge = std::numeric_limits<size_t>::max() });
			}

			/* iterate over the ranges and select all relevant edges (first occurrence
			*	for each) and write them to the layers and register the self-loops */
			std::set<Strand> ranges;
			for (size_t i = 0; i < pRanges.size(); ++i) {
				Strand strand = Strand{ .first = pRanges[i].first, .last = pRanges[i].last };
				if (!ranges.insert(strand).second)
					continue;
				size_t first = nodes[strand.first], last = nodes[strand.last];

				/* add the range to the list of edges */
				size_t edge = pLayerGraph.edges.size();
				pLayerGraph.edges.push_back(detail::Edge{ .headFirst = pRanges[i].headFirst, .headLast = pRanges[i].headLast });
				pEdges.push_back(BuildEdge{ .first = first, .last = last, .stamp = 0 });

				/* check if this is a self-loop and register it to the layer (can at most be one) */
				if (first == last)
					pLayerGraph.layers[first].selfEdge = edge;
			}

			/* collect the overlap between the edges (used to select the side each edge will
			*	be inserted on) and the list of all relevant edges, which cross one layer */
			std::vector<std::vector<size_t>> overlap(pEdges.size());
			std::vector<size_t> relevant;
			for (size_t i = 0; i < pEdges.size(); ++i) {
				/* ensure that the edge at least spans one layer */
				size_t f0 = pEdges[i].first, l0 = pEdges[i].last;
				if (l0 - f0 <= 1)
					continue;
				relevant.push_back(i);

				/* find all other edges, which may overlap */
				for (size_t j = i + 1; j < pEdges.size(); ++j) {
					size_t f1 = pEdges[j].first, l1 = pEdges[j].last;
					if (l1 - f1 <= 1)
						continue;

					/* check if the edges truly overlap */
					if ((f0 < f1 && f1 < l0 && l0 < l1) || (f1 < f0 && f0 < l1 && l1 < l0)) {
						overlap[i].push_back(j);
						overlap[j].push_back(i);
					}
				}
			}

			/* assign each relevant edge a side (greedy assignment) where it encounters the fewest overlapping
			*	conflicts (longest first as they affect others most; ties and conflict-free strands go to the
			*	right; stable_sort for consistent graph results independent on sorting algorithm) */
			std::vector<int8_t> side(pEdges.size(), 0);
			std::stable_sort(relevant.begin(), relevant.end(), [&edges = pEdges](size_t a, size_t b) {
				return (edges[a].last - edges[a].first) > (edges[b].last - edges[b].first);
			});
			const auto getConflicts = [&overlap, &side](size_t edge) -> intptr_t {
				intptr_t sum = 0;
				for (size_t other : overlap[edge])
					sum += side[other];
				return sum;
			};
			for (size_t edge : relevant)
				side[edge] = (getConflicts(edge) > 0 ? -1 : 1);

			/* reduce the side selection conflicts by flipping edges, which
			*	lie on the side of most of their conflicts (strictly monotonic) */
			for (bool changed = true; changed;) {
				changed = false;
				for (size_t edge : relevant) {
					if (getConflicts(edge) * side[edge] > 0)
						side[edge] = -side[edge], changed = true;
				}
			}

			/* helper to determine which edge is further outwards (later
			*	end is outwards, on equal ends, earlier start is outwards) */
			const auto sortByOuter = [&edges = pEdges](size_t test, size_t other) -> bool {
				if (edges[test].last != edges[other].last)
					return (edges[test].last > edges[other].last);
				return (edges[test].first < edges[other].first);
			};

			/* construct the points per layer with all edges on side (<0) on
			*	the left of the node, and all other edges on the right */
			std::vector<size_t> left, right, endpoint;
			for (size_t i = 0; i < pLayerGraph.layers.size(); ++i) {
				left.clear(), right.clear(), endpoint.clear();

				/* collect all of the edges, which start/end at the layer or pass it on either side */
				for (size_t j = 0; j < pEdges.size(); ++j) {
					if (j == pLayerGraph.layers[i].selfEdge)
						continue;
					if (pEdges[j].first == i || pEdges[j].last == i)
						endpoint.push_back(j);
					else if (pEdges[j].first < i && i < pEdges[j].last)
						(side[j] < 0 ? left : right).push_back(j);
				}

				/* sort the sides from outwards to inwards (i.e. by nesting level towards the node) */
				std::sort(left.begin(), left.end(), sortByOuter);
				std::sort(right.begin(), right.end(), [&sortByOuter](size_t a, size_t b) { return sortByOuter(b, a); });

				/* construct the layer points themselves */
				detail::Layer& layer = pLayerGraph.layers[i];
				layer.points.reserve(left.size() + right.size() + 1);
				for (size_t edge : left)
					layer.points.push_back(detail::Point{ .body = detail::Traverse{.edge = edge} });
				detail::Node& node = layer.points.emplace_back(detail::Point{ .body = detail::Node{} }).node();
				for (size_t edge : endpoint)
					(pEdges[edge].first == i ? node.start : node.end).push(detail::Endpoint{ .edge = edge });
				for (size_t edge : right)
					layer.points.push_back(detail::Point{ .body = detail::Traverse{.edge = edge} });
			}

			/* helper to link a point to the previous layer */
			std::unordered_map<size_t, std::pair<size_t, size_t*>> mapping;
			const auto linkPoint = [&mapping](size_t edge, size_t& self, size_t index) -> void {
				std::pair<size_t, size_t*> entry = mapping[edge];
				self = entry.first;
				*entry.second = index;
			};

			/* setup the links between the separate layers */
			for (detail::Layer& layer : pLayerGraph.layers) {
				std::unordered_map<size_t, std::pair<size_t, size_t*>> current;

				/* iterate over the points of the current layer and link them to
				*	the previous layer and update the map for the upcoming layer */
				for (size_t i = 0; i < layer.points.size(); ++i) {
					detail::Point& point = layer.points[i];
					if (detail::Traverse* traverse = point.isTraverse(); traverse != nullptr) {
						current[traverse->edge] = { i, &traverse->next };
						linkPoint(traverse->edge, traverse->prev, i);
					}
					else {
						detail::Node& node = point.node();
						for (detail::Endpoint& ep : node.end)
							linkPoint(ep.edge, ep.other, i);
						for (detail::Endpoint& ep : node.start)
							current[ep.edge] = { i, &ep.other };
					}
				}

				/* replace the mapping (of the previous layer) with the current next layer */
				mapping = std::move(current);
			}
		}
		bool fReorderLayer(size_t index, Direction direction) {
			detail::Layer& layer = pLayerGraph.layers[index];
			std::vector<Preferred> preferred;
			preferred.reserve(layer.points.size());

			/* calculate the average position of each point based on its connected neighbors
			*	and sort the points into the preferred positions (barycenter method) */
			for (size_t i = 0; i < layer.points.size(); ++i) {
				float others = 0.0f;
				size_t count = 0;

				/* compute the average position of the endpoints of the current point (a point
				*	without any neighbors in the given direction keeps the current position) */
				const detail::Point& pt = layer.points[i];
				if (const detail::Traverse* traverse = pt.isTraverse(); traverse != nullptr) {
					if (direction != Direction::forwards)
						others += static_cast<float>(traverse->prev), ++count;
					if (direction != Direction::backwards)
						others += static_cast<float>(traverse->next), ++count;
				}
				else {
					const detail::Node& node = pt.node();
					if (direction != Direction::forwards) for (const detail::Endpoint& ep : node.end)
						others += static_cast<float>(ep.other), ++count;
					if (direction != Direction::backwards) for (const detail::Endpoint& ep : node.start)
						others += static_cast<float>(ep.other), ++count;
				}
				preferred.push_back(Preferred{ .average = (count == 0 ? static_cast<float>(i) : others / static_cast<float>(count)), .index = i });
			}

			/* sort the points by their preferred position (equal scores will preserve
			*	their current position) and check if the layer changes at all */
			std::sort(preferred.begin(), preferred.end());
			bool changed = false;
			for (size_t i = 0; i < preferred.size() && !changed; ++i)
				changed = (preferred[i].index != i);
			if (!changed)
				return false;

			/* construct the new layer based on the new indices */
			detail::Points oldPoints{ layer.points.size(), detail::Point{} };
			std::swap(oldPoints, layer.points);
			for (size_t i = 0; i < preferred.size(); ++i) {
				if (preferred[i].index != i)
					fMovePoint(oldPoints[preferred[i].index], i, index);
				layer.points[i] = std::move(oldPoints[preferred[i].index]);
			}
			return true;
		}
		bool fShiftLayer(size_t index) {
			detail::Layer& layer = pLayerGraph.layers[index];
			bool changed = false;

			/* try every point of the layer to be moved either anywhere to the left or right */
			for (size_t i = 0; i < layer.points.size(); ++i) {
				intptr_t current = 0, bestScore = 0;
				size_t bestIndex = i;

				/* compute the change in the crossings if point i and j were to change position (as the crossings
				*	with any other neighbors are not affected by this, only the edges of the two points can be
				*	evaluated; provided the neighbors inbetween the points were already included into the calculation) */
				for (size_t j = i + 1; j < layer.points.size(); ++j) {
					current += fGetSwapDelta(layer.points[i], layer.points[j]);
					if (current < bestScore)
						bestScore = current, bestIndex = j;
				}
				current = 0;
				for (size_t j = i; j > 0; --j) {
					current += fGetSwapDelta(layer.points[j - 1], layer.points[i]);
					if (current < bestScore)
						bestScore = current, bestIndex = j - 1;
				}

				/* check if a new better position has been found */
				if (bestScore >= 0)
					continue;
				changed = true;

				/* move the point to the target position by swapping the neighbors
				*	one-by-one to keep the references to neighbors valid */
				for (size_t j = i; j != bestIndex;) {
					size_t next = detail::Towards(j, bestIndex);
					fMovePoint(layer.points[j], next, index);
					fMovePoint(layer.points[next], j, index);
					std::swap(layer.points[j], layer.points[next]);
					j = next;
				}
			}
			return changed;
		}
		bool fShiftStrand(size_t index) {
			/* check if the edge is too small and can be ignored */
			BuildEdge& edge = pEdges[index];
			if (edge.last - edge.first <= 1)
				return false;

			/* create the set of potential paths from the first to the last node and track their crossing counts
			*	as well as the current crossings (dynamic programming approach to finding cheapest paths) */
			std::vector<std::vector<size_t>> predecessors;
			predecessors.reserve(edge.last - edge.first - 2);
			std::vector<StrandPath> paths, last;
			size_t crossings = 0;
			fSetupPaths(index, paths, crossings);
			for (size_t i = edge.first + 1; i < edge.last - 1; ++i) {
				std::swap(paths, last);
				fAdvancePaths(i, index, last, paths, predecessors.emplace_back(), crossings);
			}

			/* check if a best path has been found, which is better than the current path
			*	(will then make the layers strictly better, thereby being a monotonic algorithm) */
			size_t bestIndex = fFinalizePaths(index, paths, crossings);
			if (paths[bestIndex].crossings >= crossings)
				return false;

			/* move the points into position (only the traversals, as the nodes will
			*	stay intact; backwards to allow the predecessor-path to be unrolled) */
			for (size_t i = edge.last - 1; i > edge.first; --i) {
				detail::Points& points = pLayerGraph.layers[i].points;

				/* find the index of the current point */
				size_t where = 0;
				while (points[where].isTraverse() == nullptr || points[where].traverse().edge != index)
					++where;

				/* extract the target position the point actually needs
				*	to be moved to and backtrack the best index */
				size_t target = bestIndex;
				if (target > where)
					--target;
				if (i - edge.first >= 2)
					bestIndex = predecessors[i - edge.first - 2][bestIndex];

				/* move the point to the new position */
				while (where != target) {
					size_t next = detail::Towards(where, target);
					fMovePoint(points[where], next, i);
					fMovePoint(points[next], where, i);
					std::swap(points[where], points[next]);
					where = next;
				}
			}
			return true;
		}
		bool fResetLayer(size_t index, bool toFront, size_t& bestCrossings) {
			detail::Points& points = pLayerGraph.layers[index].points;

			/* find the node and check if it is already at the target position */
			size_t where = fFindNode(index), target = (toFront ? 0 : points.size() - 1);
			if (where == target)
				return false;

			/* preserve the current state (to revert worse graphs) and move the node to the edge */
			std::vector<detail::Layer> layers = pLayerGraph.layers;
			while (where != target) {
				size_t next = detail::Towards(where, target);
				fMovePoint(points[where], next, index);
				fMovePoint(points[next], where, index);
				std::swap(points[where], points[next]);
				where = next;
			}

			/* propagate the changes through by reordering the layers overall to include the new shifted node */
			for (size_t i = index + 1; i < pLayerGraph.layers.size() && fReorderLayer(i, Direction::backwards); ++i);
			for (size_t i = index; i > 0 && fReorderLayer(i - 1, Direction::forwards); --i);

			/* repair the layers and strands locally once to check if the reset
			*	has any value, and only then repair them until they settle */
			fShiftLayers();
			fShiftStrands();
			size_t crossings = fMeasureLayers().second;
			if (crossings <= bestCrossings) {
				for (bool dirty = true; dirty;) {
					dirty = fShiftLayers();
					dirty = (fShiftStrands() || dirty);
				}
				crossings = fMeasureLayers().second;
			}

			/* check if the old graph should be recovered, as it has not been
			*	meaningfully improved and otherwise commit the better state */
			if (crossings >= bestCrossings) {
				pLayerGraph.layers = std::move(layers);
				return false;
			}
			bestCrossings = crossings;
			return true;
		}

	private:
		bool fReorderLayers() {
			bool changed = false;

			/* perform a full sweep by moving down the layers first and
			*	back up (adjusting against the neighbors in both directions) */
			for (size_t i = 1; i < pLayerGraph.layers.size(); ++i)
				changed = (fReorderLayer(i, Direction::bidirectional) || changed);
			for (size_t i = pLayerGraph.layers.size() - 1; i > 0; --i)
				changed = (fReorderLayer(i - 1, Direction::bidirectional) || changed);
			return changed;
		}
		bool fShiftLayers() {
			bool changed = false;

			/* try to reduce the crossings by shifting the nodes within the
			*	layers to reduce crossings to the immediate neighbor layers */
			for (bool dirty = true; dirty;) {
				dirty = false;
				for (size_t i = 0; i < pLayerGraph.layers.size(); ++i)
					dirty = (fShiftLayer(i) || dirty);
				changed = (changed || dirty);
			}
			return changed;
		}
		bool fShiftStrands() {
			bool changed = false;

			/* setup the current stamps and reset the edge-stamps */
			size_t stamp = 1;
			std::vector<size_t> layers(pLayerGraph.layers.size(), 1);
			for (BuildEdge& edge : pEdges)
				edge.stamp = 0;

			/* iterate over the strands and try to move all strands into better positions (will
			*	only be moved, if they are strictly better, hence the algorithm is monotonic) */
			for (bool dirty = true; dirty;) {
				dirty = false;
				for (size_t i = 0; i < pEdges.size(); ++i) {
					BuildEdge& edge = pEdges[i];

					/* check if the edge is outdated and needs to be re-checked */
					auto relevant = (layers | std::views::drop(edge.first) | std::views::take(edge.last - edge.first + 1));
					if (std::ranges::all_of(relevant, [is = edge.stamp](auto v) { return v <= is; }))
						continue;
					edge.stamp = stamp;

					/* try to shift the strand into a new position */
					if (!fShiftStrand(i))
						continue;

					/* mark the affected layers and edge as being updated */
					edge.stamp = ++stamp;
					for (size_t& layer : relevant)
						layer = stamp;
					dirty = true;
				}
				changed = (changed || dirty);
			}
			return changed;
		}
		size_t fOptimizeLayers() {
			while (true) {
				std::vector<detail::Layer> bestLayers = pLayerGraph.layers;

				/* measure the current layers to keep track of the best
				*	version and to detect cycles (barycenter is not monotonic) */
				auto [hash, bestCrossings] = fMeasureLayers();
				std::unordered_set<size_t> seen{ hash };

				/* alternate reordering and shifting until the order settles or repeats itself (based on already seen hashes
				*	of the layer states; terminates as the iteration step is a deterministic change in a finite order) */
				while (true) {
					bool changed = fReorderLayers();
					changed = (fShiftLayers() || changed);

					/* check if the new state is the best state so far and update
					*	the seen list to determine if the layers have settled */
					auto [hash, crossings] = fMeasureLayers();
					if (crossings < bestCrossings)
						bestCrossings = crossings, bestLayers = pLayerGraph.layers;
					if (!changed || !seen.insert(hash).second)
						break;
				}

				/* restore the current best layer state (will always be just as good as the stored state) */
				pLayerGraph.layers = std::move(bestLayers);

				/* try to move the strands into position and restart the algorithm if a change occured (to potentially
				*	reach a new optimal minimum), and otherwise, return the crossing count, as it will not have changed */
				if (fShiftStrands())
					continue;
				return bestCrossings;
			}
		}
		void fResetLayers(size_t bestCrossings, ascg::IterationStop auto&& stopTest) {
			if (bestCrossings == 0)
				return;

			/* setup the list of relevant nodes (ignore any nodes with too many edges
			*	on either side as they are likely to be pulled back again anyways) */
			std::vector<std::pair<size_t, size_t>> nodes;
			for (size_t i = 0; i < pLayerGraph.layers.size(); ++i) {
				const detail::Node& node = pLayerGraph.layers[i].points[fFindNode(i)].node();
				size_t edges = std::min<size_t>(node.start.size(), node.end.size());
				if (pLayerGraph.layers[i].points.size() > 1 && edges < 2)
					nodes.push_back({ edges, i });
			}

			/* sort the nodes by their edge-count to ensure nodes, which are more likely
			*	to move, are moved first, and the stamina will therefore run out faster */
			std::ranges::sort(nodes);

			/* reset each layer into either direction and iterate until
			*	no changes are found anymore (cycle through the nodes) */
			size_t node = 0;
			for (size_t stamina = 1; (stamina <= nodes.size() && bestCrossings > 0); ++stamina) {
				size_t layer = nodes[node++].second;
				if (node >= nodes.size())
					node = 0;

				/* try each direction and check if the layout has improved */
				for (size_t dir = 0; dir < 2; ++dir) {
					if (stopTest())
						return;
					if (fResetLayer(layer, (dir == 0), bestCrossings))
						stamina = 0, dir = 2;
				}
			}
		}

	public:
		detail::LayerState build(ascg::IterationStop auto&& stopTest) {
			/* check if the ranges are empty, in which case nothing needs to be done,
			*	and otherwise create the initial layers (cannot be empty anymore) */
			if (pRanges.empty())
				return {};
			fConstructLayers();

			/* optimize the layers and try to escape
			*	local minima by resetting node positions */
			fResetLayers(fOptimizeLayers(), stopTest);

			/* return the complete generated layers from the internal state */
			detail::LayerState state;
			std::swap(pLayerGraph, state);
			return state;
		}
	};
}
