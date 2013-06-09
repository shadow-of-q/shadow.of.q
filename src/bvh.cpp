#include <memory>
#include <cstring>
#include <functional>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>
#include "tools.hpp"
#include "math.hpp"
#include "bvh.hpp"

namespace cube {
namespace bvh {

struct waldtriangle {
  vec2f bn, cn, vertk, n;
  float nd;
  u32 k, id, matid;
};

struct intersector {
  struct node {
    aabb box;
    u32 offsetflag;
    union {
      u32 triid;
      u32 d;
    };
    INLINE u32 isleaf(void) const { return offsetflag == 0; }
    INLINE u32 getoffset(void) const { return offsetflag; }
  };
  node *root;
  std::vector<waldtriangle> acc;
};

struct centroid : public vec3f {
  INLINE centroid(const triangle &t) : vec3f(t.v[0]+t.v[1]+t.v[2]) {}
  INLINE centroid(void) {}
};

enum {OTHERAXISNUM = 2};
enum {ONLEFT, ONRIGHT};

// n log(n) compiler with bounding box sweeping and SAH heuristics
struct compiler {
  compiler(void) : currid(0) {}
  int injection(const triangle *soup, u32 trinum);
  int compile(void);
  std::vector<int> pos;
  std::vector<u32> ids[3];
  std::vector<u32> tmpids;
  std::vector<aabb> boxes;
  std::vector<aabb> rlboxes;
  waldtriangle *acc;
  intersector::node *root;
  int n;
  u32 currid;
  aabb scenebox;
};

template<u32 axis> struct sorter : public std::binary_function<int,int,bool> {
  const std::vector<centroid> &centroids;
  sorter(const std::vector<centroid> &c) : centroids(c) {}
  INLINE int operator() (const u32 a, const u32 b) const  {
    return centroids[a][axis] < centroids[b][axis];
  }
};

int compiler::injection(const triangle *soup, const u32 trinum) {
  std::vector<centroid> centroids;

  root = NEWAE(intersector::node,2*trinum+1);
  if (root == NULL) return -1;

  loopi(3) ids[i].resize(trinum);
  pos.resize(trinum);
  tmpids.resize(trinum);
  centroids.resize(trinum);
  boxes.resize(trinum);
  rlboxes.resize(trinum);
  n = trinum;

  scenebox = aabb(FLT_MAX, -FLT_MAX);
  loopj(n) {
    centroids[j] = centroid(soup[j]);
    boxes[j] = soup[j].getaabb();
    scenebox.compose(boxes[j]);
  }

  loopi(3) loopj(n) ids[i][j] = j;
  std::sort(ids[0].begin(), ids[0].end(), sorter<0>(centroids));
  std::sort(ids[1].begin(), ids[1].end(), sorter<1>(centroids));
  std::sort(ids[2].begin(), ids[2].end(), sorter<2>(centroids));

  return n;
}

struct partition {
  aabb boxes[2];
  float cost;
  u32 axis;
  int first[2], last[2];
  partition() {}
  partition(int f, int l, u32 d) : cost(FLT_MAX), axis(d) {
    boxes[ONLEFT] = boxes[ONRIGHT] = aabb(FLT_MAX, -FLT_MAX);
    first[ONRIGHT] = first[ONLEFT] = f;
    last[ONRIGHT] = last[ONLEFT] = l;
  }
};

// sweep the bounding boxen from left to right
template <u32 axis> INLINE partition sweep(compiler &c, int first, int last) {
  partition part(first, last, axis);

  // compute the inclusion sequence
  c.rlboxes[c.ids[axis][last]] = c.boxes[c.ids[axis][last]];
  for (int j  = last - 1; j >= first; --j) {
    c.rlboxes[c.ids[axis][j]] = c.boxes[c.ids[axis][j]];
    c.rlboxes[c.ids[axis][j]].compose(c.rlboxes[c.ids[axis][j + 1]]);
  }

  //  sweep from left to right and find the best partition
  aabb box(FLT_MAX, -FLT_MAX);
  int trinum = (last-first)+1, n = 1;
  part.cost = FLT_MAX;
  for (int j = first; j < last; ++j) {
    box.compose(c.boxes[c.ids[axis][j]]);
    const float cost = box.halfarea()*n + c.rlboxes[c.ids[axis][j+1]].halfarea()*(trinum-n);
    n++;
    if (cost > part.cost) continue;
    part.cost = cost;
    part.last[ONLEFT] = j;
    part.first[ONRIGHT] = j + 1;
    part.boxes[ONLEFT] = box;
    part.boxes[ONRIGHT] = c.rlboxes[c.ids[axis][j + 1]];
  }
  return part;
}

struct segment {
  segment(void) {}
  segment(s32 first, s32 last, u32 id, const aabb &box) :
    first(first), last(last), id(id), box(box) {}
  s32 first, last;
  u32 id;
  aabb box;
  INLINE u32 isleaf() { return first == last; }
};

INLINE void makenode(compiler &c, const segment &data, u32 axis) {
  c.root[data.id].d = axis;
  c.root[data.id].box = data.box;
  c.root[data.id].offsetflag = (c.currid + 1);
}

INLINE void makeleaf(compiler &c, const segment &data) {
  c.root[data.id].box = data.box;
  c.root[data.id].offsetflag = 0;
  c.root[data.id].triid = c.ids[0][data.first];
}

INLINE void growboxes(compiler &c) {
  const float aabbeps = 1e-6f;
  loopi(2*c.n-1) {
    c.root[i].box.pmin = c.root[i].box.pmin - vec3f(aabbeps);
    c.root[i].box.pmax = c.root[i].box.pmax + vec3f(aabbeps);
  }
}

int compiler::compile(void) {
  segment node;
  segment stack[64];
  u32 stacksz = 1;
  stack[0] = segment(0,n-1,0,scenebox);

  while (stacksz) {
    node = stack[--stacksz];
    while (!node.isleaf()) {

      // find the best partition for this node
      partition best = sweep<0>(*this, node.first, node.last);
      partition part = sweep<1>(*this, node.first, node.last);
      if (part.cost < best.cost) best = part;
      part = sweep<2>(*this, node.last, node.last);
      if (part.cost < best.cost) best = part;

      // register this node
      makenode(*this, node, best.axis);

      // first, store the positions of the primitives
      for (int j = best.first[ONLEFT]; j <= best.last[ONLEFT]; ++j)
        pos[ids[best.axis][j]] = ONLEFT;
      for (int j = best.first[ONRIGHT]; j <= best.last[ONRIGHT]; ++j)
        pos[ids[best.axis][j]] = ONRIGHT;

      // then, for each axis, reorder the indices for the next step
      int leftnum, rightnum;
      loopi(OTHERAXISNUM) {
        const int otheraxis[] = {1,2,0,1};
        const int d0 = otheraxis[best.axis + i];
        leftnum = 0, rightnum = 0;
        for (int j = node.first; j <= node.last; ++j)
          if (pos[ids[d0][j]] == ONLEFT)
            ids[d0][node.first + leftnum++] = ids[d0][j];
          else
            tmpids[rightnum++] = ids[d0][j];
        for (int j = node.first + leftnum; j <= node.last; ++j)
          ids[d0][j] = tmpids[j - leftnum - node.first];
      }

      // prepare the stack data for the next step
      const int p0 = rightnum > leftnum ? ONLEFT : ONRIGHT;
      const int p1 = rightnum > leftnum ? ONRIGHT : ONLEFT;
      stack[stacksz++] = segment(best.first[p1], best.last[p1], currid+p1+1, best.boxes[p1]);
      node.first = best.first[p0];
      node.last = best.last[p0];
      node.box = best.boxes[p0];
      node.id = currid+p0+1;
      currid += 2;
    }

    makeleaf(*this, node);
  }

  growboxes(*this);
  return 0;
}

INLINE void computewaldtriangle(const triangle &t, waldtriangle &w, u32 id, u32 matid) {
  const vec3f &A(t.v[0]), &B(t.v[1]), &C(t.v[2]);
  const vec3f b(C-A), c(B-A), N(cross(b,c));
  u32 k = 0;
  for (u32 i=1; i<3; ++i) k = abs(N[i]) > abs(N[k]) ? i : k;
  const u32 u = (k+1)%3, v = (k+2)%3;
  const float denom = (b[u]*c[v] - b[v]*c[u]), krec = N[k];
  w.n = vec2f(N[u]/krec, N[v]/krec);
  w.bn = vec2f(-b[v]/denom, b[u]/denom);
  w.cn = vec2f(c[v]/denom, -c[u]/denom);
  w.vertk = vec2f(A[u], A[v]);
  w.nd = dot(N,A)/krec;
  w.id = id;
  w.k = k;
  w.matid = matid;
}

intersector *create(const triangle *tri, int n) {
  if (n==0) return NULL;
  compiler c;
  if (c.injection(tri, n) < 0) return NULL;
  c.compile();
  auto tree = NEWE(intersector);
  tree->root = c.root;
  tree->acc.resize(n);
  loopi(n) computewaldtriangle(tri[i], tree->acc[i], i, 0);
  return tree;
}

void destroy(intersector *bvhtree) {
  if (bvhtree == NULL) return;
  SAFE_DELETEA(bvhtree->root);
  SAFE_DELETE(bvhtree);
}

static const u32 waldmodulo[] = {1,2,0,1};

INLINE void raytriangle(const waldtriangle &tri, const ray &r, hit &hit) {
  const u32 k = tri.k, ku = waldmodulo[k], kv = waldmodulo[k+1];
  const vec2f dirk(r.dir[ku], r.dir[kv]);
  const vec2f posk(r.org[ku], r.org[kv]);
  const float t = (tri.nd-r.org[k]-dot(tri.n,posk))/(r.dir[k]+dot(tri.n,dirk));
  if (!((hit.t > t) & (t >= 0.f))) return;
  const vec2f h = posk + t*dirk - tri.vertk;
  const float beta = dot(h,tri.bn), gamma = dot(h,tri.cn);
  if ((beta < 0.f) | (gamma < 0.f) | ((beta + gamma) > 1.f)) return;
  hit.t = t;
  hit.u = beta;
  hit.v = gamma;
  hit.id = tri.id;
}

void trace(const intersector &bvhtree, const ray &r, hit &hit) {
  const s32 signs[3] = {(r.dir.x>=0.f)&1, (r.dir.y>=0.f)&1, (r.dir.z>=0.f)&1};
  const intersector::node *stack[64];
  stack[0] = bvhtree.root;
  u32 stacksz = 1;

  while (stacksz) {
    const intersector::node *node = stack[--stacksz];
    while (slab(node->box, r.org, r.rdir, hit.t).isec) {
      if (node->isleaf()) {
        raytriangle(bvhtree.acc[node->triid], r, hit);
        break;
      } else {
        const s32 second = signs[node->d];
        const s32 first = second^1;
        stack[stacksz++] = &bvhtree.root[node->offsetflag+second];
        node = &bvhtree.root[node->offsetflag+first];
      }
    }
  }
}
} // namespace bvh
} // namespace cube

