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

struct wald_tri_t {
  u32 k;
  float    n_u;
  float    n_v;
  float    n_d;
  float    vert_ku;
  float    vert_kv;
  float    b_nu;
  float    b_nv;
  float    c_nu;
  float    c_nv;
  u32 id, matid;
};

typedef u32 node_id_t;
typedef u32 tri_id_t;
struct node_t {
  aabb_t aabb;
  node_id_t offset_flag;
  union {
    tri_id_t tri_id;
    u32 d;
  };
  INLINE u32 is_leaf() const { return offset_flag == 0; }
  INLINE u32 get_offset() const { return offset_flag; }
};

struct intersector {
  node_t* root;
  std::vector<wald_tri_t> acc;
};

static const int remap_other_axis[4] = {1, 2, 0, 1};
enum {other_axis_n = 2};
struct centroid_t : public vec3f {
  centroid_t() {}
  centroid_t(const triangle &t) : vec3f(
      t.v[0].x + t.v[1].x + t.v[2].x,
      t.v[0].y + t.v[1].y + t.v[2].y,
      t.v[0].z + t.v[1].z + t.v[2].z) {}
};

enum {on_left = 0, on_right};

// n log(n) compiler with bounding box sweeping and SAH heuristics
struct compiler_t {
  std::vector<int> pos;
  std::vector<u32> ids[3];
  std::vector<u32> tmp_ids;
  std::vector<aabb_t> aabbs;
  std::vector<aabb_t> rl_aabbs;
  int n;
  u32 curr_id;
  aabb_t scene_aabb;
  wald_tri_t * __restrict acc;
  node_t * __restrict root;
  compiler_t() { curr_id = 0; }
  int injection(const triangle * const soup, u32 tri_n);
  int compile();
};

template<u32 axis> struct sorter_t : public std::binary_function<int,int,bool> {
  const std::vector<centroid_t> &centroids;
  sorter_t(const std::vector<centroid_t> &c) : centroids(c) {}
  INLINE int operator() (const u32 a, const u32 b) const  {
    return centroids[a][axis] <  centroids[b][axis];
  }
};

int compiler_t::injection(const triangle * const soup, const u32 tri_n) {
  std::vector<centroid_t> centroids;

  root = (node_t *) new node_t [2 * tri_n + 1];
  if (root == 0) {
    return -1;
  }

  for(int i = 0; i < 3; ++i) {
    ids[i].resize(tri_n);
    if(ids[i].size() < tri_n)
      return -1;
  }
  pos.resize(tri_n);
  tmp_ids.resize(tri_n);
  centroids.resize(tri_n);
  aabbs.resize(tri_n);
  rl_aabbs.resize(tri_n);
  if(centroids.size() < tri_n || aabbs.size() < tri_n
      || rl_aabbs.size() < tri_n  || pos.size() < tri_n
      || tmp_ids.size() < tri_n)
    return -1;
  n = tri_n;

  scene_aabb = aabb_t(FLT_MAX, -FLT_MAX);
  for(int j = 0; j < n; ++j) {
    centroids[j] = centroid_t(soup[j]);
    aabbs[j] = soup[j].get_aabb();
    scene_aabb.compose(aabbs[j]);
  }

  for(int i = 0; i < 3; ++i)
    for(int j = 0; j < n; ++j)
      ids[i][j] = j;
  std::sort(ids[0].begin(), ids[0].end(), sorter_t<0>(centroids));
  std::sort(ids[1].begin(), ids[1].end(), sorter_t<1>(centroids));
  std::sort(ids[2].begin(), ids[2].end(), sorter_t<2>(centroids));

  return n;
}

struct stack_data_t {
  int first, last;
  u32 id;
  aabb_t aabb;
  stack_data_t() {}
  stack_data_t(int f, int l, u32 index, const aabb_t &paabb)
    : first(f), last(l), id(index), aabb(paabb) {}
  INLINE u32 is_leaf() {
    return first == last;
  }
};

