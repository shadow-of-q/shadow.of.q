#pragma once
#include "entities.hpp"
#include "tools.hpp"
#include "math.hpp"

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
typedef vvec<u16,6> cubetex;

// data contained in the most lower grid
struct brickcube {
  INLINE brickcube(u8 m) : tex(zero), p(zero), mat(m) {}
  INLINE brickcube(vec3<s8> o = vec3<s8>(zero), u8 m = EMPTY, cubetex tex = cubetex(zero)) :
    tex(tex), p(o), mat(m) {}
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

template <int x> struct powerof2policy { enum {value = !!((x&(x-1))==0)}; };
#define GRIDPOLICY(x,y,z) \
  static_assert(powerof2policy<x>::value,"x must be power of 2");\
  static_assert(powerof2policy<y>::value,"y must be power of 2");\
  static_assert(powerof2policy<z>::value,"z must be power of 2");

// actually contains the data (geometries)
template <int x, int y, int z>
struct brick : public noncopyable {
  INLINE brick(void) {
    vbo = ibo = 0u;
    elemnum = 0;
    dirty = 1;
  }
  static INLINE vec3i size(void) { return vec3i(x,y,z); }
  static INLINE vec3i global(void) { return size(); }
  static INLINE vec3i local(void) { return size(); }
  static INLINE vec3i cuben(void) { return size(); }
  static INLINE vec3i subcuben(void) { return vec3i(one); }
  INLINE brickcube get(vec3i v) const { return elem[v.x][v.y][v.z]; }
  INLINE brickcube subgrid(vec3i v) const { return get(v); }
  INLINE void set(vec3i v, const brickcube &cube) {
    dirty=1;
    elem[v.x][v.y][v.z]=cube;
  }
  INLINE brick &getbrick(vec3i idx) { return *this; }
  template <typename F> INLINE void forallcubes(const F &f, vec3i org) {
    loopxyz(zero, size(), {
      auto cube = get(xyz);
      if (cube.mat != EMPTY)
        f(cube, org + xyz);
    });
  }
  template <typename F> INLINE void forallbricks(const F &f, vec3i org) {
    f(*this, org);
  }
  brickcube elem[x][y][z];
  GRIDPOLICY(x,y,z);
  enum {cubenx=x, cubeny=y, cubenz=z};
  u32 vbo, ibo;
  u32 elemnum:31; // number of elements to draw in the vbo
  u32 dirty:1; // true if the ogl data need to be rebuilt
};

// recursive sparse grid
template <typename T, int locx, int locy, int locz, int globx, int globy, int globz>
struct grid : public noncopyable {
  typedef T childtype;
  enum {cubenx=locx*T::cubenx, cubeny=locy*T::cubeny, cubenz=locz*T::cubenz};
  INLINE grid(void) { memset(elem, 0, sizeof(elem)); }
  static INLINE vec3i local(void) { return vec3i(locx,locy,locz); }
  static INLINE vec3i global(void) { return vec3i(globx,globy,globz); }
  static INLINE vec3i cuben(void) { return vec3i(cubenx, cubeny, cubenz); }
  static INLINE vec3i subcuben(void) { return T::cuben(); }
  INLINE vec3i index(vec3i p) const {
    return any(p<vec3i(zero))?local():p*local()/global();
  }
  INLINE T *subgrid(vec3i idx) const {
    if (any(idx>=local())) return NULL;
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
    if (e == NULL) e = new T;
    e->set(v-idx*subcuben(), cube);
  }
  template <typename F> INLINE void forallcubes(const F &f, vec3i org) {
    loopxyz(zero, local(), if (T *e = subgrid(xyz))
      e->forallcubes(f, org + xyz*global()/local()));
  }
  template <typename F> INLINE void forallbricks(const F &f, vec3i org) {
    loopxyz(zero, local(), if (T *e = subgrid(xyz))
      e->forallbricks(f, org + xyz*global()/local()););
  }
  T *elem[locx][locy][locz]; // each element may be null when empty
  u32 dirty:1; // true if anything changed in the child grids
  GRIDPOLICY(locx,locy,locz);
  GRIDPOLICY(globx,globy,globz);
};
#undef GRIDPOLICY

// all levels of details for our world
static const int lvl1x = 16, lvl1y = 16, lvl1z = 16;
static const int lvl2x = 4,  lvl2y = 4,  lvl2z = 4;
static const int lvl3x = 4,  lvl3y = 4,  lvl3z = 4;
static const int lvlt1x = lvl1x, lvlt1y = lvl1y, lvlt1z = lvl1z;
static const vec3i brickisize(lvl1x, lvl1y, lvl1z);

// compute the total number of cubes for each level
#define LVL_TOT(N, M)\
static const int lvlt##N##x = lvl##N##x * lvlt##M##x;\
static const int lvlt##N##y = lvl##N##y * lvlt##M##y;\
static const int lvlt##N##z = lvl##N##z * lvlt##M##z;
LVL_TOT(2,1)
LVL_TOT(3,2)
#undef LVL_TOT

// define a type for one level of the hierarchy
#define GRID(CHILD,N)\
typedef grid<CHILD,lvl##N##x,lvl##N##y,lvl##N##z,lvlt##N##x,lvlt##N##y,lvlt##N##z> lvl##N##grid;
typedef brick<lvl1x,lvl1y,lvl1z> lvl1grid;
GRID(lvl1grid,2)
GRID(lvl2grid,3)
#undef GRID

// our world and its total dimension
extern lvl3grid root;
static const int sizex=lvlt3x, sizey=lvlt3y, sizez=lvlt3z;
static const vec3i isize(sizex,sizey,sizez);
static const vec3f fsize(sizex,sizey,sizez);

template <typename F> static void forallbricks(const F &f) { root.forallbricks(f, zero); }
template <typename F> static void forallcubes(const F &f) { root.forallcubes(f, zero); }

// get and set the cube at position (x,y,z)
brickcube getcube(const vec3i &xyz);
void setcube(const vec3i &xyz, const brickcube &cube);
INLINE vec3f getpos(vec3i xyz) {return vec3f(xyz)+vec3f(world::getcube(xyz).p)/256.f;}

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
void writemap(const char *mname, int msize, uchar *mdata);
uchar *readmap(const char *mname, int *msize);
// test occlusion for a cube (v = viewer, c = cube to test)
int isoccluded(float vx, float vy, float cx, float cy, float csize);
// return the water level for the loaded map
int waterlevel(void);
// return the name of the current map
char *maptitle(void);

} // namespace world
} // namespace cube

