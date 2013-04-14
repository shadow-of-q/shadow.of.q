#pragma once
#include "tools.hpp"
#include <cassert>
#include <cmath>
#include <cfloat>

namespace cube {
template<typename T> struct vec2;
template<typename T> struct vec3;
template<typename T> struct vec4;

// macros shared for swizzle declarations and definitions
#define sw21(A) sw22(A,x) sw22(A,y)
#define sw20 sw21(x) sw21(y)
#define sw31(A) sw32(A,x) sw32(A,y) sw32(A,z)
#define sw30 sw31(x) sw31(y) sw31(z)
#define sw41(A) sw42(A,x) sw42(A,y) sw42(A,z) sw42(A,w)
#define sw40 sw41(x) sw41(y) sw41(z) sw41(w)

// polymorphic constant values
#define CONSTANT_TYPE(TYPE,VALUE,NUM)\
static const struct TYPE {\
  INLINE TYPE(void) {}\
  INLINE operator double(void) const {return NUM;}\
  INLINE operator float (void) const {return NUM;}\
  INLINE operator s64(void) const {return NUM;}\
  INLINE operator u64(void) const {return NUM;}\
  INLINE operator s32(void) const {return NUM;}\
  INLINE operator u32(void) const {return NUM;}\
  INLINE operator s16(void) const {return NUM;}\
  INLINE operator u16(void) const {return NUM;}\
  INLINE operator s8 (void) const {return NUM;}\
  INLINE operator u8 (void) const {return NUM;}\
} VALUE;
CONSTANT_TYPE(zerotype,zero,0);
CONSTANT_TYPE(onetype,one,1);
CONSTANT_TYPE(twotype,two,2);
#undef CONSTANT_TYPE

// make the code terser
#define op operator
#define v2 vec2<T>
#define v3 vec3<T>
#define v4 vec4<T>
#define m33 mat3x3<T>
#define m43 mat4x3<T>
#define m44 mat4x4<T>
#define v2arg const vec2<T>&
#define v3arg const vec3<T>&
#define v4arg const vec4<T>&
#define m33arg const mat3x3<T>&
#define m43arg const mat4x3<T>&
#define m44arg const mat4x4<T>&
#define TINLINE template <typename T> INLINE
#define UINLINE template <typename U> INLINE

// all useful math functions
INLINE int abs    (int x)   {return ::abs(x);}
INLINE float sign (float x) {return x<0?-1.0f:1.0f;}
INLINE float rcp  (float x) {return 1.0f/x;}
INLINE float rsqrt(float x) {return 1.0f/::sqrtf(x);}
INLINE float abs  (float x) {return ::fabsf(x);}
INLINE float acos (float x) {return ::acosf (x);}
INLINE float asin (float x) {return ::asinf (x);}
INLINE float atan (float x) {return ::atanf (x);}
INLINE float cos  (float x) {return ::cosf  (x);}
INLINE float cosh (float x) {return ::coshf (x);}
INLINE float exp  (float x) {return ::expf  (x);}
INLINE float log  (float x) {return ::logf  (x);}
INLINE float log2 (float x) {return ::log(x) / 0.69314718055994530941723212145818f;}
INLINE float log10(float x) {return ::log10f(x);}
INLINE float sin  (float x) {return ::sinf  (x);}
INLINE float sinh (float x) {return ::sinhf (x);}
INLINE float sqrt (float x) {return ::sqrtf (x);}
INLINE float tan  (float x) {return ::tanf  (x);}
INLINE float tanh (float x) {return ::tanhf (x);}
INLINE float floor(float x) {return ::floorf (x);}
INLINE float ceil (float x) {return ::ceilf (x);}
INLINE float atan2(float y, float x) {return ::atan2f(y, x);}
INLINE float fmod (float x, float y) {return ::fmodf (x, y);}
INLINE float pow  (float x, float y) {return ::powf  (x, y);}
INLINE double abs  (double x) {return ::fabs(x);}
INLINE double sign (double x) {return x<0?-1.0:1.0;}
INLINE double log2 (double x) {return ::log(x) / 0.69314718055994530941723212145818;}
INLINE double log10(double x) {return ::log10(x);}
INLINE double rcp  (double x) {return 1.0/x;}
INLINE double rsqrt(double x) {return 1.0/::sqrt(x);}
TINLINE T max (T a, T b) {return a<b? b:a;}
TINLINE T min (T a, T b) {return a<b? a:b;}
TINLINE T min (T a, T b, T c) {return min(min(a,b),c);}
TINLINE T max (T a, T b, T c) {return max(max(a,b),c);}
TINLINE T max (T a, T b, T c, T d) {return max(max(a,b),max(c,d));}
TINLINE T min (T a, T b, T c, T d) {return min(min(a,b),min(c,d));}
TINLINE T min (T a, T b, T c, T d, T e) {return min(min(min(a,b),min(c,d)),e);}
TINLINE T max (T a, T b, T c, T d, T e) {return max(max(max(a,b),max(c,d)),e);}
TINLINE T clamp (T x, T lower = T(zero), T upper = T(one)) {return max(lower, min(x,upper));}
TINLINE T deg2rad (T x) {return x * T(1.74532925199432957692e-2);}
TINLINE T rad2deg (T x) {return x * T(5.72957795130823208768e1);}
TINLINE T sin2cos (T x) {return sqrt(max(T(zero),T(one)-x*x));}
TINLINE T cos2sin (T x) {return sin2cos(x);}

template<typename T> struct vec2 {
  T x, y;
  typedef T scalar;
  INLINE vec2(void) {}
  INLINE vec2(const vec2& v) {x = v.x; y = v.y;}
  UINLINE vec2 (const vec2<U>& a) : x(T(a.x)), y(T(a.y)) {}
  UINLINE vec2& op= (const vec2<U>& v) {x = v.x; y = v.y; return *this;}
  INLINE explicit vec2(const T &a) : x(a), y(a) {}
  INLINE explicit vec2(const T &x, const T &y) : x(x), y(y) {}
  INLINE explicit vec2(const T *a, int stride = 1) : x(a[0]), y(a[stride]) {}
  INLINE vec2(zerotype) : x(zero),y(zero) {}
  INLINE vec2(onetype) : x(one),y(one) {}
  INLINE const T& op[](int axis) const {return (&x)[axis];}
  INLINE T& op[](int axis) {return (&x)[axis];}
#define sw22(A,B) INLINE const v2 A##B(void); // declare all swizzles
  sw20
#undef sw22
#define sw22(A,B) sw23(A,B,x) sw23(A,B,y)
#define sw23(A,B,C) INLINE const v3 A##B##C(void);
  sw20
#undef sw23
#define sw23(A,B,C) sw24(A,B,C,x) sw24(A,B,C,y)
#define sw24(A,B,C,D) INLINE const v4 A##B##C##D(void);
  sw20
#undef sw23
#undef sw24
#undef sw23
#undef sw22
};

TINLINE v2 op+ (v2arg a)  {return v2(+a.x, +a.y);}
TINLINE v2 op- (v2arg a)  {return v2(-a.x, -a.y);}
TINLINE v2 abs (v2arg a)  {return v2(abs(a.x), abs(a.y));}
TINLINE v2 rcp (v2arg a)  {return v2(rcp(a.x), rcp(a.y));}
TINLINE v2 sqrt (v2arg a) {return v2(sqrt(a.x), sqrt(a.y));}
TINLINE v2 rsqrt(v2arg a) {return v2(rsqrt(a.x), rsqrt(a.y));}
TINLINE v2 op+ (v2arg a, v2arg b) {return v2(a.x+b.x, a.y+b.y);}
TINLINE v2 op- (v2arg a, v2arg b) {return v2(a.x-b.x, a.y-b.y);}
TINLINE v2 op* (v2arg a, v2arg b) {return v2(a.x*b.x, a.y*b.y);}
TINLINE v2 op* (const T &a, v2arg b)  {return v2(a*b.x, a*b.y);}
TINLINE v2 op* (v2arg a, const T &b)  {return v2(a.x*b, a.y*b);}
TINLINE v2 op/ (v2arg a, v2arg b) {return v2(a.x/b.x, a.y/b.y);}
TINLINE v2 op/ (v2arg a, const T &b)  {return v2(a.x/b, a.y /b);}
TINLINE v2 op/ (const T &a, v2arg b)  {return v2(a/b.x, a/b.y);}
TINLINE v2 min(v2arg a, v2arg b)  {return v2(min(a.x,b.x), min(a.y,b.y));}
TINLINE v2 max(v2arg a, v2arg b)  {return v2(max(a.x,b.x), max(a.y,b.y));}
TINLINE T dot(v2arg a, v2arg b)   {return a.x*b.x + a.y*b.y;}
TINLINE T length(v2arg a)      {return sqrt(dot(a,a));}
TINLINE v2 normalize(v2arg a)  {return a*rsqrt(dot(a,a));}
TINLINE T distance (v2arg a, v2arg b) {return length(a-b);}
TINLINE v2& op+= (v2& a, v2arg b) {a.x+=b.x; a.y+=b.y; return a;}
TINLINE v2& op-= (v2& a, v2arg b) {a.x-=b.x; a.y-=b.y; return a;}
TINLINE v2& op*= (v2& a, const T &b)  {a.x*=b; a.y*=b; return a;}
TINLINE v2& op/= (v2& a, const T &b)  {a.x/=b; a.y/=b; return a;}
TINLINE T reduce_add(v2arg a) {return a.x+a.y;}
TINLINE T reduce_mul(v2arg a) {return a.x*a.y;}
TINLINE T reduce_min(v2arg a) {return min(a.x, a.y);}
TINLINE T reduce_max(v2arg a) {return max(a.x, a.y);}
TINLINE bool op== (v2arg a, v2arg b) {return a.x==b.x && a.y==b.y;}
TINLINE bool op!= (v2arg a, v2arg b) {return a.x!=b.x || a.y!=b.y;}
TINLINE bool op<  (v2arg a, v2arg b) {
  if (a.x != b.x) return a.x < b.x;
  if (a.y != b.y) return a.y < b.y;
  return false;
}
TINLINE v2 select (bool s, v2arg t, v2arg f) {
  return v2(select(s,t.x,f.x), select(s,t.y,f.y));
}

template<typename T> struct vec3 {
  T x, y, z;
  typedef T scalar;
  INLINE vec3(void) {}
  INLINE vec3(const vec3& v) {x = v.x; y = v.y; z = v.z;}
  UINLINE vec3(const vec3<U>& a) : x(T(a.x)), y(T(a.y)), z(T(a.z)) {}
  UINLINE vec3& op= (const vec3<U>& v) {x = v.x; y = v.y; z = v.z; return *this;}
  INLINE explicit vec3(const T &a) : x(a), y(a), z(a) {}
  INLINE explicit vec3(const T &x, const T &y, const T &z) : x(x), y(y), z(z) {}
  INLINE explicit vec3(const T* a, int stride = 1) : x(a[0]), y(a[stride]), z(a[2*stride]) {}
  INLINE vec3 (zerotype) : x(zero),y(zero),z(zero) {}
  INLINE vec3 (onetype)  : x(one),y(one),z(one) {}
  INLINE const T& op[](int axis) const {return (&x)[axis];}
  INLINE T& op[](int axis) {return (&x)[axis];}
#define sw32(A,B) INLINE const v2 A##B(void); // declare all swizzles
  sw30
#undef sw32
#define sw32(A,B) sw33(A,B,x) sw33(A,B,y) sw33(A,B,z)
#define sw33(A,B,C) INLINE const v3 A##B##C(void);
  sw30
#undef sw33
#define sw33(A,B,C) sw34(A,B,C,x) sw34(A,B,C,y) sw34(A,B,C,z)
#define sw34(A,B,C,D) INLINE const v4 A##B##C##D(void);
  sw30
#undef sw33
#undef sw34
#undef sw33
#undef sw32
};

TINLINE v3 abs (v3arg a) {return v3(abs(a.x), abs(a.y), abs(a.z));}
TINLINE v3 rcp (v3arg a) {return v3(rcp(a.x), rcp(a.y), rcp(a.z));}
TINLINE v3 sqrt (v3arg a) {return v3(sqrt (a.x), sqrt (a.y), sqrt(a.z));}
TINLINE v3 rsqrt (v3arg a) {return v3(rsqrt(a.x), rsqrt(a.y), rsqrt(a.z));}
TINLINE v3 op+ (v3arg a) {return v3(+a.x, +a.y, +a.z);}
TINLINE v3 op- (v3arg a) {return v3(-a.x, -a.y, -a.z);}
TINLINE v3 op+ (v3arg a, v3arg b) {return v3(a.x+b.x, a.y+b.y, a.z+b.z);}
TINLINE v3 op- (v3arg a, v3arg b) {return v3(a.x-b.x, a.y-b.y, a.z-b.z);}
TINLINE v3 op* (v3arg a, v3arg b) {return v3(a.x*b.x, a.y*b.y, a.z*b.z);}
TINLINE v3 op/ (v3arg a, v3arg b) {return v3(a.x/b.x, a.y/b.y, a.z/b.z);}
TINLINE v3 op* (v3arg a, const T &b) {return v3(a.x*b, a.y*b, a.z*b);}
TINLINE v3 op/ (v3arg a, const T &b) {return v3(a.x/b, a.y/b, a.z/b);}
TINLINE v3 op* (const T &a, v3arg b) {return v3(a*b.x, a*b.y, a*b.z);}
TINLINE v3 op/ (const T &a, v3arg b) {return v3(a/b.x, a/b.y, a/b.z);}
TINLINE v3 op+= (v3& a, v3arg b) {a.x+=b.x; a.y+=b.y; a.z+=b.z; return a;}
TINLINE v3 op-= (v3& a, v3arg b) {a.x-=b.x; a.y-=b.y; a.z-=b.z; return a;}
TINLINE v3 op*= (v3& a, const T &b) {a.x*=b; a.y*=b; a.z*=b; return a;}
TINLINE v3 op/= (v3& a, const T &b) {a.x/=b; a.y/=b; a.z/=b; return a;}
TINLINE bool op== (v3arg a, v3arg b) {return a.x==b.x && a.y==b.y && a.z==b.z;}
TINLINE bool op!= (v3arg a, v3arg b) {return a.x!=b.x || a.y!=b.y || a.z!=b.z;}
TINLINE bool op<  (v3arg a, v3arg b) {
  if (a.x != b.x) return a.x < b.x;
  if (a.y != b.y) return a.y < b.y;
  if (a.z != b.z) return a.z < b.z;
  return false;
}
TINLINE T reduce_add (v3arg a) {return a.x+a.y+a.z;}
TINLINE T reduce_mul (v3arg a) {return a.x*a.y*a.z;}
TINLINE T reduce_min (v3arg a) {return min(a.x,a.y,a.z);}
TINLINE T reduce_max (v3arg a) {return max(a.x,a.y,a.z);}
TINLINE v3 min (v3arg a, v3arg b) {return v3(min(a.x,b.x),min(a.y,b.y),min(a.z,b.z));}
TINLINE v3 max (v3arg a, v3arg b) {return v3(max(a.x,b.x),max(a.y,b.y),max(a.z,b.z));}
TINLINE v3 select (bool s, v3arg t, v3arg f) {
  return v3(select(s,t.x,f.x), select(s,t.y,f.y), select(s,t.z,f.z));
}
TINLINE T length (v3arg a) {return sqrt(dot(a,a));}
TINLINE T dot (v3arg a, v3arg b) {return a.x*b.x + a.y*b.y + a.z*b.z;}
TINLINE T distance (v3arg a, v3arg b) {return length(a-b);}
TINLINE v3 normalize(v3arg a) {return a*rsqrt(dot(a,a));}
TINLINE v3 cross(v3arg a, v3arg b) {
  return v3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x);
}
TINLINE bool rejectxy(v3 u, v3 v, T m) {return abs(v.x-u.x)>m || abs(v.y-u.y)>m;}

