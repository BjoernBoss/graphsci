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
}
