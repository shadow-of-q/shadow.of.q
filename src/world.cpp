#include "cube.hpp"
#include <SDL/SDL.h>
#include <zlib.h>

#if defined(EMSCRIPTEN)
#error "check sse"
#else
#include <xmmintrin.h>
#endif // EMSCRIPTEN

#if 1
namespace cube {
#define op operator
  static const __m128 _mm_lookupmask_ps[16] = {
    _mm_castsi128_ps(_mm_set_epi32( 0, 0, 0, 0)),
    _mm_castsi128_ps(_mm_set_epi32( 0, 0, 0,-1)),
    _mm_castsi128_ps(_mm_set_epi32( 0, 0,-1, 0)),
    _mm_castsi128_ps(_mm_set_epi32( 0, 0,-1,-1)),
    _mm_castsi128_ps(_mm_set_epi32( 0,-1, 0, 0)),
    _mm_castsi128_ps(_mm_set_epi32( 0,-1, 0,-1)),
    _mm_castsi128_ps(_mm_set_epi32( 0,-1,-1, 0)),
    _mm_castsi128_ps(_mm_set_epi32( 0,-1,-1,-1)),
    _mm_castsi128_ps(_mm_set_epi32(-1, 0, 0, 0)),
    _mm_castsi128_ps(_mm_set_epi32(-1, 0, 0,-1)),
    _mm_castsi128_ps(_mm_set_epi32(-1, 0,-1, 0)),
    _mm_castsi128_ps(_mm_set_epi32(-1, 0,-1,-1)),
    _mm_castsi128_ps(_mm_set_epi32(-1,-1, 0, 0)),
    _mm_castsi128_ps(_mm_set_epi32(-1,-1, 0,-1)),
    _mm_castsi128_ps(_mm_set_epi32(-1,-1,-1, 0)),
    _mm_castsi128_ps(_mm_set_epi32(-1,-1,-1,-1))
  };
  struct sseb {
    union { __m128 m128; s32 v[4]; };
    INLINE sseb(void) {}
    INLINE sseb(const sseb& other) { m128 = other.m128; }
    INLINE sseb& op= (const sseb& other) { m128 = other.m128; return *this; }
    INLINE sseb(__m128  x) : m128(x) {}
    INLINE sseb(__m128i x) : m128(_mm_castsi128_ps(x)) {}
    INLINE sseb(__m128d x) : m128(_mm_castpd_ps(x)) {}
    INLINE sseb(bool x)
      : m128(x ? _mm_castsi128_ps(_mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128())) : _mm_setzero_ps()) {}
    INLINE sseb(bool x0, bool x1, bool x2, bool x3)
      : m128(_mm_lookupmask_ps[(size_t(x3) << 3) | (size_t(x2) << 2) | (size_t(x1) << 1) | size_t(x0)]) {}
    INLINE op const __m128&(void) const { return m128; }
    INLINE op const __m128i(void) const { return _mm_castps_si128(m128); }
    INLINE op const __m128d(void) const { return _mm_castps_pd(m128); }
    INLINE bool op[] (const size_t index) const { assert(index < 4); return (_mm_movemask_ps(m128) >> index) & 1; }
    INLINE s32& op[] (const size_t index) { assert(index < 4); return this->v[index]; }
  };

  INLINE const sseb op !(const sseb& a) { return _mm_xor_ps(a, sseb(~0x0)); }
  INLINE const sseb op &(const sseb& a, const sseb& b) { return _mm_and_ps(a, b); }
  INLINE const sseb op |(const sseb& a, const sseb& b) { return _mm_or_ps (a, b); }
  INLINE const sseb op ^(const sseb& a, const sseb& b) { return _mm_xor_ps(a, b); }
  INLINE sseb op &=(sseb& a, const sseb& b) { return a = a & b; }
  INLINE sseb op |=(sseb& a, const sseb& b) { return a = a | b; }
  INLINE sseb op ^=(sseb& a, const sseb& b) { return a = a ^ b; }
  INLINE const sseb op !=(const sseb& a, const sseb& b) { return _mm_xor_ps(a, b); }
  INLINE const sseb op ==(const sseb& a, const sseb& b) { return _mm_cmpeq_epi32(a, b); }
  INLINE bool reduce_and(const sseb& a) { return _mm_movemask_ps(a) == 0xf; }
  INLINE bool reduce_or(const sseb& a) { return _mm_movemask_ps(a) != 0x0; }
  INLINE bool all(const sseb& b) { return _mm_movemask_ps(b) == 0xf; }
  INLINE bool any(const sseb& b) { return _mm_movemask_ps(b) != 0x0; }
  INLINE bool none(const sseb& b) { return _mm_movemask_ps(b) == 0x0; }
  INLINE size_t movemask(const sseb& a) { return _mm_movemask_ps(a); }
  INLINE sseb unmovemask(size_t m)      { return _mm_lookupmask_ps[m]; }
  template<size_t index_0, size_t index_1, size_t index_2, size_t index_3>
  INLINE sseb shuffle(const sseb& a) {
    return sseb(_mm_shuffle_epi32(_mm_castps_si128(a.m128), _MM_SHUFFLE(index_3, index_2, index_1, index_0)));
  }

  struct ssei {
    union { __m128i m128; s32 v[4]; };
    INLINE ssei() {}
    INLINE ssei(const ssei& other) { m128 = other.m128; }
    INLINE ssei& operator=(const ssei& other) { m128 = other.m128; return *this; }
    INLINE ssei(const __m128i a) : m128(a) {}
    INLINE explicit ssei(const __m128 a) : m128(_mm_cvtps_epi32(a)) {}
    INLINE explicit ssei(const s32* const a) : m128(_mm_loadu_si128((__m128i*)a)) {}
    INLINE ssei(const s32 a) : m128(_mm_set1_epi32(a)) {}
    INLINE ssei(const s32 a, const s32 b, const s32 c, const s32 d) : m128(_mm_set_epi32(d, c, b, a)) {}
    INLINE operator const __m128i&(void) const { return m128; }
    INLINE operator __m128i&(void) { return m128; }
    INLINE const s32& operator [](const size_t index) const { assert(index < 4); return v[index]; }
    INLINE s32& operator [](const size_t index) { assert(index < 4); return v[index]; }
  };

  INLINE ssei op +(const ssei& a) { return a; }
  INLINE ssei op -(const ssei& a) { return _mm_sub_epi32(_mm_setzero_si128(), a.m128); }
  INLINE ssei truncate(const __m128 a)  { return _mm_cvttps_epi32(a); }
  INLINE const ssei op+ (const ssei& a, const ssei& b) { return _mm_add_epi32(a.m128, b.m128); }
  INLINE const ssei op- (const ssei& a, const ssei& b) { return _mm_sub_epi32(a.m128, b.m128); }
  INLINE const ssei op& (const ssei& a, const ssei& b) { return _mm_and_si128(a.m128, b.m128); }
  INLINE const ssei op| (const ssei& a, const ssei& b) { return _mm_or_si128(a.m128, b.m128); }
  INLINE const ssei op^ (const ssei& a, const ssei& b) { return _mm_xor_si128(a.m128, b.m128); }
  INLINE const ssei op<< (const ssei& a, const s32 bits) { return _mm_slli_epi32(a.m128, bits); }
  INLINE const ssei op>> (const ssei& a, const s32 bits) { return _mm_srai_epi32(a.m128, bits); }
  INLINE const ssei op& (const ssei& a, const s32 b) { return a & ssei(b); }
  INLINE const ssei op| (const ssei& a, const s32 b) { return a | ssei(b); }
  INLINE const ssei op^ (const ssei& a, const s32 b) { return a ^ ssei(b); }
  INLINE ssei& op+= (ssei& a, const ssei& b) { return a = a  + b; }
  INLINE ssei& op-= (ssei& a, const ssei& b) { return a = a  - b; }
  INLINE ssei& op&= (ssei& a, const ssei& b) { return a = a  & b; }
  INLINE ssei& op&= (ssei& a, const s32 b) { return a = a  & b; }
  INLINE ssei& op<<= (ssei& a, const s32 b) { return a = a << b; }
  INLINE ssei& op>>= (ssei& a, const s32 b) { return a = a >> b; }
  INLINE const ssei sra(const ssei& a, const s32 b) { return _mm_srai_epi32(a.m128, b); }
  INLINE const ssei srl(const ssei& a, const s32 b) { return _mm_srli_epi32(a.m128, b); }
  INLINE const ssei rotl(const ssei& a, const s32 b) { return _mm_or_si128(_mm_srli_epi32(a.m128, 32 - b), _mm_slli_epi32(a.m128, b)); }
  INLINE const ssei rotr(const ssei& a, const s32 b) { return _mm_or_si128(_mm_slli_epi32(a.m128, 32 - b), _mm_srli_epi32(a.m128, b)); }
  INLINE const sseb op==(const ssei& a, const ssei& b) { return _mm_cmpeq_epi32 (a.m128, b.m128); }
  INLINE const sseb op< (const ssei& a, const ssei& b) { return _mm_cmplt_epi32 (a.m128, b.m128); }
  INLINE const sseb op> (const ssei& a, const ssei& b) { return _mm_cmpgt_epi32 (a.m128, b.m128); }
  INLINE const sseb op!= (const ssei& a, const ssei& b) { return !(a == b); }
  INLINE const sseb op>= (const ssei& a, const ssei& b) { return !(a <  b); }
  INLINE const sseb op<= (const ssei& a, const ssei& b) { return !(a >  b); }
  INLINE const sseb cmpa(const ssei& a, const ssei& b) { return _mm_cmpgt_epi32(_mm_xor_si128(a.m128, _mm_set1_epi32(0x80000000)), _mm_xor_si128(b.m128, _mm_set1_epi32(0x80000000))); }
  INLINE const ssei select(const sseb& mask, const ssei& a, const ssei& b) { 
    return _mm_castps_si128(_mm_or_ps(_mm_and_ps(mask, _mm_castsi128_ps(a)), _mm_andnot_ps(mask, _mm_castsi128_ps(b)))); 
  }
  template<size_t index_0, size_t index_1, size_t index_2, size_t index_3>
  INLINE const ssei shuffle(const ssei& a) {
    return _mm_shuffle_epi32(a, _MM_SHUFFLE(index_3, index_2, index_1, index_0));
  }
  template<size_t index> INLINE const ssei expand(const ssei& b) { return shuffle<index, index, index, index>(b); }
  template<size_t src> INLINE int extract(const ssei& b) { return b[src]; }
  INLINE ssei unpacklo(const ssei& a, const ssei& b) { return _mm_castps_si128(_mm_unpacklo_ps(_mm_castsi128_ps(a.m128), _mm_castsi128_ps(b.m128))); }
  INLINE ssei unpackhi(const ssei& a, const ssei& b) { return _mm_castps_si128(_mm_unpackhi_ps(_mm_castsi128_ps(a.m128), _mm_castsi128_ps(b.m128))); }

  struct ssef {
    union { __m128 m128; float v[4]; };
    INLINE ssef(void) {}
    INLINE ssef(const ssef& other) { m128 = other.m128; }
    INLINE ssef(__m128 a) : m128(a) {}
    INLINE explicit ssef(__m128i a) : m128(_mm_cvtepi32_ps(a)) {}
    INLINE explicit ssef(const float *a) : m128(_mm_loadu_ps(a)) {}
    INLINE ssef(float a) : m128(_mm_set1_ps(a)) {}
    INLINE ssef(float a, float b, float c, float d) : m128(_mm_set_ps(d, c, b, a)) {}
    INLINE ssef& op=(const ssef& other) { m128 = other.m128; return *this; }
    INLINE op const __m128&(void) const { return m128; }
    INLINE op __m128&(void) { return m128; }
    INLINE const float& op[] (size_t index) const { assert(index < 4); return this->v[index]; }
    INLINE float& op[] (size_t index) { assert(index < 4); return this->v[index]; }
    static INLINE ssef load(const float *a) { return _mm_load_ps(a); }
    static INLINE ssef uload(const float *a) { return _mm_loadu_ps(a); }
    static INLINE void store(const ssef &x, float *a) { return _mm_store_ps(a, x.m128); }
    static INLINE void ustore(const ssef &x, float *a) { return _mm_storeu_ps(a, x.m128); }
  };

  INLINE const ssef op+ (const ssef& a) { return a; }
  INLINE const ssef op- (const ssef& a) {
    const __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
    return _mm_xor_ps(a.m128, mask);
  }
  INLINE const ssef abs  (const ssef& a) {
    const __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));
    return _mm_and_ps(a.m128, mask);
  }
  INLINE const ssef rcp  (const ssef& a) { return _mm_div_ps(ssef(1.f), a.m128); }
  INLINE const ssef sqrt (const ssef& a) { return _mm_sqrt_ps(a.m128); }
  INLINE const ssef sqr  (const ssef& a) { return _mm_mul_ps(a,a); }
  INLINE const ssef rsqrt(const ssef& a) {
    const ssef r = _mm_rsqrt_ps(a.m128);
    return _mm_add_ps(_mm_mul_ps(_mm_set_ps(1.5f, 1.5f, 1.5f, 1.5f), r),
                        _mm_mul_ps(_mm_mul_ps(
                          _mm_mul_ps(a, _mm_set_ps(-0.5f, -0.5f, -0.5f, -0.5f)), r),
                            _mm_mul_ps(r, r)));
  }

  INLINE ssef op+ (const ssef& a, const ssef& b) { return _mm_add_ps(a.m128, b.m128); }
  INLINE ssef op- (const ssef& a, const ssef& b) { return _mm_sub_ps(a.m128, b.m128); }
  INLINE ssef op* (const ssef& a, const ssef& b) { return _mm_mul_ps(a.m128, b.m128); }
  INLINE ssef op/ (const ssef& a, const ssef& b) { return _mm_div_ps(a.m128, b.m128); }
  INLINE ssef op+ (const ssef& a, float b) { return a + ssef(b); }
  INLINE ssef op- (const ssef& a, float b) { return a - ssef(b); }
  INLINE ssef op* (const ssef& a, float b) { return a * ssef(b); }
  INLINE ssef op/ (const ssef& a, float b) { return a / ssef(b); }
  INLINE ssef op+ (float a, const ssef& b) { return ssef(a) + b; }
  INLINE ssef op- (float a, const ssef& b) { return ssef(a) - b; }
  INLINE ssef op* (float a, const ssef& b) { return ssef(a) * b; }
  INLINE ssef op/ (float a, const ssef& b) { return ssef(a) / b; }
  INLINE ssef& op+= (ssef& a, const ssef& b) { return a = a + b; }
  INLINE ssef& op-= (ssef& a, const ssef& b) { return a = a - b; }
  INLINE ssef& op*= (ssef& a, const ssef& b) { return a = a * b; }
  INLINE ssef& op/= (ssef& a, const ssef& b) { return a = a / b; }
  INLINE ssef& op+= (ssef& a, float b) { return a = a + b; }
  INLINE ssef& op-= (ssef& a, float b) { return a = a - b; }
  INLINE ssef& op*= (ssef& a, float b) { return a = a * b; }
  INLINE ssef& op/= (ssef& a, float b) { return a = a / b; }
  INLINE ssef max(const ssef& a, const ssef& b) { return _mm_max_ps(a.m128,b.m128); }
  INLINE ssef min(const ssef& a, const ssef& b) { return _mm_min_ps(a.m128,b.m128); }
  INLINE ssef max(const ssef& a, float b) { return _mm_max_ps(a.m128,ssef(b)); }
  INLINE ssef min(const ssef& a, float b) { return _mm_min_ps(a.m128,ssef(b)); }
  INLINE ssef max(float a, const ssef& b) { return _mm_max_ps(ssef(a),b.m128); }
  INLINE ssef min(float a, const ssef& b) { return _mm_min_ps(ssef(a),b.m128); }
  INLINE ssef mulss(const ssef &a, const ssef &b) { return _mm_mul_ss(a,b); }
  INLINE ssef divss(const ssef &a, const ssef &b) { return _mm_div_ss(a,b); }
  INLINE ssef subss(const ssef &a, const ssef &b) { return _mm_sub_ss(a,b); }
  INLINE ssef addss(const ssef &a, const ssef &b) { return _mm_add_ss(a,b); }
  INLINE ssef mulss(const ssef &a, float b) { return _mm_mul_ss(a,ssef(b)); }
  INLINE ssef divss(const ssef &a, float b) { return _mm_div_ss(a,ssef(b)); }
  INLINE ssef subss(const ssef &a, float b) { return _mm_sub_ss(a,ssef(b)); }
  INLINE ssef addss(const ssef &a, float b) { return _mm_add_ss(a,ssef(b)); }
  INLINE ssef mulss(float a, const ssef &b) { return _mm_mul_ss(ssef(a),b); }
  INLINE ssef divss(float a, const ssef &b) { return _mm_div_ss(ssef(a),b); }
  INLINE ssef subss(float a, const ssef &b) { return _mm_sub_ss(ssef(a),b); }
  INLINE ssef addss(float a, const ssef &b) { return _mm_add_ss(ssef(a),b); }
  INLINE ssef op^ (const ssef& a, const ssei& b) { return _mm_castsi128_ps(_mm_xor_si128(_mm_castps_si128(a.m128),b.m128)); }
  INLINE ssef op^ (const ssef& a, const ssef& b) { return _mm_xor_ps(a.m128,b.m128); }
  INLINE ssef op& (const ssef& a, const ssef& b) { return _mm_and_ps(a.m128,b.m128); }
  INLINE ssef op& (const ssef& a, const ssei& b) { return _mm_and_ps(a.m128,_mm_castsi128_ps(b.m128)); }
  INLINE ssef op| (const ssef& a, const ssef& b) { return _mm_or_ps(a.m128,b.m128); }
  INLINE ssef op| (const ssef& a, const ssei& b) { return _mm_or_ps(a.m128,_mm_castsi128_ps(b.m128)); }
  INLINE sseb op== (const ssef& a, const ssef& b) { return _mm_cmpeq_ps (a.m128, b.m128); }
  INLINE sseb op!= (const ssef& a, const ssef& b) { return _mm_cmpneq_ps(a.m128, b.m128); }
  INLINE sseb op<  (const ssef& a, const ssef& b) { return _mm_cmplt_ps (a.m128, b.m128); }
  INLINE sseb op<= (const ssef& a, const ssef& b) { return _mm_cmple_ps (a.m128, b.m128); }
  INLINE sseb op>  (const ssef& a, const ssef& b) { return _mm_cmpnle_ps(a.m128, b.m128); }
  INLINE sseb op>= (const ssef& a, const ssef& b) { return _mm_cmpnlt_ps(a.m128, b.m128); }
  INLINE sseb op== (const ssef& a, float b) {return _mm_cmpeq_ps (a.m128, ssef(b));}
  INLINE sseb op!= (const ssef& a, float b) {return _mm_cmpneq_ps(a.m128, ssef(b));}
  INLINE sseb op<  (const ssef& a, float b) {return _mm_cmplt_ps (a.m128, ssef(b));}
  INLINE sseb op<= (const ssef& a, float b) {return _mm_cmple_ps (a.m128, ssef(b));}
  INLINE sseb op>  (const ssef& a, float b) {return _mm_cmpnle_ps(a.m128, ssef(b));}
  INLINE sseb op>= (const ssef& a, float b) {return _mm_cmpnlt_ps(a.m128, ssef(b));}
  INLINE sseb op== (float a, const ssef& b) {return _mm_cmpeq_ps (ssef(a), b.m128);}
  INLINE sseb op!= (float a, const ssef& b) {return _mm_cmpneq_ps(ssef(a), b.m128);}
  INLINE sseb op<  (float a, const ssef& b) {return _mm_cmplt_ps (ssef(a), b.m128);}
  INLINE sseb op<= (float a, const ssef& b) {return _mm_cmple_ps (ssef(a), b.m128);}
  INLINE sseb op>  (float a, const ssef& b) {return _mm_cmpnle_ps(ssef(a), b.m128);}
  INLINE sseb op>= (float a, const ssef& b) {return _mm_cmpnlt_ps(ssef(a), b.m128);}
  INLINE ssef select(const sseb& mask, const ssef& a, const ssef& b) { return _mm_or_ps(_mm_and_ps(mask, a), _mm_andnot_ps(mask, b)); }
  template<size_t i0, size_t i1, size_t i2, size_t i3>
  INLINE const ssef shuffle(const ssef& b) {
    return _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(b), _MM_SHUFFLE(i3, i2, i1, i0)));
  }
  template<size_t i> INLINE
  ssef expand(const ssef& b) { return shuffle<i, i, i, i>(b); }
  template<size_t i0, size_t i1, size_t i2, size_t i3>
  INLINE ssef shuffle(const ssef& a, const ssef& b) {
    return _mm_shuffle_ps(a, b, _MM_SHUFFLE(i3, i2, i1, i0));
  }
  INLINE ssef reduce_min(const ssef& v) { ssef h = min(shuffle<1,0,3,2>(v),v); return min(shuffle<2,3,0,1>(h),h); }
  INLINE ssef reduce_max(const ssef& v) { ssef h = max(shuffle<1,0,3,2>(v),v); return max(shuffle<2,3,0,1>(h),h); }
  INLINE ssef reduce_add(const ssef& v) { ssef h = shuffle<1,0,3,2>(v) + v; return shuffle<2,3,0,1>(h) + h; }
