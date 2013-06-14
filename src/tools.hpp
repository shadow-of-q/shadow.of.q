#pragma once
#if defined(EMSCRIPTEN)
#define IF_EMSCRIPTEN(X) X
#define IF_NOT_EMSCRIPTEN(X)
#else
#define IF_EMSCRIPTEN(X)
#define IF_NOT_EMSCRIPTEN(X) X
#endif // EMSCRIPTEN

#ifdef _MSC_VER
#undef NOINLINE
#define NOINLINE        __declspec(noinline)
#define INLINE          __forceinline
#define RESTRICT        __restrict
#define THREAD          __declspec(thread)
#define ALIGNED(...)    __declspec(align(__VA_ARGS__))
//#define __FUNCTION__  __FUNCTION__
#define DEBUGBREAK      __debugbreak()
#define alloca _alloca
#else
#undef NOINLINE
#undef INLINE
#define NOINLINE        __attribute__((noinline))
#define INLINE          inline __attribute__((always_inline))
#define RESTRICT        __restrict
#define THREAD          __thread
#define ALIGNED(...)    __attribute__((aligned(__VA_ARGS__)))
#define __FUNCTION__    __PRETTY_FUNCTION__
#define DEBUGBREAK      asm ("int $3")
#endif

#define DEFAULT_ALIGNMENT 8
#define DEFAULT_ALIGNED ALIGNED(DEFAULT_ALIGNMENT)

#ifdef __GNUC__
#define gamma __gamma
#endif

#define _USE_MATH_DEFINES
#include <cmath>

#ifdef __GNUC__
#undef gamma
#endif

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <climits>
#include <cassert>
#include <cfloat>
#ifdef __GNUC__
#include <new>
#else
#include <new.h>
#endif
#if defined(_MSC_VER)
#include <malloc.h>
#endif

#define ASSERT assert

namespace cube {

/*-------------------------------------------------------------------------
 - Standard types
 -------------------------------------------------------------------------*/
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

#if defined(__MSVC__)
typedef          __int64 s64;
typedef unsigned __int64 u64;
typedef          __int32 s32;
typedef unsigned __int32 u32;
typedef          __int16 s16;
typedef unsigned __int16 u16;
typedef          __int8  s8;
typedef unsigned __int8  u8;
#else
typedef          long long s64;
typedef unsigned long long u64;
typedef                int s32;
typedef unsigned       int u32;
typedef              short s16;
typedef unsigned     short u16;
typedef               char s8;
typedef unsigned      char u8;
#endif

/*-------------------------------------------------------------------------
 - Various useful macros, helper classes or functions
 -------------------------------------------------------------------------*/
#if defined(WIN32)
  #undef min
  #undef max
#if defined(_MSC_VER)
  INLINE bool finite (float x) {return _finite(x) != 0;}
#endif
#endif

#define rnd(max) (rand()%(max))
#define rndreset() (srand(1))
#define rndtime() {loopi(lastmillis()()&0xF) rnd(i+1);}
#define loop(v,m) for(int v = 0; v<(m); v++)
#define loopi(m) loop(i,m)
#define loopj(m) loop(j,m)
#define loopk(m) loop(k,m)
#define loopl(m) loop(l,m)
#define loopv(v)    for(int i = 0; i<(v).length(); ++i)
#define loopvj(v)   for(int j = 0; j<(v).length(); ++j)
#define loopvrev(v) for(int i = (v).length()-1; i>=0; --i)

#ifdef WIN32
#define PATHDIV '\\'
#else
#define __cdecl
#define _vsnprintf vsnprintf
#define PATHDIV '/'
#endif

// concatenation
#define JOIN(X, Y) _DO_JOIN(X, Y)
#define _DO_JOIN(X, Y) _DO_JOIN2(X, Y)
#define _DO_JOIN2(X, Y) X##Y

// align X on A
#define ALIGN(X,A) (((X) % (A)) ? ((X) + (A) - ((X) % (A))) : (X))

// useful for repetitive calls to same functions
#define MAKE_VARIADIC(NAME)\
INLINE void NAME##v(void) {}\
template <typename First, typename... Rest>\
INLINE void NAME##v(First first, Rest... rest) {\
  NAME(first);\
  NAME##v(rest...);\
}