// 4d vector
template<typename T> struct vec4 {
  T x, y, z, w;
  typedef T scalar;
  INLINE vec4(void) {}
  INLINE vec4(const vec4& v) {x = v.x; y = v.y; z = v.z; w = v.w;}
  UINLINE vec4(const vec4<U>& a) : x(T(a.x)), y(T(a.y)), z(T(a.z)), w(T(a.w)) {}
  UINLINE vec4& op= (const vec4<U>& v) {x = v.x; y = v.y; z = v.z; w = v.w; return *this;}
  INLINE explicit vec4(const T &a) : x(a), y(a), z(a), w(a) {}
  INLINE explicit vec4(const T &x, const T &y, const T &z, const T &w) : x(x), y(y), z(z), w(w) {}
  INLINE explicit vec4(const T *a, int stride = 1) : x(a[0]), y(a[stride]), z(a[2*stride]), w(a[3*stride]) {}
  INLINE vec4 (zerotype) : x(zero),y(zero),z(zero),w(zero) {}
  INLINE vec4 (onetype)  : x(one),y(one),z(one),w(one) {}
  INLINE const T& op[](int axis) const {return (&x)[axis];}
  INLINE T& op[](int axis) {return (&x)[axis];}
#define sw42(A,B) INLINE const v2 A##B(void); // declare all swizzles
  sw40
#undef sw42
#define sw42(A,B) sw43(A,B,x) sw43(A,B,y) sw43(A,B,z) sw43(A,B,w)
#define sw43(A,B,C) INLINE const v3 A##B##C(void);
  sw40
#undef sw43
#define sw43(A,B,C) sw44(A,B,C,x) sw44(A,B,C,y) sw44(A,B,C,z) sw44(A,B,C,w)
#define sw44(A,B,C,D) INLINE const v4 A##B##C##D(void);
  sw40
#undef sw43
#undef sw44
#undef sw43
#undef sw42
};