#undef operator
} // namespace cube
#endif

namespace cube {
namespace world {

using namespace game;
const int ssize = 1024;

int findentity(int type, int index) {
  for (int i = index; i<ents.length(); i++)
    if (ents[i].type==type) return i;
  loopj(index) if (ents[j].type==type) return j;
  return -1;
}

// map file format header
static struct header {
  char head[4]; // "CUBE"
  int version; // any >8bit quantity is a little indian
  int headersize; // sizeof(header)
  int sfactor; // in bits
  int numents;
  char maptitle[128];
  uchar texlists[3][256];
  int waterlevel;
  int reserved[15];
} hdr;

int waterlevel(void) { return hdr.waterlevel; }
char *maptitle(void) { return hdr.maptitle; }

void trigger(int tag, int type, bool savegame) {
  if (!tag) return;
  // settag(tag, type);
  if (!savegame && type!=3)
    sound::play(sound::RUMBLE);
  sprintf_sd(aliasname)("level_trigger_%d", tag);
  if (cmd::identexists(aliasname))
    cmd::execute(aliasname);
  if (type==2)
   endsp(false);
}

int closestent(void) {
  if (edit::noteditmode()) return -1;
  int best = 0;
  float bdist = 99999.f;
  loopv(ents) {
    entity &e = ents[i];
    if (e.type==NOTUSED) continue;
    const vec3f v(float(e.x), float(e.y), float(e.z));
    const float dist = distance(player1->o, v);
    if (dist<bdist) {
      best = i;
      bdist = dist;
    }
  }
  return bdist==99999.f ? -1 : best;
}

void entproperty(int prop, int amount) {
  const int e = closestent();
  if (e < 0) return;
  switch (prop) {
    case 0: ents[e].attr1 += amount; break;
    case 1: ents[e].attr2 += amount; break;
    case 2: ents[e].attr3 += amount; break;
    case 3: ents[e].attr4 += amount; break;
  }
}

static void delent(void) {
  const int e = closestent();
  if (e < 0) {
    console::out("no more entities");
    return;
  }
  const int t = ents[e].type;
  console::out("%s entity deleted", entnames(t));
  ents[e].type = NOTUSED;
  client::addmsg(1, 10, SV_EDITENT, e, NOTUSED, 0, 0, 0, 0, 0, 0, 0);
}
COMMAND(delent, ARG_NONE);

static int findtype(const char *what) {
  loopi(MAXENTTYPES) if (strcmp(what,entnames(i))==0) return i;
  console::out("unknown entity type \"%s\"", what);
  return NOTUSED;
}

entity *newentity(int x, int y, int z, char *what, int v1, int v2, int v3, int v4) {
  const int type = findtype(what);
  persistent_entity e = {short(x), short(y), short(z), short(v1),
    uchar(type), uchar(v2), uchar(v3), uchar(v4)};
  switch (type) {
    case LIGHT:
      if (v1>32) v1 = 32;
      if (!v1) e.attr1 = 16;
      if (!v2 && !v3 && !v4) e.attr2 = 255;
    break;
    case MAPMODEL:
      e.attr4 = e.attr3;
      e.attr3 = e.attr2;
    case MONSTER:
    case TELEDEST:
      e.attr2 = (uchar)e.attr1;
    case PLAYERSTART:
      e.attr1 = (int)player1->yaw;
    break;
  }
  client::addmsg(1, 10, SV_EDITENT, ents.length(), type, e.x, e.y, e.z, e.attr1, e.attr2, e.attr3, e.attr4);
  ents.add(*((entity *)&e)); // unsafe!
  return &ents.last();
}

static void clearents(char *name) {
  int type = findtype(name);
  if (edit::noteditmode() || client::multiplayer())
    return;
  loopv(ents) {
    entity &e = ents[i];
    if (e.type==type) e.type = NOTUSED;
  }
}
COMMAND(clearents, ARG_1STR);

int isoccluded(float vx, float vy, float cx, float cy, float csize) {return 0;}

static string cgzname, bakname, pcfname, mcfname;

struct deletegrid {
  template <typename G> void operator()(G &g, vec3i) const {
    if (uintptr_t(&g)!=uintptr_t(&root)) delete &g;
  }
};
static void empty(void) {
  forallgrids(deletegrid());
  MEMZERO(root.elem);
  root.dirty = 1;
}

void setnames(const char *name) {
  string pakname, mapname;
  const char *slash = strpbrk(name, "/\\");
  if (slash) {
    strn0cpy(pakname, name, slash-name+1);
    strcpy_s(mapname, slash+1);
  } else {
    strcpy_s(pakname, "base");
    strcpy_s(mapname, name);
  }
  sprintf_s(cgzname)("packages/%s/%s.cgz", pakname, mapname);
  sprintf_s(bakname)("packages/%s/%s_%d.BAK", pakname, mapname, lastmillis());
  sprintf_s(pcfname)("packages/%s/package.cfg", pakname);
  sprintf_s(mcfname)("packages/%s/%s.cfg", pakname, mapname);
  path(cgzname);
  path(bakname);
}

void backup(char *name, char *backupname) {
  remove(backupname);
  rename(name, backupname);
}

void save(const char *mname) {
  if (!*mname) mname = getclientmap();
  setnames(mname);
  backup(cgzname, bakname);
  gzFile f = gzopen(cgzname, "wb9");
  if (!f) {
    console::out("could not write map to %s", cgzname);
    return;
  }
  hdr.version = MAPVERSION;
  hdr.numents = 0;
  loopv(ents) if (ents[i].type!=NOTUSED) hdr.numents++;
  header tmp = hdr;
  endianswap(&tmp.version, sizeof(int), 4);
  endianswap(&tmp.waterlevel, sizeof(int), 16);
  gzwrite(f, &tmp, sizeof(header));
  loopv(ents) {
    if (ents[i].type!=NOTUSED) {
      entity tmp = ents[i];
      endianswap(&tmp, sizeof(short), 4);
      gzwrite(f, &tmp, sizeof(persistent_entity));
    }
  }
  s32 n=0;
  forallcubes([&](const brickcube&, const vec3i&){++n;});
  gzwrite(f, &n, sizeof(n));
  vec3i pos(zero);
  forallcubes([&](const brickcube &c, const vec3i &xyz) {
    vec3i delta = xyz-pos;
    gzwrite(f, (void*) &delta, sizeof(vec3i));
    pos = xyz;
  });
  vec3i d(zero);
  forallcubes([&](const brickcube &c, const vec3i &xyz) {
    vec3i delta = vec3i(c.p) - d;
    gzwrite(f, (void*) &delta, sizeof(vec3i));
    d = vec3i(c.p);
  });
  s16 mat = 0;
  forallcubes([&](const brickcube &c, const vec3i &xyz) {
    s16 delta = c.mat - mat;
    gzwrite(f, (void*) &delta, sizeof(s16));
    mat = c.mat;
  });
  loopi(6) {
    s16 tex = 0;
    forallcubes([&](const brickcube &c, const vec3i &xyz) {
      s16 delta = c.tex[i] - tex;
      gzwrite(f, (void*) &delta, sizeof(s16));
      tex = c.tex[i];
    });
  }
  gzclose(f);
  console::out("wrote map file %s (%i cubes in total)", cgzname, n);
}

void load(const char *mname) {
  demo::stopifrecording();
  edit::pruneundos();
  setnames(mname);
  empty();
  gzFile f = gzopen(cgzname, "rb9");
  if (!f) {
    console::out("could not read map %s", cgzname);
    return;
  }
  gzread(f, &hdr, sizeof(header)-sizeof(int)*16);
  endianswap(&hdr.version, sizeof(int), 4);
  if (strncmp(hdr.head, "CUBE", 4)!=0)
    fatal("while reading map: header malformatted");
  if (hdr.version>MAPVERSION)
    fatal("this map requires a newer version of cube");
  if (hdr.sfactor<SMALLEST_FACTOR || hdr.sfactor>LARGEST_FACTOR)
    fatal("illegal map size");
  if (hdr.version>=4) {
    gzread(f, &hdr.waterlevel, sizeof(int)*16);
    endianswap(&hdr.waterlevel, sizeof(int), 16);
  } else
    hdr.waterlevel = -100000;
  ents.setsize(0);
  loopi(hdr.numents) {
    entity &e = ents.add();
    gzread(f, &e, sizeof(persistent_entity));
    endianswap(&e, sizeof(short), 4);
    e.spawned = false;
    if (e.type==LIGHT) {
      if (!e.attr2) e.attr2 = 255; // needed for MAPVERSION<=2
      if (e.attr1>32) e.attr1 = 32; // 12_03 and below
    }
  }
  s32 n;
  gzread(f, &n, sizeof(n));
  vector<vec3i> xyz(n);
  vector<brickcube> cubes(n);
  vec3i p(zero); // get all world cube positions
  loopi(n) {
    gzread(f, (void*) &xyz[i], sizeof(vec3i));
    xyz[i] += p;
    p = xyz[i];
  }
  vec3i d(zero); // get displacements
  loopi(n) {
    vec3i r;
    gzread(f, (void*) &r, sizeof(vec3i));
    cubes[i].p = vec3<s8>(r+d);
    d = r+d;
  }
  s16 m=0; // get all material IDs
  loopi(n) {
    s16 r;
    gzread(f, (void*) &r, sizeof(s16));
    cubes[i].mat = s8(r+m);
    m += r;
  }
  loopj(6) { // get all textures
    s16 tex = 0;
    loopi(n) {
      gzread(f, (void*) &cubes[i].tex[j], sizeof(s16));
      cubes[i].tex[j] += tex;
      tex = cubes[i].tex[j];
    }
  }
  loopv(xyz) { world::setcube(xyz[i], cubes[i]); }
  gzclose(f);

  console::out("read map %s (%d milliseconds)", cgzname, SDL_GetTicks()-lastmillis());
  console::out("%s", hdr.maptitle);
  startmap(mname);
  loopl(256) {
    sprintf_sd(aliasname)("level_trigger_%d", l);
    if (cmd::identexists(aliasname))
      cmd::alias(aliasname, "");
  }
  cmd::execfile("data/default_map_settings.cfg");
  //cmd::execfile(pcfname);
  //cmd::execfile(mcfname);
}
COMMANDN(savemap, save, ARG_1STR);

// our world and its total dimension
lvl3grid root;

brickcube getcube(const vec3i &xyz) {return world::root.get(xyz);}
void setcube(const vec3i &xyz, const brickcube &cube) {
  root.set(xyz, cube);
  const vec3i m = xyz % brickisize;
  loopi(3) // reset neighbors to force rebuild of neighbor bricks
    if (m[i] == brickisize.x-1)
      root.set(xyz+iaxis[i], root.get(xyz+iaxis[i]));
    else if (m[i] == 0)
      root.set(xyz-iaxis[i], root.get(xyz-iaxis[i]));
}

static void writebmp(const int *data, int width, int height, const char *filename) {
  int x, y;
  FILE *fp = fopen(filename, "wb");
  assert(fp);
  struct bmphdr {
    int filesize;
    short as0, as1;
    int bmpoffset;
    int headerbytes;
    int width;
    int height;
    short nplanes;
    short bpp;
    int compression;
    int sizeraw;
    int hres;
    int vres;
    int npalcolors;
    int nimportant;
  };

  const char magic[2] = { 'B', 'M' };
  char *raw = (char *) malloc(width * height * sizeof(int)); // at most
  assert(raw);
  char *p = raw;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      int c = *data++;
      *p++ = ((c >> 16) & 0xff);
      *p++ = ((c >> 8) & 0xff);
      *p++ = ((c >> 0) & 0xff);
    }
    while (x & 3) {
      *p++ = 0;
      x++;
    } // pad to dword
  }
  int sizeraw = p - raw;
  int scanline = (width * 3 + 3) & ~3;
  assert(sizeraw == scanline * height);