/* Grows up the bounding boxen to avoid precision problems */
static const float aabb_eps = 1e-6f;

/* The stack of nodes to process */
struct stack_t {
  enum { max_size = 64 };
  stack_t() { n = 0; }
  INLINE void push(int a, int b, u32 id, const aabb_t &aabb) {
    data[n++] = stack_data_t(a, b, id, aabb);
  }
  INLINE stack_data_t pop() {
    return data[--n];
  }
  INLINE u32 is_not_empty() {
    return n != 0;
  }
  private:
  int n;
  stack_data_t data[max_size];
};

/* A partition of the current given sub-array */
struct partition_t {
  aabb_t aabbs[2];
  float cost;
  u32 axis;
  int first[2], last[2];
  partition_t() {}
  partition_t(int f, int l, u32 d) :
    cost(FLT_MAX), axis(d) {
      aabbs[on_left] = aabbs[on_right] = aabb_t(FLT_MAX, -FLT_MAX);
      first[on_right] = first[on_left] = f;
      last[on_right] = last[on_left] = l;
    }
};

/* Sweep the bounding boxen from left to right */
template <u32 axis> INLINE partition_t do_sweep(compiler_t &c, int first, int last) {
  // we return the best partition
  partition_t part(first, last, axis);

  // compute the inclusion sequence
  c.rl_aabbs[c.ids[axis][last]] = c.aabbs[c.ids[axis][last]];
  for(int j  = last - 1; j >= first; --j) {
    c.rl_aabbs[c.ids[axis][j]] = c.aabbs[c.ids[axis][j]];
    c.rl_aabbs[c.ids[axis][j]].compose(c.rl_aabbs[c.ids[axis][j + 1]]);
  }

  //  sweep from left to right and find the best partition
  aabb_t aabb(FLT_MAX, -FLT_MAX);
  const float tri_n = (float) (last - first) + 1.f;
  float n = 1.f;
  part.cost = FLT_MAX;
  for(int j = first; j < last; ++j) {
    aabb.compose(c.aabbs[c.ids[axis][j]]);
    const float cost = aabb.half_surface_area() * n
      + c.rl_aabbs[c.ids[axis][j + 1]].half_surface_area() * (tri_n - n);
    n += 1.f;
    if(cost > part.cost) continue;
    part.cost = cost;
    part.last[on_left] = j;
    part.first[on_right] = j + 1;
    part.aabbs[on_left] = aabb;
    part.aabbs[on_right] = c.rl_aabbs[c.ids[axis][j + 1]];
  }
  return part;
}

INLINE void do_make_node(compiler_t &compiler, const stack_data_t &data, u32 axis) {
  compiler.root[data.id].d = axis;
  compiler.root[data.id].aabb = data.aabb;
  compiler.root[data.id].offset_flag = (compiler.curr_id + 1);
}

INLINE void do_make_leaf(compiler_t &compiler, const stack_data_t &data) {
  compiler.root[data.id].aabb = data.aabb;
  compiler.root[data.id].offset_flag = 0;
  compiler.root[data.id].tri_id = compiler.ids[0][data.first];
}

INLINE void do_grow_aabbs(compiler_t &compiler) {
  const int aabb_n = 2 * compiler.n - 1;
  const vec3f eps_vec(aabb_eps, aabb_eps, aabb_eps);
  for(int i = 0; i < aabb_n; ++i) {
    compiler.root[i].aabb.pmin = compiler.root[i].aabb.pmin - eps_vec;
    compiler.root[i].aabb.pmax = compiler.root[i].aabb.pmax + eps_vec;
  }
}

