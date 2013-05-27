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

// XXX REMOVE THAT
struct aabb_t {
  vec3f pmin, pmax;
  aabb_t() {}
  aabb_t(const vec3f &v1, const vec3f &v2): pmin(v1), pmax(v2) {}
  explicit aabb_t(float min_value, float max_value)
    : pmin(vec3f(min_value, min_value, min_value)),
    pmax(vec3f(max_value, max_value, max_value)) {}

  INLINE void compose(const aabb_t &b) {
    pmin.x = min(pmin.x, b.pmin.x);
    pmin.y = min(pmin.y, b.pmin.y);
    pmin.z = min(pmin.z, b.pmin.z);
    pmax.x = max(pmax.x, b.pmax.x);
    pmax.y = max(pmax.y, b.pmax.y);
    pmax.z = max(pmax.z, b.pmax.z);
  }
  INLINE float half_surface_area() const {
    const vec3f e(get_extent());
    return e.x*e.y + e.y*e.z + e.x*e.z;
  }
  INLINE double get_area() const {
    const vec3f ext(pmax-pmin);
    return double(ext.x)*double(ext.y) + double(ext.y)*double(ext.z) + double(ext.x)*double(ext.z);
  }
  vec3f get_extent() const { return pmax-pmin; }
};

// triangle required to build the BVH
struct triangle {
  INLINE triangle(void) {}
  INLINE triangle(vec3f a, vec3f b, vec3f c) {v[0]=a; v[1]=b; v[2]=c;}
  INLINE aabb_t get_aabb() const {
    return aabb_t(min(min(v[0],v[1]),v[2]), max(max(v[0],v[1]),v[2]));
  }
  vec3f v[3];
};

// opaque intersector routine
struct intersector *create(const triangle *tri, int n);
void destroy(struct intersector*);

// ray tracing routine inside the intersector
void trace(const struct intersector&, const ray&, hit&);

} // namespace bvh
} // namespace cube