  struct bmphdr hdr;

  hdr.filesize = scanline * height + sizeof(hdr) + 2;
  hdr.as0 = 0;
  hdr.as1 = 0;
  hdr.bmpoffset = sizeof(hdr) + 2;
  hdr.headerbytes = 40;
  hdr.width = width;
  hdr.height = height;
  hdr.nplanes = 1;
  hdr.bpp = 24;
  hdr.compression = 0;
  hdr.sizeraw = sizeraw;
  hdr.hres = 0;
  hdr.vres = 0;
  hdr.npalcolors = 0;
  hdr.nimportant = 0;
  fwrite(&magic[0], 1, 2, fp);
  fwrite(&hdr, 1, sizeof(hdr), fp);
  fwrite(raw, 1, hdr.sizeraw, fp);
  fclose(fp);
  free(raw);
}

VAR(raycast, 0, 0, 1);

template <typename T> struct gridpolicy {
  static INLINE vec3f cellorg(vec3f boxorg, vec3i xyz, vec3f cellsize) {
    return boxorg+vec3f(xyz)*cellsize;
  }
};
template <> struct gridpolicy<lvl1grid> {
  static INLINE vec3f cellorg(vec3f boxorg, vec3i xyz, vec3f cellsize) {
    return vec3f(zero);
  }
};
INLINE isecres intersect(const brickcube &cube, const vec3f&, const ray&, float t) {
  return isecres(cube.mat==FULL, t);
}