TINLINE v4 op+ (v4arg a) {return v4(+a.x, +a.y, +a.z, +a.w);}
TINLINE v4 op- (v4arg a) {return v4(-a.x, -a.y, -a.z, -a.w);}
TINLINE v4 op+ (v4arg a, v4arg b) {return v4(a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w);}
TINLINE v4 op- (v4arg a, v4arg b) {return v4(a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w);}
TINLINE v4 op* (v4arg a, v4arg b) {return v4(a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w);}
TINLINE v4 op/ (v4arg a, v4arg b) {return v4(a.x/b.x, a.y/b.y, a.z/b.z, a.w/b.w);}
TINLINE v4 op* (v4arg a, const T &b) {return v4(a.x*b, a.y*b, a.z*b, a.w*b);}
TINLINE v4 op/ (v4arg a, const T &b) {return v4(a.x/b, a.y/b, a.z/b, a.w/b);}
TINLINE v4 op* (const T &a, v4arg b) {return v4(a*b.x, a*b.y, a*b.z, a*b.w);}
TINLINE v4 op/ (const T &a, v4arg b) {return v4(a/b.x, a/b.y, a/b.z, a/b.w);}
TINLINE v4& op+= (v4& a, v4arg b) {a.x+=b.x; a.y+=b.y; a.z+=b.z; a.w+=b.w; return a;}
TINLINE v4& op-= (v4& a, v4arg b) {a.x-=b.x; a.y-=b.y; a.z-=b.z; a.w-=b.w; return a;}
TINLINE v4& op*= (v4& a, const T &b) {a.x*=b; a.y*=b; a.z*=b; a.w*=b; return a;}
TINLINE v4& op/= (v4& a, const T &b) {a.x/=b; a.y/=b; a.z/=b; a.w/=b; return a;}
TINLINE bool op== (v4arg a, v4arg b) {return a.x==b.x && a.y==b.y && a.z==b.z && a.w==b.w;}
TINLINE bool op!= (v4arg a, v4arg b) {return a.x!=b.x || a.y!=b.y || a.z!=b.z || a.w!=b.w;}
TINLINE bool op<  (v4arg a, v4arg b) {
  if (a.x != b.x) return a.x < b.x;
  if (a.y != b.y) return a.y < b.y;
  if (a.z != b.z) return a.z < b.z;
  if (a.w != b.w) return a.w < b.w;
  return false;
}
TINLINE T reduce_add(v4arg a) {return a.x + a.y + a.z + a.w;}
TINLINE T reduce_mul(v4arg a) {return a.x * a.y * a.z * a.w;}
TINLINE T reduce_min(v4arg a) {return min(a.x, a.y, a.z, a.w);}
TINLINE T reduce_max(v4arg a) {return max(a.x, a.y, a.z, a.w);}
TINLINE v4 min(v4arg a, v4arg b) {return v4(min(a.x,b.x), min(a.y,b.y), min(a.z,b.z), min(a.w,b.w));}
TINLINE v4 max(v4arg a, v4arg b) {return v4(max(a.x,b.x), max(a.y,b.y), max(a.z,b.z), max(a.w,b.w));}
TINLINE v4 abs (v4arg a) {return v4(abs(a.x), abs(a.y), abs(a.z), abs(a.w));}
TINLINE v4 rcp (v4arg a) {return v4(rcp(a.x), rcp(a.y), rcp(a.z), rcp(a.w));}
TINLINE v4 sqrt (v4arg a) {return v4(sqrt (a.x), sqrt (a.y), sqrt (a.z), sqrt (a.w));}
TINLINE v4 rsqrt(v4arg a) {return v4(rsqrt(a.x), rsqrt(a.y), rsqrt(a.z), rsqrt(a.w));}
TINLINE T length(v4arg a) {return sqrt(dot(a,a));}
TINLINE T dot (v4arg a, v4arg b) {return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;}
TINLINE v4 normalize(v4arg a) {return a*rsqrt(dot(a,a));}
TINLINE T distance (v4arg a, v4arg b) {return length(a-b);}
TINLINE v4 select (bool s, v4arg t, v4arg f) {
  return v4(select(s,t.x,f.x), select(s,t.y,f.y), select(s,t.z,f.z), select(s,t.w,f.w));
}

