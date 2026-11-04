#pragma once

#include "ascg-common.h"

namespace ascg::detail {
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
				delete[] pValue.list;
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
		void fUpdatePrevPoint(detail::Point& point, size_t range, size_t to) const {
			/* update the endpoint into the given direction to reference the new target */
			if (detail::Traverse* traverse = point.isTraverse(); traverse != nullptr)
				traverse->next = to;
			else for (detail::Endpoint& ep : point.node().start) {
				if (ep.range != range) continue;
				ep.other = to;
				break;
			}
		}
		void fUpdateNextPoint(detail::Point& point, size_t range, size_t to) const {
			/* update the endpoint into the given direction to reference the new target */
			if (detail::Traverse* traverse = point.isTraverse(); traverse != nullptr)
				traverse->prev = to;
			else for (detail::Endpoint& ep : point.node().end) {
				if (ep.range != range) continue;
				ep.other = to;
				break;
			}
		}
		void fMovePoint(detail::Point& point, size_t to, size_t index) const {
			/* notify the neighbors on the other layers of the point moving (the point itself is not updated) */
			if (detail::Traverse* traverse = point.isTraverse(); traverse != nullptr) {
				fUpdatePrevPoint(pLayers[index - 1].points[traverse->prev], traverse->range, to);
				fUpdateNextPoint(pLayers[index + 1].points[traverse->next], traverse->range, to);
				return;
			}
			for (detail::Endpoint& ep : point.node().end)
				fUpdatePrevPoint(pLayers[index - 1].points[ep.other], ep.range, to);
			for (detail::Endpoint& ep : point.node().start)
				fUpdateNextPoint(pLayers[index + 1].points[ep.other], ep.range, to);
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
			for (const detail::Layer& layer : pLayers) {
				for (size_t i = 0; i < layer.points.size(); ++i) {
					const detail::Point& point = layer.points[i];

					/* extend the hash (use the ranges + 1 to identify traversals and 0 for the node) */
					hasher.add(point.isTraverse() != nullptr ? point.traverse().range + 1 : 0);

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
		bool fReorderLayer(size_t index, Direction direction) const {
			detail::Layer& layer = pLayers[index];
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
		bool fShiftLayer(size_t index) const {
			detail::Layer& layer = pLayers[index];
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
		bool fReorderLayers(bool bidirectional) const {
			bool changed = false;

			/* perform a full sweep by moving down the layers first and then back up (always adjust against
			*	either the neighbors in both directions or the direction-based previous neighbors) */
			for (size_t i = 1; i < pLayers.size(); ++i)
				changed = (fReorderLayer(i, bidirectional ? Direction::bidirectional : Direction::backwards) || changed);
			for (size_t i = pLayers.size() - 1; i > 0; --i)
				changed = (fReorderLayer(i - 1, bidirectional ? Direction::bidirectional : Direction::forwards) || changed);
			return changed;
		}
		bool fShiftLayers() const {
			bool changed = false;

			/* try to reduce the crossings by shifting the nodes within the
			*	layers to reduce crossings to the immediate neighbor layers */
			for (bool dirty = true; dirty;) {
				dirty = false;
				for (size_t i = 0; i < pLayers.size(); ++i)
					dirty = (fShiftLayer(i) || dirty);
				changed = (changed || dirty);
			}
			return changed;
		}
		size_t fOptimizeLayers(bool bidirectional) const {
			std::vector<detail::Layer> bestLayers = pLayers;

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
					bestCrossings = crossings, bestLayers = pLayers;
				if (!changed || !seen.insert(hash).second)
					break;
			}

			/* restore the current best layer state (will always be just as good as the stored state) */
			pLayers = std::move(bestLayers);
			return bestCrossings;
		}

	public:
		std::vector<detail::Layer> build() const {
			/* check if the ranges are empty, in which case nothing needs to be done,
			*	and otherwise create the initial layers (cannot be empty anymore) */
			if (pRanges.empty())
				return {};
			fConstructLayers();

			/* optimize the layers by trying to optimize uni-directional and bi-directional and then
			*	picking the better final result (depending on the graph density, one or the other is better) */
			std::vector<detail::Layer> others{ pLayers };
			size_t bidirectional = fOptimizeLayers(true);
			std::swap(pLayers, others);
			if (fOptimizeLayers(false) >= bidirectional)
				pLayers = std::move(others);

			/* return the complete generated layers from the internal state */
			std::vector<detail::Layer> layers;
			std::swap(pLayers, layers);
			return layers;
		}
	};
}
