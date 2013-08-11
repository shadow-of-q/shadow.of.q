#pragma once
#include "base/tools.hpp"
#include "base/math.hpp"

// generic 3d object file directly taken from wavefront .obj (but vertices are
// processed and merged here)
namespace cube {
struct obj : noncopyable {
  // indexed vertices and material
  struct triangle {
    INLINE triangle(void) {}
    INLINE triangle(vec3i v, int m) : v(v), m(m) {}
    vec3i v;
    int m;
  };

  // stores position, normal and texture coordinates
  struct vertex {
    INLINE vertex(void) {}
    INLINE vertex(vec3f p, vec3f n, vec2f t) : p(p), n(n), t(t) {}
    vec3f p, n;
    vec2f t;
  };

  // triangles are grouped by material
  struct matgroup {
    matgroup(int first, int last, int m) : first(first), last(last), m(m) {}
    matgroup(void) {}
    int first, last, m;
  };

  // just a dump of mtl description
  struct material {
    char *name;
    char *map_Ka;
    char *map_Kd;
    char *map_D;
    char *map_Bump;
    double amb[3];
    double diff[3];
    double spec[3];
    double km;
    double reflect;
    double refract;
    double trans;
    double shiny;
    double glossy;
    double refract_index;
  };

  obj(void);
  ~obj(void);
  INLINE bool isValid(void) const {return triNum > 0;}
  bool load(const char *path);
  triangle *tri;
  vertex *vert;
  matgroup *grp;
  material *mat;
  size_t triNum;
  size_t vertNum;
  size_t grpNum;
  size_t matNum;
};
} // namespace cube