template<typename T> struct mat3x3 {
  vec3<T> vx,vy,vz;
  INLINE mat3x3(void) {}
  INLINE mat3x3(const mat3x3 &m) {vx = m.vx; vy = m.vy; vz = m.vz;}
  INLINE mat3x3& op= (const mat3x3 &m) {vx = m.vx; vy = m.vy; vz = m.vz; return *this;}
  mat3x3(v3arg n);
  UINLINE explicit mat3x3(const mat3x3<U> &s) : vx(s.vx), vy(s.vy), vz(s.vz) {}
  INLINE mat3x3(v3arg vx, v3arg vy, v3arg vz) : vx(vx), vy(vy), vz(vz) {}
  INLINE mat3x3(T m00, T m01, T m02, T m10, T m11, T m12, T m20, T m21, T m22) :
    vx(m00,m10,m20), vy(m01,m11,m21), vz(m02,m12,m22) {}
  INLINE mat3x3(zerotype) : vx(zero),vy(zero),vz(zero) {}
  INLINE mat3x3(onetype)  : vx(one,zero,zero),vy(zero,one,zero),vz(zero,zero,one) {}
  INLINE mat3x3 adjoint(void) const {
    return mat3x3(cross(vy,vz), cross(vz,vx), cross(vx,vy)).transposed();
  }
  INLINE mat3x3 transposed(void) const {
    return mat3x3(vx.x,vx.y,vx.z, vy.x,vy.y,vy.z, vz.x,vz.y,vz.z);
  }
  INLINE mat3x3 inverse(void) const {return rcp(det())*adjoint();}
  INLINE T det(void) const {return dot(vx,cross(vy,vz));}
  static INLINE mat3x3 scale(v3arg s) {
    return mat3x3(s.x,zero,zero,zero,s.y,zero,zero,zero,s.z);
  }
  static INLINE mat3x3 rotate(v3arg _u, T r) {
    const v3 u = normalize(_u);
    const T s = sin(r), c = cos(r);
    return mat3x3(u.x*u.x+(one-u.x*u.x)*c,u.x*u.y*(one-c)-u.z*s,  u.x*u.z*(one-c)+u.y*s,
                  u.x*u.y*(one-c)+u.z*s,  u.y*u.y+(one-u.y*u.y)*c,u.y*u.z*(one-c)-u.x*s,
                  u.x*u.z*(one-c)-u.y*s,  u.y*u.z*(one-c)+u.x*s,  u.z*u.z+(one-u.z*u.z)*c);
  }
};