int compiler_t::compile(void) {
  stack_t stack;
  stack_data_t node;

  stack.push(0, n - 1, 0, scene_aabb);
  while(stack.is_not_empty()) {
    node = stack.pop();
    while(!node.is_leaf()) {

      /* Find the best partition for this node */
      partition_t best = do_sweep<0>(*this, node.first, node.last);
      partition_t part = do_sweep<1>(*this, node.first, node.last);
      if(part.cost < best.cost) best = part;
      part = do_sweep<2>(*this, node.last, node.last);
      if(part.cost < best.cost) best = part;

      /* Register this node */
      do_make_node(*this, node, best.axis);

      /* First, store the positions of the primitives */
      const int d = best.axis;
      for(int j = best.first[on_left]; j <= best.last[on_left]; ++j)
        pos[ids[d][j]] = on_left;
      for(int j = best.first[on_right]; j <= best.last[on_right]; ++j)
        pos[ids[d][j]] = on_right;

      /* Then, for each axis, reorder the indices for the next step */
      int left_n, right_n;
      for(int i = 0; i < other_axis_n; ++i) {
        const int d0 = remap_other_axis[best.axis + i];
        left_n = 0, right_n = 0;
        for(int j = node.first; j <= node.last; ++j)
          if(pos[ids[d0][j]] == on_left)
            ids[d0][node.first + left_n++] = ids[d0][j];
          else
            tmp_ids[right_n++] = ids[d0][j];
        for(int j = node.first + left_n; j <= node.last; ++j)
          ids[d0][j] = tmp_ids[j - left_n - node.first];
      }

      /* Now, prepare the stack data for the next step */
      const int p0 = right_n > left_n ? on_left : on_right;
      const int p1 = right_n > left_n ? on_right : on_left;
      stack.push(best.first[p1], best.last[p1],
          curr_id + p1 + 1, best.aabbs[p1]);
      node.first = best.first[p0];
      node.last = best.last[p0];
      node.aabb = best.aabbs[p0];
      node.id = curr_id + p0 + 1;
      curr_id += 2;
    }

    /* Register this leaf */
    do_make_leaf(*this, node);
  }

  do_grow_aabbs(*this); 
  return 0;
}

/* Convert one triangle into a Baduel triangles */
static u32 compute_tri_acc(const triangle &t, wald_tri_t &w, u32 id, u32 matid)
{
  const vec3f
    &A(t.v[0]), &B(t.v[1]), &C(t.v[2]),
  b(C - A), c(B - A), N(cross(b,c));
  u32 k = 0;
  for (u32 i=1; i<3; ++i)
    k = fabsf(N[i]) > fabsf(N[k]) ? i : k;
  const u32 u = (k+1)%3, v = (k+2)%3;
  const float
    denom = (b[u]*c[v] - b[v]*c[u]),
          krec = N[k];
  const float
    nu = N[u] / krec, nv = N[v] / krec, nd = dot(N,A) / krec,
       bnu =  b[u] / denom, bnv = -b[v] / denom,
       cnu =  c[v] / denom, cnv = -c[u] / denom;
  w.k          = k;
  w.n_u        = float(nu);
  w.n_v        = float(nv);
  w.n_d        = float(nd);
  w.vert_ku    = float(A[u]);
  w.vert_kv    = float(A[v]);
  w.b_nu       = float(bnu);
  w.b_nv       = float(bnv);
  w.c_nu       = float(cnu);
  w.c_nv       = float(cnv);
  w.id         = id;
  w.matid      = matid;

  return (krec == 0.) | (denom == 0.);
}

intersector *create(const triangle *tri, int n) {
  compiler_t c;
  if(c.injection(tri, n) < 0) return NULL;
  c.compile();
  auto tree = new intersector;
  tree->root = c.root;
  tree->acc.resize(n);
  loopi(n) compute_tri_acc(tri[i], tree->acc[i], i, 0);
  return tree;
}

void destroy(intersector *bvhtree) {
  if(bvhtree == NULL) return;
  if(bvhtree->root != NULL) delete[] bvhtree->root;
  delete bvhtree;
}

