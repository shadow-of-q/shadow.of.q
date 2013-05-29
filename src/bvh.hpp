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

// triangle required to build the BVH
struct triangle {
  INLINE triangle(void) {}
  INLINE triangle(vec3f a, vec3f b, vec3f c) {v[0]=a; v[1]=b; v[2]=c;}
  INLINE aabb getaabb(void) const {
    return aabb(min(min(v[0],v[1]),v[2]), max(max(v[0],v[1]),v[2]));
  }
  vec3f v[3];
};

// opaque intersector data structure
struct intersector *create(const triangle *tri, int n);
void destroy(struct intersector*);

// ray tracing routine inside the intersector
void trace(const struct intersector&, const ray&, hit&);

} // namespace bvh
} // namespace cube