TINLINE m33 op- (m33arg a) {return m33(-a.vx,-a.vy,-a.vz);}
TINLINE m33 op+ (m33arg a) {return m33(+a.vx,+a.vy,+a.vz);}
TINLINE m33 rcp (m33arg a) {return a.inverse();}
TINLINE m33 op+ (m33arg a, m33arg b) {return m33(a.vx+b.vx,a.vy+b.vy,a.vz+b.vz);}
TINLINE m33 op- (m33arg a, m33arg b) {return m33(a.vx-b.vx,a.vy-b.vy,a.vz-b.vz);}
TINLINE m33 op* (m33arg a, m33arg b) {return m33(a*b.vx, a*b.vy, a*b.vz);}
TINLINE m33 op/ (m33arg a, m33arg b) {return a*rcp(b);}
TINLINE v3  op* (m33arg a, v3arg  b) {return b.x*a.vx + b.y*a.vy + b.z*a.vz;}
TINLINE m33 op* (T a, m33arg b)   {return m33(a*b.vx, a*b.vy, a*b.vz);}
TINLINE m33 op/ (m33arg a, T b)   {return a * rcp(b);}
TINLINE m33& op/= (m33& a, T b)   {return a = a/b;}
TINLINE m33& op*= (m33& a, T b)   {return a = a*b;}
TINLINE m33& op*= (m33& a, m33 b) {return a = a*b;}
TINLINE m33& op/= (m33& a, m33 b) {return a = a/b;}
TINLINE bool op== (m33arg a, m33arg b)  {return (a.vx==b.vx) && (a.vy == b.vy) && (a.vz == b.vz);}
TINLINE bool op!= (m33arg a, m33arg b)  {return (a.vx!=b.vx) || (a.vy != b.vy) || (a.vz != b.vz);}
TINLINE v3 xfmpoint (m33arg s, v3arg a) {return a.x*s.vx + a.y*s.vy + a.z*s.vz;}
TINLINE v3 xfmvector(m33arg s, v3arg a) {return a.x*s.vx + a.y*s.vy + a.z*s.vz;}
TINLINE v3 xfmnormal(m33arg s, v3arg a) {return xfmvector(s.inverse().transposed(),a);}
TINLINE m33 frame(v3arg N) {
  const v3 dx0 = cross(v3(T(one),T(zero),T(zero)),N);
  const v3 dx1 = cross(v3(T(zero),T(one),T(zero)),N);
  const v3 dx = normalize(select(dot(dx0,dx0) > dot(dx1,dx1),dx0,dx1));
  const v3 dy = normalize(cross(N,dx));
  return m33(dx,dy,N);
}

template<typename T> struct mat4x3 {
  mat3x3<T> l;
  vec3<T> p;
  INLINE mat4x3(void) {}
  INLINE mat4x3(const mat4x3 &m) {l = m.l; p = m.p;}
  INLINE mat4x3(v3arg vx, v3arg vy, v3arg vz, v3arg p) : l(vx,vy,vz), p(p) {}
  INLINE mat4x3(m33arg l, v3arg p) : l(l), p(p) {}
  INLINE mat4x3& op= (const mat4x3 &m) {l=m.l; p=m.p; return *this;}
  UINLINE explicit mat4x3(const mat4x3<U>& s) : l(s.l), p(s.p) {}
  INLINE mat4x3(zerotype) : l(zero),p(zero) {}
  INLINE mat4x3(onetype)  : l(one),p(zero) {}

  static INLINE mat4x3 scale(v3arg s) {return mat4x3(m33::scale(s),zero);}
  static INLINE mat4x3 translate(v3arg p) {return mat4x3(one,p);}
  static INLINE mat4x3 rotate(v3arg u, T r) {return mat4x3(m33::rotate(u,r),zero);}
  static INLINE mat4x3 rotate(v3arg p, v3arg u, T r) {return translate(p)*rotate(u,r)*translate(-p);}
  static INLINE mat4x3 lookat(v3arg eye, v3arg point, v3arg up) {
    const v3 z = normalize(point-eye);
    const v3 u = normalize(cross(up,z));
    const v3 v = normalize(cross(z,u));
    return mat4x3(m33(u,v,z),eye);
  }
};

