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
    u32 offset_flag;
    union {
      u32 tri_id;
      u32 d;
    };
    INLINE u32 is_leaf() const { return offset_flag == 0; }
    INLINE u32 get_offset() const { return offset_flag; }
  };
  node *root;
  std::vector<waldtriangle> acc;
};

static const int remap_other_axis[4] = {1, 2, 0, 1};
enum {other_axis_n = 2};
struct centroid : public vec3f {
  centroid() {}
  centroid(const triangle &t) : vec3f(t.v[0]+t.v[1]+t.v[2]) {}
};

enum {ONLEFT = 0, ONRIGHT};

// n log(n) compiler with bounding box sweeping and SAH heuristics
struct compiler {
  compiler(void) : curr_id(0) {}
  int injection(const triangle *soup, u32 tri_n);
  int compile(void);
  std::vector<int> pos;
  std::vector<u32> ids[3];
  std::vector<u32> tmp_ids;
  std::vector<aabb> aabbs;
  std::vector<aabb> rl_aabbs;
  waldtriangle *acc;
  intersector::node *root;
  int n;
  u32 curr_id;
  aabb scene_aabb;
};

template<u32 axis> struct sorter_t : public std::binary_function<int,int,bool> {
  const std::vector<centroid> &centroids;
  sorter_t(const std::vector<centroid> &c) : centroids(c) {}
  INLINE int operator() (const u32 a, const u32 b) const  {
    return centroids[a][axis] < centroids[b][axis];
  }
};

int compiler::injection(const triangle * const soup, const u32 tri_n) {
  std::vector<centroid> centroids;

  root = (intersector::node *) new intersector::node[2*tri_n+1];
  if (root == NULL) return -1;

  loopi(3) {
    ids[i].resize(tri_n);
    if (ids[i].size() < tri_n) return -1;
  }
  pos.resize(tri_n);
  tmp_ids.resize(tri_n);
  centroids.resize(tri_n);
  aabbs.resize(tri_n);
  rl_aabbs.resize(tri_n);
  n = tri_n;

  scene_aabb = aabb(FLT_MAX, -FLT_MAX);
  loopj(n) {
    centroids[j] = centroid(soup[j]);
    aabbs[j] = soup[j].get_aabb();
    scene_aabb.compose(aabbs[j]);
  }

  loopi(3) loopj(n) ids[i][j] = j;
  std::sort(ids[0].begin(), ids[0].end(), sorter_t<0>(centroids));
  std::sort(ids[1].begin(), ids[1].end(), sorter_t<1>(centroids));
  std::sort(ids[2].begin(), ids[2].end(), sorter_t<2>(centroids));

  return n;
}

struct stack_data_t {
  stack_data_t(void) {}
  stack_data_t(int f, int l, u32 index, const aabb &box) :
    first(f), last(l), id(index), box(box) {}
  int first, last;
  u32 id;
  aabb box;
  INLINE u32 is_leaf() { return first == last; }
};

// grows up the bounding boxen to avoid precision problems
static const float aabb_eps = 1e-6f;

struct stack_t {
  enum { max_size = 64 };
  stack_t() { n = 0; }
  INLINE void push(int a, int b, u32 id, const aabb &box) {
    data[n++] = stack_data_t(a, b, id, box);
  }
  INLINE stack_data_t pop() { return data[--n]; }
  INLINE u32 is_not_empty() { return n != 0; }
private:
  int n;
  stack_data_t data[max_size];
};

struct partition {
  aabb aabbs[2];
  float cost;
  u32 axis;
  int first[2], last[2];
  partition() {}
  partition(int f, int l, u32 d) : cost(FLT_MAX), axis(d) {
    aabbs[ONLEFT] = aabbs[ONRIGHT] = aabb(FLT_MAX, -FLT_MAX);
    first[ONRIGHT] = first[ONLEFT] = f;
    last[ONRIGHT] = last[ONLEFT] = l;
  }
};

// sweep the bounding boxen from left to right
template <u32 axis> INLINE partition do_sweep(compiler &c, int first, int last) {
  partition part(first, last, axis);

  // compute the inclusion sequence
  c.rl_aabbs[c.ids[axis][last]] = c.aabbs[c.ids[axis][last]];
  for (int j  = last - 1; j >= first; --j) {
    c.rl_aabbs[c.ids[axis][j]] = c.aabbs[c.ids[axis][j]];
    c.rl_aabbs[c.ids[axis][j]].compose(c.rl_aabbs[c.ids[axis][j + 1]]);
  }

  //  sweep from left to right and find the best partition
  aabb box(FLT_MAX, -FLT_MAX);
  int tri_n = (last-first)+1, n = 1;
  part.cost = FLT_MAX;
  for (int j = first; j < last; ++j) {
    box.compose(c.aabbs[c.ids[axis][j]]);
    const float cost = box.halfarea()*n + c.rl_aabbs[c.ids[axis][j+1]].halfarea()*(tri_n-n);
    n++;
    if (cost > part.cost) continue;
    part.cost = cost;
    part.last[ONLEFT] = j;
    part.first[ONRIGHT] = j + 1;
    part.aabbs[ONLEFT] = box;
    part.aabbs[ONRIGHT] = c.rl_aabbs[c.ids[axis][j + 1]];
  }
  return part;
}

INLINE void do_make_node(compiler &c, const stack_data_t &data, u32 axis) {
  c.root[data.id].d = axis;
  c.root[data.id].box = data.box;
  c.root[data.id].offset_flag = (c.curr_id + 1);
}

