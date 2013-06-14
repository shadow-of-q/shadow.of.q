#pragma once
#include "entities.hpp"
#include "tools.hpp"
#include "math.hpp"
#include "bvh.hpp"
#include "ogl.hpp"

namespace cube {
namespace world {
struct block { int x, y, xs, ys; };

enum {
  MAPVERSION = 5, // bump if map format changes, see worldio.cpp
  SMALLEST_FACTOR = 6, // determines number of mips there can be
  DEFAULT_FACTOR = 8,
  LARGEST_FACTOR = 11 // 10 is already insane
};

// cube material
enum  {
  EMPTY = 0, // nothing at all
  FULL = 1 // contains a deformed cube
};

// 6 textures (one per face)
typedef array<u16,6> cubetex;

// data contained in the most lower grid
struct brickcube {
  INLINE brickcube(u8 m) : tex(zero), p(zero), mat(m) {}
  INLINE brickcube(vec3<s8> o = vec3<s8>(zero), u8 m = EMPTY, cubetex tex = cubetex(zero)) :
    tex(tex), p(o), mat(m) {}
  INLINE bool isdefault(void) const {
    return mat==EMPTY && all(p==vec3<s8>(zero));
  }
  cubetex tex;
  vec3<s8> p;
  u8 mat;
};

INLINE bool operator!= (const brickcube &c0, const brickcube &c1) {
  return any(c0.p!= c1.p) || (c0.mat!=c1.mat) || (c0.tex!=c1.tex);
}
INLINE bool operator== (const brickcube &c0, const brickcube &c1) {
  return !(c0!=c1);
}

// empty (== air) cube and no displacement
static const brickcube emptycube;

#define loopxyz(org, end, body)\
  for (int X = vec3i(org).x; X < vec3i(end).x; ++X)\
  for (int Y = vec3i(org).y; Y < vec3i(end).y; ++Y)\
  for (int Z = vec3i(org).z; Z < vec3i(end).z; ++Z) do {\
    vec3i xyz(X,Y,Z);\
    body;\
  } while (0)

template <int x> struct powerof2policy {enum {value = !!((x&(x-1))==0)};};
template<size_t x>
struct log2 {
  enum {value = log2<x/2>::value+1};
  static_assert(powerof2policy<x>::value,"given value must be a power of 2");
};
template<> struct log2<1> {enum {value=0};};

// actually contains the data (geometries)
template <int sz>
struct brick : public noncopyable {
  static_assert(powerof2policy<sz>::value,"grid dimensions must be power of 2");\
  enum {
    cubenumber=sz,
    subcubenumber=1,
    l=sz
  };
  brick(void) : vbo(0), ibo(0), lm(0), dirty(1) {}
  ~brick(void) {
    if (ibo) ogl::deletebuffers(1,&ibo);
    if (vbo) ogl::deletebuffers(1,&vbo);
    if (lm)  ogl::deletetextures(1,&lm);
    lm=ibo=vbo=0;
  }
  static INLINE vec3i size(void) { return vec3i(sz); }
  static INLINE vec3i global(void) { return size(); }
  static INLINE vec3i local(void) { return size(); }
  static INLINE vec3i cuben(void) { return size(); }
  static INLINE vec3i subcuben(void) { return vec3i(one); }
  INLINE brickcube get(vec3i v) const { return elem[v.x][v.y][v.z]; }
  INLINE brickcube subgrid(vec3i v) const { return get(v); }
  INLINE brickcube fastsubgrid(vec3i v) const { return subgrid(v); }
  INLINE void set(vec3i v, const brickcube &cube) {
    dirty=1;
    elem[v.x][v.y][v.z]=cube;
  }
  INLINE brick &getbrick(vec3i idx) { return *this; }
  template <typename F> INLINE void forallcubes(const F &f, vec3i org) {
    loopxyz(zero, size(), {
      auto cube = get(xyz);
      if (!cube.isdefault()) f(cube, org + xyz);
    });
  }
  template <typename F> INLINE void forallbricks(const F &f, vec3i org) {
    f(*this, org);
  }
  template <typename F> INLINE void forallgrids(const F &f, vec3i org) {
    f(*this, org);
  }
  brickcube elem[sz][sz][sz];
  u32 vbo, ibo; // ogl handles for vertex and index buffers
  u32 lm; // light map
  vec2f rlmdim; // rcp(lightmap_dimension)
  vector<vec2i> draws; // (elemnum, texid)
  u32 dirty; // 1 if the ogl data need to be rebuilt
};

// recursive sparse grid
template <typename T, int loc, int glob>
struct grid : public noncopyable {
  typedef T childtype;
  enum {
    cubenumber=loc*T::cubenumber,
    subcubenumber=T::cubenumber,
    l=loc
  };
  INLINE grid(void) { dirty=1; memset(elem, 0, sizeof(elem)); }
  static INLINE vec3i local(void) { return vec3i(loc); }
  static INLINE vec3i global(void) { return vec3i(glob); }
  static INLINE vec3i cuben(void) { return vec3i(cubenumber); }
  static INLINE vec3i subcuben(void) { return T::cuben(); }
  INLINE vec3i index(vec3i p) const {
    return any(p<vec3i(zero))?local():p*local()/global();
  }
  INLINE T *subgrid(vec3i idx) const {
    if (any(idx>=local())) return NULL;
    return elem[idx.x][idx.y][idx.z];
  }
  INLINE T *fastsubgrid(vec3i idx) const {
    ASSERT(all(idx<local()) && all(idx>=vec3i(zero)));
    return elem[idx.x][idx.y][idx.z];
  }
  INLINE brickcube get(vec3i v) const {
    auto idx = index(v);
    auto e = subgrid(idx);
    if (e == NULL) return emptycube;
    return e->get(v-idx*subcuben());
  }
  INLINE void set(vec3i v, const brickcube &cube) {
    auto idx = index(v);
    if (any(idx>=local())) return;
    auto &e = elem[idx.x][idx.y][idx.z];
    if (e == NULL) e = NEWE(T);
    e->set(v-idx*subcuben(), cube);
    dirty=1;
  }
  template <typename F> INLINE void forallcubes(const F &f, vec3i org) {
    loopxyz(zero, local(), if (T *e = subgrid(xyz))
      e->forallcubes(f, org + xyz*global()/local()));
  }
  template <typename F> INLINE void forallbricks(const F &f, vec3i org) {
    loopxyz(zero, local(), if (T *e = subgrid(xyz))
      e->forallbricks(f, org + xyz*global()/local()););
  }
  template <typename F> INLINE void forallgrids(const F &f, vec3i org) {
    loopxyz(zero, local(), if (T *e = subgrid(xyz))
      e->forallgrids(f, org + xyz*global()/local()););
    f(*this,org);
  }
  T *elem[loc][loc][loc]; // each element may be null when empty
  u32 dirty:1; // true if anything changed in the child grids
  static_assert(powerof2policy<loc>::value,"grid dimensions must be power of 2");\
  static_assert(powerof2policy<glob>::value,"grid dimensions must be power of 2");\
};
#undef GRIDPOLICY

// all levels of details for our world
#define BRICKDIM 16
static const int lvl1 = BRICKDIM, lvl2 = 4, lvl3 = 4;
static const int lvlt1 = lvl1;
static const vec3i brickisize(lvl1);

// compute the total number of cubes for each level
#define LVL_TOT(N, M) static const int lvlt##N = lvl##N * lvlt##M;
LVL_TOT(2,1)
LVL_TOT(3,2)
#undef LVL_TOT

// define a type for one level of the hierarchy
#define GRID(CHILD,N)\
typedef grid<CHILD,lvl##N,lvlt##N> lvl##N##grid;
typedef brick<lvl1> lvl1grid;
GRID(lvl1grid,2)
GRID(lvl2grid,3)
#undef GRID

// our world and its total dimension
extern lvl3grid root;
static const int size=lvlt3;
static const vec3i isize(size);

template <typename F> static void forallgrids(const F &f) { root.forallgrids(f, zero); }
template <typename F> static void forallbricks(const F &f) { root.forallbricks(f, zero); }
template <typename F> static void forallcubes(const F &f) { root.forallcubes(f, zero); }

// get and set the cube at position (x,y,z)
brickcube getcube(const vec3i &xyz);
void setcube(const vec3i &xyz, const brickcube &cube);
INLINE bool visibleface(vec3i xyz, u32 face) {
  return getcube(xyz).mat != world::EMPTY &&
         getcube(xyz+cubenorms[face]).mat == world::EMPTY;
}
INLINE vec3f getpos(vec3i xyz) {return vec3f(xyz)+vec3f(world::getcube(xyz).p)/255.f;}
// cast a ray in the world and return the intersection result
isecres castray(const ray &ray);
void setup(int factor);
// used for edit mode ent display
int closestent(void);
int findentity(int type, int index = 0);
// trigger tag
void trigger(int tag, int type, bool savegame);
// reset for editing or map saving
void resettagareas(void);
// set for playing
void settagareas(void);
// create a new entity
game::entity *newentity(int x, int y, int z, char *what, int v1, int v2, int v3, int v4);
// save the world as .cgz file
void save(const char *fname);
// load the world from .cgz file
void load(const char *mname);
void writemap(const char *mname, int msize, u8 *mdata);
u8 *readmap(const char *mname, int *msize);
// test occlusion for a cube (v = viewer, c = cube to test)
int isoccluded(float vx, float vy, float cx, float cy, float csize);
// return the water level for the loaded map
int waterlevel(void);
// return the name of the current map
char *maptitle(void);
// free the world resources
void clean(void);
// build a bvh from the world
bvh::intersector *buildbvh(void);

} // namespace world
} // namespace cube