TINLINE m43 op- (m43arg a) {return m43(-a.l,-a.p);}
TINLINE m43 op+ (m43arg a) {return m43(+a.l,+a.p);}
TINLINE m43 rcp (m43arg a) {m33 il = rcp(a.l); return m43(il,-il*a.p);}
TINLINE m43 op+ (m43arg a, m43arg b)  {return m43(a.l+b.l,a.p+b.p);}
TINLINE m43 op- (m43arg a, m43arg b)  {return m43(a.l-b.l,a.p-b.p);}
TINLINE m43 op* (T a, m43 b)          {return m43(a*b.l,a*b.p);}
TINLINE m43 op* (m43arg a, m43arg b)  {return m43(a.l*b.l,a.l*b.p+a.p);}
TINLINE m43 op/ (m43arg a, m43arg b)  {return a * rcp(b);}
TINLINE m43 op/ (m43arg a, T b) {return a * rcp(b);}
TINLINE m43& op*= (m43& a, T b) {return a = a*b;}
TINLINE m43& op/= (m43& a, T b) {return a = a/b;}
TINLINE m43& op*= (m43& a, m43arg b) {return a = a*b;}
TINLINE m43& op/= (m43& a, m43arg b) {return a = a/b;}
TINLINE bool op== (m43arg a, m43arg b)  {return (a.l==b.l) && (a.p==b.p);}
TINLINE bool op!= (m43arg a, m43arg b)  {return (a.l!=b.l) || (a.p!=b.p);}
TINLINE v3 xfmpoint (m43arg m, v3arg p) {return xfmpoint(m.l,p) + m.p;}
TINLINE v3 xfmvector(m43arg m, v3arg v) {return xfmvector(m.l,v);}
TINLINE v3 xfmnormal(m43arg m, v3arg n) {return xfmnormal(m.l,n);}

template<typename T> struct mat4x4 {
  vec4<T> vx,vy,vz,vw;
  INLINE mat4x4(void) {}
  INLINE mat4x4(m44arg m) {vx=m.vx; vy=m.vy; vz=m.vz; vw=m.vw;}
  UINLINE mat4x4(const mat4x4<U> &m) :
    vx(v4(m.vx)), vy(v4(m.vy)), vz(v4(m.vz)), vw(v4(m.vw)) {}
  UINLINE explicit mat4x4 (U s) {
    vx = v4(T(s), zero, zero, zero);
    vy = v4(zero, T(s), zero, zero);
    vz = v4(zero, zero, T(s), zero);
    vw = v4(zero, zero, zero, T(s));
  }
  INLINE mat4x4(T s) {
    vx = v4(s, zero, zero, zero);
    vy = v4(zero, s, zero, zero);
    vz = v4(zero, zero, s, zero);
    vw = v4(zero, zero, zero, s);
  }
  INLINE mat4x4(v4arg c0, v4arg c1, v4arg c2, v4arg c3) :
    vx(c0), vy(c1), vz(c2), vw(c3) {}
  INLINE mat4x4 (m33arg m) {
    vx = v4(m.vx, zero);
    vy = v4(m.vy, zero);
    vz = v4(m.vz, zero);
    vw = v4(zero, zero, zero, one);
  }
  INLINE mat4x4 (m43arg m) {
    vx = v4(m.vx, zero);
    vy = v4(m.vy, zero);
    vz = v4(m.vz, zero);
    vw = v4(m.vw, one);
  }
  INLINE mat4x4(zerotype) : vx(zero),vy(zero),vz(zero),vw(zero) {}
  INLINE mat4x4(onetype) {
    vx = v4(one, zero, zero, zero);
    vy = v4(zero, one, zero, zero);
    vz = v4(zero, zero, one, zero);
    vw = v4(zero, zero, zero, one);
  }
  m44& op= (const m44 &m) {vx=m.vx;vy=m.vy;vz=m.vz;vw=m.vw;return *this;}
  m44 inverse(void) const;
  INLINE v4& op[] (int i) {return (&vx)[i];}
  INLINE const v4& op[] (int i) const {return (&vx)[i];}
};