template <typename G>
INLINE isecres intersect(const G *grid, const vec3f &boxorg, const ray &ray, float t) {
  if (grid == NULL) return isecres(false);
  const vec3b signs = ray.dir > vec3f(zero);
  const vec3f cellsize = grid->subcuben();
  const vec3i step = select(signs, vec3i(one), -vec3i(one));
  const vec3i out = select(signs, grid->local(), -vec3i(one));
  const vec3f delta = abs(ray.rdir*cellsize);
  const vec3f entry = ray.org+t*ray.dir;
  vec3i xyz = min(vec3i((entry-boxorg)/cellsize), grid->local()-vec3i(one));
  const vec3f floorentry = vec3f(xyz)*cellsize+boxorg;
  const vec3f exit = floorentry + select(signs, cellsize, vec3f(zero));
  vec3f tmax = vec3f(t)+(exit-entry)*ray.rdir;
  tmax = select(ray.dir==vec3f(zero),vec3f(FLT_MAX),tmax);

  for (;;) {
    const vec3f cellorg = gridpolicy<G>::cellorg(boxorg, xyz, cellsize);
    const auto isec = intersect(grid->fastsubgrid(xyz), cellorg, ray, t);
    if (isec.isec) return isec;
    if (tmax.x < tmax.y) {
      if (tmax.x < tmax.z) {
        xyz.x += step.x;
        if (xyz.x == out.x) return isecres(false);
        t = tmax.x;
        tmax.x += delta.x;
      } else {
        xyz.z += step.z;
        if (xyz.z == out.z) return isecres(false);
        t = tmax.z;
        tmax.z += delta.z;
      }
    } else {
      if (tmax.y < tmax.z) {
        xyz.y += step.y;
        if (xyz.y == out.y) return isecres(false);
        t = tmax.y;
        tmax.y += delta.y;
      } else {
        xyz.z += step.z;
        if (xyz.z == out.z) return isecres(false);
        t = tmax.z;
        tmax.z += delta.z;
      }
    }
  }
  return isecres(false);
}

