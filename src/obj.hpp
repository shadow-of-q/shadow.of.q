#pragma once
#include "base/tools.hpp"
#include "base/math.hpp"

namespace cube {

// generic 3d object file directly taken from wavefront .obj (but vertices are
// processed and merged here)
struct obj : noncopyable
{
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
    INLINE matgroup(void) {}
    INLINE matgroup(int first, int last, int m) : first(first), last(last), m(m) {}
    int first, last, m;
  };
  // just a dump of mtl description
  struct material {
    char *name;
    char *mapka;
    char *mapkd;
    char *mapd;
    char *mapbump;
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
  INLINE bool valid(void) const { return trinum > 0; }
  bool load(const char *path);
  triangle *tri;
  vertex *vert;
  matgroup *grp;
  material *mat;
  u32 trinum;
  u32 vertnum;
  u32 grpnum;
  u32 matnum;
};
} // namespace cube