TINLINE m44 op- (m44arg a) {return m44(-a.vx,-a.vy,-a.vz,-a.vw);}
TINLINE m44 op+ (m44arg a) {return m44(+a.vx,+a.vy,+a.vz,+a.vw);}
TINLINE m44 op+ (m44arg m, T s) {return m44(m.vx+s, m.vy+s, m.vz+s, m.vw+s);}
TINLINE m44 op- (m44arg m, T s) {return m44(m.vx-s, m.vy-s, m.vz-s, m.vw-s);}
TINLINE m44 op* (m44arg m, T s) {return m44(m.vx*s, m.vy*s, m.vz*s, m.vw*s);}
TINLINE m44 op/ (m44arg m, T s) {return m44(m.vx/s, m.vy/s, m.vz/s, m.vw/s);}
TINLINE m44 op+ (T s, m44arg m) {return m44(m.vx+s, m.vy+s, m.vz+s, m.vw+s);}
TINLINE m44 op- (T s, m44arg m) {return m44(m.vx-s, m.vy-s, m.vz-s, m.vw-s);}
TINLINE m44 op* (T s, m44arg m) {return m44(m.vx*s, m.vy*s, m.vz*s, m.vw*s);}
TINLINE m44 op+ (m44arg m, m44arg n) {return m44(m.vx+n.vx, m.vy+n.vy, m.vz+n.vz, m.vw+n.vw);}
TINLINE m44 op- (m44arg m, m44arg n) {return m44(m.vx-n.vx, m.vy-n.vy, m.vz-n.vz, m.vw-n.vw);}
TINLINE v4  op* (m44arg m, v4arg v) {
  return v4(m.vx.x*v.x + m.vy.x*v.y + m.vz.x*v.z + m.vw.x*v.w,
            m.vx.y*v.x + m.vy.y*v.y + m.vz.y*v.z + m.vw.y*v.w,
            m.vx.z*v.x + m.vy.z*v.y + m.vz.z*v.z + m.vw.z*v.w,
            m.vx.w*v.x + m.vy.w*v.y + m.vz.w*v.z + m.vw.w*v.w);
}
TINLINE v4 op* (v4arg v, m44arg m) {
  return v4(dot(m.vx,v), dot(m.vy,v), dot(m.vz,v), dot(m.vw,v));
}
TINLINE m44 op* (m44arg m, m44arg n) {
  const v4 a0 = m.vx, a1 = m.vy, a2 = m.vz, a3 = m.vw;
  const v4 b0 = n.vx, b1 = n.vy, b2 = n.vz, b3 = n.vw;
  return m44(a0*b0.x + a1*b0.y + a2*b0.z + a3*b0.w,
             a0*b1.x + a1*b1.y + a2*b1.z + a3*b1.w,
             a0*b2.x + a1*b2.y + a2*b2.z + a3*b2.w,
             a0*b3.x + a1*b3.y + a2*b3.z + a3*b3.w);
}
TINLINE m44& op*= (m44& a, T b) {return a = a*b;}
TINLINE m44& op/= (m44& a, T b) {return a = a/b;}
TINLINE m44& op*= (m44& a, m44arg b) {return a = a*b;}
TINLINE m44& op/= (m44& a, m44arg b) {return a = a/b;}
TINLINE v4 op/ (m44arg m, v4arg v) {return m.inverse() * v;}
TINLINE v4 op/ (v4arg v, m44arg m) {return v * m.inverse();}
TINLINE m44 op/ (m44arg m, m44arg n) {return m * n.inverse();}
TINLINE bool op== (m44arg m, m44arg n) {return (m.vx==n.x) && (m.vy==n.y) && (m.vz==n.z) && (m.vw==n.w);}
TINLINE bool op!= (m44arg m, m44arg n) {return (m.vx!=n.x) || (m.vy!=n.y) || (m.vz!=n.z) || (m.vw!=n.w);}
TINLINE m44 translate(m44arg m, v3arg v) {
  m44 dst(m);
  dst.vw = m.vx*v.x + m.vy*v.y + m.vz*v.z + m.vw;
  return dst;
}
TINLINE m44 lookat(v3arg eye, v3arg center, v3arg up) {
  const v3 f = normalize(center - eye);
  const v3 u = normalize(up);
  const v3 s = normalize(cross(f, u));
  const v3 t = cross(s, f);
  m44 dst(one);
  dst.vx.x = s.x; dst.vy.x = s.y; dst.vz.x = s.z;
  dst.vx.y = t.x; dst.vy.y = t.y; dst.vz.y = t.z;
  dst.vx.z =-f.x; dst.vy.z =-f.y; dst.vz.z =-f.z;
  return translate(dst, -eye);
}
TINLINE m44 perspective(T fovy, T aspect, T znear, T zfar) {
  const T range = tan(deg2rad(fovy / T(two))) * znear;
  const T left = -range * aspect;
  const T right = range * aspect;
  const T bottom = -range;
  const T top = range;
  m44 dst(zero);
  dst.vx.x = (T(two) * znear) / (right - left);
  dst.vy.y = (T(two) * znear) / (top - bottom);
  dst.vz.z = -(zfar + znear) / (zfar - znear);
  dst.vz.w = -T(one);
  dst.vw.z = -(T(two) * zfar * znear) / (zfar - znear);
  return dst;
}
TINLINE m44 rotate(m44arg m, T angle, v3arg v) {
  m44 rot(zero), dst(zero);
  const T a = deg2rad(angle);
  const T c = cos(a);
  const T s = sin(a);
  const v3 axis = normalize(v);
  const v3 temp = (T(one) - c) * axis;
  rot.vx.x = c + temp.x*axis.x;
  rot.vx.y = T(zero) + temp.x*axis.y + s*axis.z;
  rot.vx.z = T(zero) + temp.x*axis.z - s*axis.y;
  rot.vy.x = T(zero) + temp.y*axis.x - s*axis.z;
  rot.vy.y = c + temp.y*axis.y;
  rot.vy.z = T(zero) + temp.y*axis.z + s*axis.x;
  rot.vz.x = T(zero) + temp.z*axis.x + s*axis.y;
  rot.vz.y = T(zero) + temp.z*axis.y - s*axis.x;
  rot.vz.z = c + temp.z*axis.z;
  dst.vx = m.vx*rot.vx.x + m.vy*rot.vx.y + m.vz*rot.vx.z;
  dst.vy = m.vx*rot.vy.x + m.vy*rot.vy.y + m.vz*rot.vy.z;
  dst.vz = m.vx*rot.vz.x + m.vy*rot.vz.y + m.vz*rot.vz.z;
  dst.vw = m.vw;
  return dst;
}
TINLINE m44 ortho(T left, T right, T bottom, T top, T znear, T zfar) {
  m44 dst(one);
  dst.vx.x = T(two) / (right - left);
  dst.vy.y = T(two) / (top - bottom);
  dst.vz.z = -T(two) / (zfar - znear);
  dst.vw.x = -(right + left) / (right - left);
  dst.vw.y = -(top + bottom) / (top - bottom);
  dst.vw.z = -(zfar + znear) / (zfar - znear);
  return dst;
}
TINLINE m44 scale(m44arg m, v3arg v) {
  m44 dst;
  dst.vx = m.vx*v.x;
  dst.vy = m.vy*v.y;
  dst.vz = m.vz*v.z;
  dst.vw = m.vw;
  return dst;
}
TINLINE v3 unproject(v3arg win, m44arg model, m44arg proj, const vec4<int> &viewport) {
  const m44 p = proj*model;
  const m44 final = p.inverse();
  v4 in(win.x, win.y, win.z, T(one));
  in.x = T(two) * (win.x - T(viewport.x)) / T(viewport.z) - T(one);
  in.y = T(two) * (win.y - T(viewport.y)) / T(viewport.w) - T(one);
  in.z = T(two) * win.z - T(one);
  const v4 out = final*in;
  return out.xyz() / out.w;
}