isecres castray(const ray &ray) {
  const vec3f cellsize(one), boxorg(zero);
  const aabb box(boxorg, cellsize*vec3f(root.global()));
  const isecres res = slab(box, ray.org, ray.rdir, ray.tfar);
  if (!res.isec) return res;
  return intersect(&root, box.pmin, ray, res.t);
}

void castrayprout(float fovy, float aspect, float farplane) {
  using namespace game;
  const int w = 1024, h = 768;
  int *pixels = (int*)malloc(w*h*sizeof(int));
  const mat3x3f r = mat3x3f::rotate(vec3f(0.f,0.f,1.f),game::player1->yaw)*
                    mat3x3f::rotate(vec3f(0.f,1.f,0.f),game::player1->roll)*
                    mat3x3f::rotate(vec3f(-1.f,0.f,0.f),game::player1->pitch);
  const camera cam(game::player1->o, -r.vz, -r.vy, fovy, aspect);
  const vec3f cellsize(one), boxorg(zero);
  const aabb box(boxorg, cellsize*vec3f(root.global()));
  const int start = SDL_GetTicks();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const int offset = x+w*y;
      const ray ray = cam.generate(w, h, x, y);
      const isecres res = slab(box, ray.org, ray.rdir, ray.tfar);
      if (!res.isec) {
        pixels[offset] = 0;
        continue;
      }
      const auto isec = intersect(&root, box.pmin, ray, res.t);
      if (isec.isec) {
        const int d = min(int(isec.t), 255);
        pixels[offset] = d|(d<<8)|(d<<16)|(0xff<<24);
      } else
        pixels[offset] = 0;
    }
  }
  const int ms = SDL_GetTicks()-start;
  printf("\n%i ms, %f ray/s\n", ms, 1000.f*(w*h)/ms);
  writebmp(pixels, w, h, "hop.bmp");
  free(pixels);
}
} // namespace world
} // namespace cube