// global variable setter / getter
#define GLOBAL_VAR(NAME,VARNAME,TYPE)\
TYPE NAME(void) { return VARNAME; }\
void set##NAME(TYPE x) { VARNAME = x; }

#define GLOBAL_VAR_DECL(NAME,TYPE)\
extern TYPE NAME(void);\
extern void set##NAME(TYPE x);

#define ARRAY_ELEM_N(X) (sizeof(X) / sizeof(X[0]))
#define MEMSET(X,V) memset(X,V,sizeof(X));
#define MEMZERO(X) MEMSET(X,0)

#define PI  (3.1415927f)
#define PI2 (2*PI)

class noncopyable {
protected:
  INLINE noncopyable(void) {}
  INLINE ~noncopyable(void) {}
private:
  INLINE noncopyable(const noncopyable&) {}
  INLINE noncopyable& operator= (const noncopyable&) {return *this;}
};
template<class T> struct remove_reference      {typedef T type;};
template<class T> struct remove_reference<T&>  {typedef T type;};
template<class T> struct remove_reference<T&&> {typedef T type;};
template<class T> INLINE typename remove_reference<T>::type&& move(T&& t) {
  return static_cast<typename remove_reference<T>::type&&>(t);
}

/*-------------------------------------------------------------------------
 - memory debugging / tracking facilities
 -------------------------------------------------------------------------*/
void meminit(void);
void *memalloc(size_t sz, const char *filename, int linenum);
void *memrealloc(void *ptr, size_t sz, const char *filename, int linenum);
void memfree(void *);

template <typename T, typename... Args>
INLINE T *memconstructa(s32 n, const char *filename, int linenum, Args&&... args) {
  void *ptr = (void*) memalloc(n*sizeof(T)+DEFAULT_ALIGNMENT, filename, linenum);
  *(s32*)ptr = n;
  T *array = (T*)((char*)ptr+DEFAULT_ALIGNMENT);
  loopi(n) new (array+i) T(args...);
  return array;
}
template <typename T, typename... Args>
INLINE T *memconstruct(const char *filename, int linenum, Args&&... args) {
  T *ptr = (T*) memalloc(sizeof(T), filename, linenum);
  new (ptr) T(args...);
  return ptr;
}
template <typename T> INLINE void memdestroy(T *ptr) {
  ptr->~T();
  memfree(ptr);
}
template <typename T> INLINE void memdestroya(T *array) {
  s32 *ptr = (s32*) ((char*)array-DEFAULT_ALIGNMENT);
  loopi(*ptr) array[i].~T();
  memfree(ptr);
}

#define MALLOC(SZ) cube::memalloc(SZ, __FILE__, __LINE__)
#define REALLOC(PTR, SZ) cube::memrealloc(PTR, SZ, __FILE__, __LINE__)
#define FREE(PTR) cube::memfree(PTR)
#define NEWE(X) memconstruct<X>(__FILE__,__LINE__)
#define NEWAE(X,N) memconstructa<X>(N,__FILE__,__LINE__)
#define NEW(X,...) memconstruct<X>(__FILE__,__LINE__,__VA_ARGS__)
#define NEWA(X,N,...) memconstructa<X>(N,__FILE__,__LINE__,__VA_ARGS__)
#define SAFE_DELETE(X) do { if (X) memdestroy(X); X = NULL; } while (0)
#define SAFE_DELETEA(X) do { if (X) memdestroya(X); X = NULL; } while (0)

void fatal(const char *s, const char *o = "");
void keyrepeat(bool on);

static const int KB = 1024;
static const int MB = KB*KB;

} // namespace cube