INLINE void do_make_leaf(compiler &c, const stack_data_t &data) {
  c.root[data.id].box = data.box;
  c.root[data.id].offset_flag = 0;
  c.root[data.id].tri_id = c.ids[0][data.first];
}

INLINE void do_grow_aabbs(compiler &c) {
  const int aabbn = 2 * c.n - 1;
  const vec3f eps_vec(aabb_eps, aabb_eps, aabb_eps);
  loopi(aabbn) {
    c.root[i].box.pmin = c.root[i].box.pmin - eps_vec;
    c.root[i].box.pmax = c.root[i].box.pmax + eps_vec;
  }
}

int compiler::compile(void) {
  stack_t stack;
  stack_data_t node;

  stack.push(0, n - 1, 0, scene_aabb);
  while (stack.is_not_empty()) {
    node = stack.pop();
    while (!node.is_leaf()) {

      // find the best partition for this node
      partition best = do_sweep<0>(*this, node.first, node.last);
      partition part = do_sweep<1>(*this, node.first, node.last);
      if (part.cost < best.cost) best = part;
      part = do_sweep<2>(*this, node.last, node.last);
      if (part.cost < best.cost) best = part;

      // register this node
      do_make_node(*this, node, best.axis);

      // first, store the positions of the primitives
      const int d = best.axis;
      for (int j = best.first[ONLEFT]; j <= best.last[ONLEFT]; ++j)
        pos[ids[d][j]] = ONLEFT;
      for (int j = best.first[ONRIGHT]; j <= best.last[ONRIGHT]; ++j)
        pos[ids[d][j]] = ONRIGHT;

      // then, for each axis, reorder the indices for the next step
      int left_n, right_n;
      loopi(other_axis_n) {
        const int d0 = remap_other_axis[best.axis + i];
        left_n = 0, right_n = 0;
        for (int j = node.first; j <= node.last; ++j)
          if (pos[ids[d0][j]] == ONLEFT)
            ids[d0][node.first + left_n++] = ids[d0][j];
          else
            tmp_ids[right_n++] = ids[d0][j];
        for (int j = node.first + left_n; j <= node.last; ++j)
          ids[d0][j] = tmp_ids[j - left_n - node.first];
      }

      // prepare the stack data for the next step
      const int p0 = right_n > left_n ? ONLEFT : ONRIGHT;
      const int p1 = right_n > left_n ? ONRIGHT : ONLEFT;
      stack.push(best.first[p1], best.last[p1], curr_id+p1+1, best.aabbs[p1]);
      node.first = best.first[p0];
      node.last = best.last[p0];
      node.box = best.aabbs[p0];
      node.id = curr_id+p0+1;
      curr_id += 2;
    }

    do_make_leaf(*this, node);
  }

  do_grow_aabbs(*this);
  return 0;
}

static void computewaldtriangle(const triangle &t, waldtriangle &w, u32 id, u32 matid) {
  const vec3f &A(t.v[0]), &B(t.v[1]), &C(t.v[2]);
  const vec3f b(C-A), c(B-A), N(cross(b,c));
  u32 k = 0;
  for (u32 i=1; i<3; ++i) k = fabsf(N[i]) > fabsf(N[k]) ? i : k;
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
  compiler c;
  if (c.injection(tri, n) < 0) return NULL;
  c.compile();
  auto tree = new intersector;
  tree->root = c.root;
  tree->acc.resize(n);
  loopi(n) computewaldtriangle(tri[i], tree->acc[i], i, 0);
  return tree;
}

void destroy(intersector *bvhtree) {
  if (bvhtree == NULL) return;
  if (bvhtree->root != NULL) delete[] bvhtree->root;
  delete bvhtree;
}

static const u32 wald_modulo[] = {1, 2, 0, 1};

INLINE void raytriangle(const waldtriangle &tri, const ray &r, hit &hit) {
  const u32 k = tri.k, ku = wald_modulo[k], kv = wald_modulo[k+1];
  const vec2f dirk(r.dir[ku], r.dir[kv]);
  const vec2f posk(r.org[ku], r.org[kv]);
  const float t = (tri.nd-r.org[k]-dot(tri.n,posk))/(r.dir[k]+dot(tri.n,dirk));
  if (!((hit.t > t) & (t >= 0.f))) return;
  const vec2f h = posk+ t*dirk - tri.vertk;
  const float beta = dot(h,tri.bn), gamma = dot(h,tri.cn);
  if ((beta < 0.f) | (gamma < 0.f) | ((beta + gamma) > 1.f)) return;
  hit.t = t;
  hit.u = beta;
  hit.v = gamma;
  hit.id = tri.id;
}

void trace(const intersector &bvhtree, const ray &r, hit &hit) {
  const s32 signs_all[3] = {(r.dir.x>=0.f)&1, (r.dir.y>=0.f)&1, (r.dir.z>=0.f)&1};
  const intersector::node *stack[64];
  stack[0] = bvhtree.root;
  u32 stacksz = 1;

  while (stacksz) {
    const intersector::node *node = stack[--stacksz];
    while (slab(node->box, r.org, r.rdir, hit.t).isec) {
      if (node->is_leaf()) {
        raytriangle(bvhtree.acc[node->tri_id], r, hit);
        break;
      } else {
        const s32 next = signs_all[node->d];
        const s32 first = ~next & 1;
        const intersector::node *left = &bvhtree.root[node->offset_flag+next];
        stack[stacksz++] = left;
        node = &bvhtree.root[node->offset_flag + first];
      }
    }
  }
}
} // namespace bvh
} // namespace cube