struct trace_bvh_stack_t {
  enum { max_lvl = 64 };
  struct trace_t {
    const node_t *node;
  };

  trace_t traces[max_lvl];
  s32 idx;
  trace_bvh_stack_t() : idx(0) {}
  void reset() { idx = 0; }
  void wipe() { memset(traces, 0, sizeof(traces)); }
  u32 pop() { return --idx >= 0; }
  const node_t * __restrict node() const { return traces[idx].node; }
};

extern u32 bvhlib_compile(const triangle * __restrict const, const u32 size_t, intersector &);
extern void bvhlib_free(intersector *bvhtree);

static const u32 wald_modulo[] = {1, 2, 0, 1};

INLINE void intersect_ray_tri(const wald_tri_t &tri, const ray &r, hit &hit) {
  const u32 k = tri.k, ku = wald_modulo[k], kv = wald_modulo[k+1];
  const float dir_ku = r.dir[ku], dir_kv = r.dir[kv];
  const float pos_ku = r.org[ku], pos_kv = r.org[kv];
  const float t = (tri.n_d - r.org[k] - tri.n_u*pos_ku - tri.n_v*pos_kv) /
                  (r.dir[k] + tri.n_u*dir_ku + tri.n_v*dir_kv);
  if (!((hit.t > t) & (t >= 0.f))) return;
  const float hu = pos_ku + t*dir_ku - tri.vert_ku;
  const float hv = pos_kv + t*dir_kv - tri.vert_kv;
  const float beta = hv*tri.b_nu + hu*tri.b_nv;
  const float gamma = hu*tri.c_nu + hv*tri.c_nv;
  if ((beta < 0.f) | (gamma < 0.f) | ((beta + gamma) > 1.f)) return;
  hit.t = t;
  hit.u = beta;
  hit.v = gamma;
  hit.id = tri.id;
}

INLINE u32 intersect_ray_box(const aabb_t &box, const ray &r, float t)
{
  float l1 = (box.pmin.x - r.org.x) * r.rdir.x;
  float l2 = (box.pmax.x - r.org.x) * r.rdir.x;
  float t_near = min(l1,l2);
  float t_far  = max(l1,l2);
  l1     = (box.pmin.y - r.org.y) * r.rdir.y;
  l2     = (box.pmax.y - r.org.y) * r.rdir.y;
  t_near = max(min(l1,l2), t_near);
  t_far  = min(max(l1,l2), t_far);
  l1     = (box.pmin.z - r.org.z) * r.rdir.z;
  l2     = (box.pmax.z - r.org.z) * r.rdir.z;
  t_near = max(min(l1,l2), t_near);
  t_far  = min(max(l1,l2), t_far);
  return ((t_far >= t_near) & (t_far >= 0.f) & (t_near < t));
}

void trace(const intersector &bvhtree, const ray &r, hit &hit)
{
  const s32 signs_all[3] = {(r.dir.x>=0.f)&1, (r.dir.y>=0.f)&1, (r.dir.z>=0.f)&1};
  trace_bvh_stack_t trace_stack;

  trace_stack.reset();
  trace_stack.traces[0].node = bvhtree.root;
  ++trace_stack.idx;

pop_label:
  while(trace_stack.pop()) {
    const node_t *node = trace_stack.traces[trace_stack.idx].node;
    while(!node->is_leaf()) {
      if(!intersect_ray_box(node->aabb, r, hit.t))
        goto pop_label;
      const int32_t next = signs_all[node->d];
      const int32_t first = ~next & 1;
      const node_t *left = &bvhtree.root[node->offset_flag + next];
      const int32_t idx = trace_stack.idx++;
      trace_stack.traces[idx].node = left;
      node = &bvhtree.root[node->offset_flag + first];
    }
    intersect_ray_tri(bvhtree.acc[node->tri_id], r, hit);
  }
}
} // namespace bvh
} // namespace cube

