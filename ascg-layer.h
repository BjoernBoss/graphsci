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
	struct Layer {
		std::vector<detail::Point> points;
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
	*	1.	construct the initial layers with the node in the first slot and the traversals passed through
	*	2.	repeat: barycenter sweep down and up the layers, then shift every point to its crossing-minimal
	*		position, until the order settles or repeats itself, while keeping the best order seen (given that
	*		barycenter is not monotonic, the states are recorded to detect cycles and abort ocilating movements)
	*	3.	run step 2 with the two-sided and the one-sided barycenter and keep the order with fewer crossings
	*		(two-sided tends to win on dense graphs, one-sided on sparse graphs, ties keep two-sided) */
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
			/* remove prev/next/other from layout, and only keep in internal state? */
			size_t first = 0;
			size_t last = 0;
			/* test larger is outwards in getOutwards */
		};

	private:
		const detail::Ranges& pRanges;
		detail::LayerState pLayerGraph;
		std::vector<BuildEdge> pEdges;

	public:
		LayerBuilder(const detail::Ranges& ranges) : pRanges{ ranges } {}

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

			/* helper to count the crossings */
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
					for (size_t k = i + 1; k < layer.points.size(); ++k) {
						const detail::Point& other = layer.points[k];
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
		void fConstructLayers() {
			pLayerGraph.layers.clear();
			pLayerGraph.edges.clear();

			/* iterate over the ranges and select all relevant edges (first occurrence
			*	for each) and update the edge states accordingly and setup the node map */
			std::set<Strand> edges;
			std::map<size_t, size_t> nodes;
			for (size_t i = 0; i < pRanges.size(); ++i) {
				Strand strand = Strand{ .first = pRanges[i].first, .last = pRanges[i].last };
				if (!edges.insert(strand).second)
					continue;

				/* add the range to the list of edges */
				pLayerGraph.edges.push_back(detail::Edge{ .headFirst = pRanges[i].headFirst, .headLast = pRanges[i].headLast });
				pEdges.push_back(BuildEdge{ .first = strand.first, .last = strand.last });

				/* update the node list */
				nodes[strand.first] = 0;
				nodes[strand.last] = 0;
			}

			/* iterate over the nodes and create the layers for each node */
			for (auto& [index, layer] : nodes) {
				layer = pLayerGraph.layers.size();
				pLayerGraph.layers.push_back(detail::Layer{ .index = index, .selfEdge = pLayerGraph.edges.size() });
			}

			/* iterate over the edges again and patch them up to point to the layer index and register self-loops */
			for (size_t i = 0; i < pEdges.size(); ++i) {
				BuildEdge& edge = pEdges[i];
				edge.first = nodes[edge.first];
				edge.last = nodes[edge.last];
				if (edge.first == edge.last)
					pLayerGraph.layers[edge.first].selfEdge = i;
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
			*	conflicts (longest first as they affect others most; ties and conflict-free strands go to the right) */
			std::vector<int8_t> side(pEdges.size(), 0);
			std::sort(relevant.begin(), relevant.end(), [&edges = pEdges](size_t a, size_t b) {
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

			/* helper to determine which edge is further outwards (earlier start is outwards, on equal starts, later end is outwards) */
			const auto testIsOuter = [&edges = pEdges](size_t test, size_t other) -> bool {
				if (edges[test].first != edges[other].first)
					return (edges[test].first < edges[other].first);
				return (edges[test].last > edges[other].last);
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
				std::sort(left.begin(), left.end(), testIsOuter);
				std::sort(right.begin(), right.end(), [&testIsOuter](size_t a, size_t b) { return testIsOuter(b, a); });

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

			/* link up all of the layers */
			std::unordered_map<size_t, std::pair<size_t, size_t*>> mapping;
			for (detail::Layer& layer : pLayerGraph.layers) {
				std::unordered_map<size_t, std::pair<size_t, size_t*>> current;

				for (size_t i = 0; i < layer.points.size(); ++i) {
					detail::Point& point = layer.points[i];
					if (detail::Traverse* traverse = point.isTraverse(); traverse != nullptr) {
						current[traverse->edge] = { i, &traverse->next };
						auto [index, other] = mapping[traverse->edge];
						traverse->prev = index;
						*other = i;
					}
					else {
						detail::Node& node = point.node();
						for (detail::Endpoint& ep : node.end) {
							auto [index, other] = mapping[ep.edge];
							ep.other = index;
							*other = i;
						}
						for (detail::Endpoint& ep : node.start)
							current[ep.edge] = { i, &ep.other };
					}
				}

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
			std::vector<detail::Point> oldPoints{ layer.points.size(), detail::Point{} };
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

	private:
		bool fReorderLayers(bool bidirectional) {
			bool changed = false;

			/* perform a full sweep by moving down the layers first and then back up (always adjust against
			*	either the neighbors in both directions or the direction-based previous neighbors) */
			for (size_t i = 1; i < pLayerGraph.layers.size(); ++i)
				changed = (fReorderLayer(i, (bidirectional ? Direction::bidirectional : Direction::backwards)) || changed);
			for (size_t i = pLayerGraph.layers.size() - 1; i > 0; --i)
				changed = (fReorderLayer(i - 1, (bidirectional ? Direction::bidirectional : Direction::forwards)) || changed);
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
		size_t fOptimizeLayers(bool bidirectional) {
			std::vector<detail::Layer> bestLayers = pLayerGraph.layers;

			/* measure the current layers to keep track of the best
			*	version and to detect cycles (barycenter is not monotonic) */
			auto [hash, bestCrossings] = fMeasureLayers();
			std::unordered_set<size_t> seen{ hash };

			/* alternate reordering and shifting until the order settles or repeats itself (based on already seen hashes
			*	of the layer states; terminates as the iteration step is a deterministic change in a finite order) */
			while (true) {
				bool changed = fReorderLayers(bidirectional);
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
			return bestCrossings;
		}

	public:
		detail::LayerState build() {
			/* check if the ranges are empty, in which case nothing needs to be done,
			*	and otherwise create the initial layers (cannot be empty anymore) */
			if (pRanges.empty())
				return {};
			fConstructLayers();

			/* optimize the layers by trying to optimize uni-directional and bi-directional and then
			*	picking the better final result (depending on the graph density, one or the other is better) */
			std::vector<detail::Layer> layers{ pLayerGraph.layers };
			size_t bidirectional = fOptimizeLayers(true);
			std::swap(pLayerGraph.layers, layers);
			if (fOptimizeLayers(false) >= bidirectional)
				pLayerGraph.layers = std::move(layers);

			/* return the complete generated layers from the internal state */
			detail::LayerState state;
			std::swap(pLayerGraph, state);
			return state;
		}
	};
}
