#include "ecs/components/bounding_volume.h"

#include "ecs/components/vertex.h"
#include "utils/logger/log.h"

#include <algorithm>
#include <array>
#include <limits>

namespace arise {
namespace ecs {
namespace bounds {

namespace {
inline void updateMinMax(math::Vector3f& minVec, math::Vector3f& maxVec, const math::Vector3f& point) {
  minVec.x() = std::min(minVec.x(), point.x());
  minVec.y() = std::min(minVec.y(), point.y());
  minVec.z() = std::min(minVec.z(), point.z());

  maxVec.x() = std::max(maxVec.x(), point.x());
  maxVec.y() = std::max(maxVec.y(), point.y());
  maxVec.z() = std::max(maxVec.z(), point.z());
}

template <typename Iterator>
BoundingBox calculateAABBFromRange(Iterator begin, Iterator end) {
  if (begin == end) {
    return createInvalid();
  }

  math::Vector3f minVec = *begin;
  math::Vector3f maxVec = *begin;

  for (auto it = std::next(begin); it != end; ++it) {
    updateMinMax(minVec, maxVec, *it);
  }

  BoundingBox box;
  box.min = minVec;
  box.max = maxVec;
  return box;
}
}  // anonymous namespace

BoundingBox calculateAABB(const std::vector<math::Vector3f>& positions) {
  if (positions.empty()) {
    LOG_WARN("Empty position list for AABB calculation");
    return createInvalid();
  }

  return calculateAABBFromRange(positions.begin(), positions.end());
}

BoundingBox calculateAABB(const std::vector<Vertex>& vertices) {
  if (vertices.empty()) {
    LOG_WARN("Empty vertex list for AABB calculation");
    return createInvalid();
  }

  std::vector<math::Vector3f> positions;
  positions.reserve(vertices.size());

  for (const auto& vertex : vertices) {
    positions.push_back(vertex.position);
  }

  return calculateAABBFromRange(positions.begin(), positions.end());
}

BoundingBox transformAABB(const BoundingBox& aabb, const math::Matrix4f<>& transform) {
  if (!isValid(aabb)) {
    return aabb;
  }

  std::vector<math::Vector3f> transformedCorners;
  transformedCorners.reserve(8);

  std::array<math::Vector3f, 8> corners = {
    {math::Vector3f(aabb.min.x(), aabb.min.y(), aabb.min.z()),
     math::Vector3f(aabb.max.x(), aabb.min.y(), aabb.min.z()),
     math::Vector3f(aabb.min.x(), aabb.max.y(), aabb.min.z()),
     math::Vector3f(aabb.max.x(), aabb.max.y(), aabb.min.z()),
     math::Vector3f(aabb.min.x(), aabb.min.y(), aabb.max.z()),
     math::Vector3f(aabb.max.x(), aabb.min.y(), aabb.max.z()),
     math::Vector3f(aabb.min.x(), aabb.max.y(), aabb.max.z()),
     math::Vector3f(aabb.max.x(), aabb.max.y(), aabb.max.z())}
  };

  for (const auto& corner : corners) {
    math::Vector4f homogeneous(corner.x(), corner.y(), corner.z(), 1.0f);
    math::Vector4f transformed  = homogeneous;
    transformed                *= transform;
    transformedCorners.emplace_back(transformed.x(), transformed.y(), transformed.z());
  }

  return calculateAABBFromRange(transformedCorners.begin(), transformedCorners.end());
}

BoundingBox combineAABBs(const std::vector<BoundingBox>& boxes) {
  if (boxes.empty()) {
    LOG_WARN("Empty bounding box list for combination");
    return createInvalid();
  }

  std::vector<math::Vector3f> allPoints;
  allPoints.reserve(boxes.size() * 2);

  for (const auto& box : boxes) {
    if (isValid(box)) {
      allPoints.push_back(box.min);
      allPoints.push_back(box.max);
    }
  }

  if (allPoints.empty()) {
    LOG_WARN("No valid bounding boxes found in list");
    return createInvalid();
  }

  return calculateAABBFromRange(allPoints.begin(), allPoints.end());
}

}  // namespace bounds
}  // namespace ecs
}  // namespace arise