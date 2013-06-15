#pragma once
#include "math.hpp"

namespace cube {
namespace bvh {

// hit point when tracing inside a bvh
struct hit {
  float t,u,v;
  u32 id;
  INLINE hit(void) : t(FLT_MAX), u(0), v(0), id(~0u) {}
  INLINE hit(float tmax) : t(tmax), u(0), v(0), id(~0x0u) {}
  INLINE bool is_hit(void) const { return id != ~0x0u; }
};
#if 0
// triangle required to build the BVH
struct triangle {
  INLINE triangle(void) {}
  INLINE triangle(vec3f a, vec3f b, vec3f c) {v[0]=a; v[1]=b; v[2]=c;}
  INLINE aabb getaabb(void) const {
    return aabb(min(min(v[0],v[1]),v[2]), max(max(v[0],v[1]),v[2]));
  }
  vec3f v[3];
};
#else
// May be either a triangle or bounding box
struct hybrid {
  enum { TRI, AABB };
  INLINE hybrid(void) {}
  INLINE hybrid(vec3f a, vec3f b, vec3f c) : type(TRI) {v[0]=a; v[1]=b; v[2]=c;}
  INLINE hybrid(aabb box) : type(AABB) {v[0]=box.pmin; v[1]=box.pmax;}
  INLINE aabb getaabb(void) const {
    if (type == TRI)
      return aabb(min(min(v[0],v[1]),v[2]), max(max(v[0],v[1]),v[2]));
    else
      return aabb(v[0],v[1]);
  }
  vec3f v[3];
  u32 type;
};

#endif
// opaque intersector data structure
struct intersector *create(const hybrid *prims, int n);
void destroy(struct intersector*);

// ray tracing routines (visiblity and shadow rays)
void closest(const struct intersector&, const ray&, hit&);
bool occluded(const struct intersector&, const ray&);

} // namespace bvh
} // namespace cube