// convenient variable size static vector
template <typename U, int n> struct vvec {
  template <typename... T> INLINE vvec(T... args) {set(0,args...);}
  template <typename First, typename... Rest>
  INLINE void set(int i, First first, Rest... rest) {
    assign(first, i);
    set(i,rest...);
  }
  INLINE void assign(U x, int &i) {this->v[i++] = x;}
  INLINE void assign(vec2<U> u, int &i) {v[i++]=u.x; v[i++]=u.y;}
  INLINE void assign(vec3<U> u, int &i) {v[i++]=u.x; v[i++]=u.y; v[i++]=u.z;}
  INLINE void assign(vec4<U> u, int &i) {v[i++]=u.x; v[i++]=u.y; v[i++]=u.z; v[i++]=u.w;}
  INLINE void set(int i) {}
  float &operator[] (int i) { return v[i]; }
  const float &operator[] (int i) const { return v[i]; }
  U v[n];
};

// define all swizzles for vec2
#define sw22(A,B) TINLINE const v2 v2::A##B(void) {return v2(A,B);}
sw20
#undef sw22
#define sw22(A,B) sw23(A,B,x) sw23(A,B,y)
#define sw23(A,B,C) TINLINE const v3 v2::A##B##C(void) {return v3(A,B,C);}
sw20
#undef sw23
#define sw23(A,B,C) sw24(A,B,C,x) sw24(A,B,C,y)
#define sw24(A,B,C,D) TINLINE const v4 v2::A##B##C##D(void) {return v4(A,B,C,D);}
sw20
#undef sw24
#undef sw23
#undef sw22

// define all swizzles for vec3
#define sw32(A,B) TINLINE const v2 v3::A##B(void) {return v2(A,B);}
sw30
#undef sw32
#define sw32(A,B) sw33(A,B,x) sw33(A,B,y) sw33(A,B,z)
#define sw33(A,B,C) TINLINE const v3 v3::A##B##C(void) {return v3(A,B,C);}
sw30
#undef sw33
#define sw33(A,B,C) sw34(A,B,C,x) sw34(A,B,C,y) sw34(A,B,C,z)
#define sw34(A,B,C,D) TINLINE const v4 v3::A##B##C##D(void) {return v4(A,B,C,D);}
sw30
#undef sw34
#undef sw33
#undef sw32

// define all swizzles for vec4
#define sw42(A,B) TINLINE const v2 v4::A##B(void) {return v2(A,B);}
sw40
#undef sw42
#define sw42(A,B) sw43(A,B,x) sw43(A,B,y) sw43(A,B,z) sw43(A,B,w)
#define sw43(A,B,C) TINLINE const v3 v4::A##B##C(void) {return v3(A,B,C);}
sw40
#undef sw43
#define sw43(A,B,C) sw44(A,B,C,x) sw44(A,B,C,y) sw44(A,B,C,z) sw44(A,B,C,w)
#define sw44(A,B,C,D) TINLINE const v4 v4::A##B##C##D(void) {return v4(A,B,C,D);}
sw40
#undef sw44
#undef sw43
#undef sw42

// commonly used types
typedef mat3x3<float> mat3x3f;
typedef mat3x3<double> mat3x3d;
typedef mat4x3<mat3x3f> mat4x3f;
typedef mat4x4<float> mat4x4f;
typedef mat4x4<double> mat4x4d;
typedef vec2<bool> vec2b;
typedef vec2<int> vec2i;
typedef vec2<float> vec2f;
typedef vec2<double> vec2d;
typedef vec3<bool> vec3b;
typedef vec3<int> vec3i;
typedef vec3<float> vec3f;
typedef vec3<double> vec3d;
typedef vec4<bool> vec4b;
typedef vec4<int> vec4i;
typedef vec4<float> vec4f;
typedef vec4<double> vec4d;
template <int n> using vvecd = vvec<double,n>;
template <int n> using vvecf = vvec<float,n>;
template <int n> using vveci = vvec<int,n>;

#undef TINLINE
#undef UINLINE
#undef op
#undef v2
#undef v3
#undef v4
#undef m33
#undef m43
#undef m44
#undef v2arg
#undef v3arg
#undef v4arg
#undef m33arg
#undef m43arg
#undef m44arg
#undef sw21
#undef sw20
#undef sw31
#undef sw30
#undef sw41
#undef sw40
} // namespace cube

