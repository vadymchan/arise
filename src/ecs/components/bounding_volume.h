#ifndef ARISE_BOUNDING_VOLUME_H
#define ARISE_BOUNDING_VOLUME_H

#include <math_library/vector.h>

#include <vector>

namespace arise {
namespace ecs {

class Vertex;

struct BoundingBox {
  math::Vector3f min{0.0f, 0.0f, 0.0f};
  math::Vector3f max{0.0f, 0.0f, 0.0f};
};

struct WorldBounds {
  BoundingBox boundingBox;
  bool        isDirty = true;
};

namespace bounds {

inline math::Vector3f getSize(const BoundingBox& box) {
  return box.max - box.min;
}

inline math::Vector3f getCenter(const BoundingBox& box) {
  return (box.min + box.max) * 0.5f;
}

inline bool isValid(const BoundingBox& box) {
  return box.min.x() <= box.max.x() && box.min.y() <= box.max.y() && box.min.z() <= box.max.z();
}

inline void expandToInclude(BoundingBox& box, const math::Vector3f& point) {
  if (box.min.x() > point.x()) {
    box.min.x() = point.x();
  }
  if (box.min.y() > point.y()) {
    box.min.y() = point.y();
  }
  if (box.min.z() > point.z()) {
    box.min.z() = point.z();
  }
  if (box.max.x() < point.x()) {
    box.max.x() = point.x();
  }
  if (box.max.y() < point.y()) {
    box.max.y() = point.y();
  }
  if (box.max.z() < point.z()) {
    box.max.z() = point.z();
  }
}

inline BoundingBox createInvalid() {
  BoundingBox box;
  box.min = math::Vector3f(std::numeric_limits<float>::max());
  box.max = math::Vector3f(std::numeric_limits<float>::lowest());
  return box;
}

inline bool intersects(const BoundingBox& a, const BoundingBox& b) {
  return (a.min.x() <= b.max.x() && a.max.x() >= b.min.x()) && (a.min.y() <= b.max.y() && a.max.y() >= b.min.y())
      && (a.min.z() <= b.max.z() && a.max.z() >= b.min.z());
}

BoundingBox calculateAABB(const std::vector<Vertex>& vertices);

BoundingBox calculateAABB(const std::vector<math::Vector3f>& positions);

BoundingBox transformAABB(const BoundingBox& aabb, const math::Matrix4f<>& transform);

BoundingBox combineAABBs(const std::vector<BoundingBox>& boxes);

}  // namespace bounds

}  // namespace ecs
}  // namespace arise

#endif  // ARISE_BOUNDING_VOLUME_H