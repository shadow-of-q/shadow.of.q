#pragma once
#include <cassert>
#include <cstdlib>
#include <new>

/*-------------------------------------------------------------------------
 - cpu architecture
 -------------------------------------------------------------------------*/

// detect 32 or 64 platform
#if defined(__x86_64__) || defined(__ia64__) || defined(_M_X64)
#define __X86_64__
#elif !defined(EMSCRIPTEN)
#define __X86__
#endif

/*-------------------------------------------------------------------------
 - operating system
 -------------------------------------------------------------------------*/

// detect linux platform
#if defined(linux) || defined(__linux__) || defined(__LINUX__)
#  if !defined(__LINUX__)
#     define __LINUX__
#  endif
#  if !defined(__UNIX__)
#     define __UNIX__
#  endif
#endif

// detect freebsd platform
#if defined(__FreeBSD__) || defined(__FREEBSD__)
#  if !defined(__FREEBSD__)
#     define __FREEBSD__
#  endif
#  if !defined(__UNIX__)
#     define __UNIX__
#  endif
#endif

// detect windows 95/98/nt/2000/xp/vista/7 platform
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)) && !defined(__CYGWIN__)
#  if !defined(__WIN32__)
#     define __WIN32__
#  endif
#endif

// detect cygwin platform
#if defined(__CYGWIN__)
#  if !defined(__UNIX__)
#     define __UNIX__
#  endif
#endif

// detect mac os x platform
#if defined(__APPLE__) || defined(MACOSX) || defined(__MACOSX__)
#  if !defined(__MACOSX__)
#     define __MACOSX__
#  endif
#  if !defined(__UNIX__)
#     define __UNIX__
#  endif
#endif

// detecting emscripten means that we are running javascript
#if defined(EMSCRIPTEN)
#  define __JAVASCRIPT__
#endif

// try to detect other Unix systems
#if defined(__unix__) || defined (unix) || defined(__unix) || defined(_unix)
#  if !defined(__UNIX__)
#     define __UNIX__
#  endif
#endif

/*-------------------------------------------------------------------------
 - compiler
 -------------------------------------------------------------------------*/

// gcc compiler
#ifdef __GNUC__
// #define __GNUC__
#endif

// intel compiler
#ifdef __INTEL_COMPILER
#define __ICC__
#endif

// visual compiler
#ifdef _MSC_VER
#define __MSVC__
#endif

// clang compiler
#ifdef __clang__
#define __CLANG__
#endif

//emscripten compiler
#if defined(EMSCRIPTEN)
#define __EMSCRIPTEN__
#endif

/*-------------------------------------------------------------------------
 - macros / extra includes
 -------------------------------------------------------------------------*/

#ifdef __MSVC__
#include <intrin.h>
#undef NOINLINE
#define NOINLINE        __declspec(noinline)
#define INLINE          __forceinline
#define RESTRICT        __restrict
#define THREAD          __declspec(thread)
#define ALIGNED(...)    __declspec(align(__VA_ARGS__))
//#define __FUNCTION__  __FUNCTION__
#define DEBUGBREAK      __debugbreak()
#define COMPILER_WRITE_BARRIER       _WriteBarrier()
#define COMPILER_READ_WRITE_BARRIER  _ReadWriteBarrier()
#define MAYALIAS
#if _MSC_VER >= 1400
#pragma intrinsic(_ReadBarrier)
#define COMPILER_READ_BARRIER        _ReadBarrier()
#else
#define COMPILER_READ_BARRIER        _ReadWriteBarrier()
#endif // _MSC_VER >= 1400
#define alloca _alloca
#else // __MSVC__
#undef NOINLINE
#undef INLINE
#define NOINLINE        __attribute__((noinline))
#define INLINE          inline __attribute__((always_inline))
#define RESTRICT        __restrict
#define THREAD          __thread
#define ALIGNED(...)    __attribute__((aligned(__VA_ARGS__)))
#define __FUNCTION__    __PRETTY_FUNCTION__
#define DEBUGBREAK      asm ("int $3")
#define COMPILER_READ_WRITE_BARRIER asm volatile("" ::: "memory");
#define COMPILER_WRITE_BARRIER COMPILER_READ_WRITE_BARRIER
#define COMPILER_READ_BARRIER COMPILER_READ_WRITE_BARRIER
#define MAYALIAS __attribute__((__may_alias__))
#endif // __MSVC__

#if defined(__SSE__)
#include <xmmintrin.h>
#define IF_SSE(X) X
#else
#define IF_SSE(X)
#endif
#if defined(__SSE2__)
#include <emmintrin.h>
#define IF_SSE2(X) X
#else
#define IF_SSE2(X)
#endif

// alignment rules
#define DEFAULT_ALIGNMENT 8
#define DEFAULT_ALIGNED ALIGNED(DEFAULT_ALIGNMENT)
#define CACHE_LINE_ALIGNMENT 64
#define CACHE_LINE_ALIGNED ALIGNED(CACHE_LINE_ALIGNMENT)

// concatenation
#define JOIN(X, Y) _DO_JOIN(X, Y)
#define _DO_JOIN(X, Y) _DO_JOIN2(X, Y)
#define _DO_JOIN2(X, Y) X##Y

// align X on A
#define ALIGN(X,A) (((X) % (A)) ? ((X) + (A) - ((X) % (A))) : (X))

#define OFFSETOF(STR,F) (uintptr(&((STR*)0)->F))
#ifdef __GNUC__
  #define MAYBE_UNUSED __attribute__((used))
#else
  #define MAYBE_UNUSED
#endif

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
#define ASSERT assert

#if defined(__WIN32__)
#define PATHDIV '\\'
#else
#define __cdecl
#define _vsnprintf vsnprintf
#define PATHDIV '/'
#endif

#if defined(__MSVC__)
#include <malloc.h>
#endif

namespace cube {

/*-------------------------------------------------------------------------
 - Standard types
 -------------------------------------------------------------------------*/
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
#endif // __MSVC__
template <u32 sz> struct ptrtype {};
template <> struct ptrtype<4> { typedef u32 value; };
template <> struct ptrtype<8> { typedef u64 value; };
typedef typename ptrtype<sizeof(void*)>::value uintptr;

typedef void *filehandle;

/*-------------------------------------------------------------------------
 - Various useful macros, helper classes or functions
 -------------------------------------------------------------------------*/
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

/*-------------------------------------------------------------------------
 - atomics / barriers / locks
 -------------------------------------------------------------------------*/
#if defined(__MSVC__)
INLINE s32 atomic_add(volatile s32* m, const s32 v) {
  return _InterlockedExchangeAdd((volatile long*)m,v);
}
INLINE s32 atomic_cmpxchg(volatile s32* m, const s32 v, const s32 c) {
  return _InterlockedCompareExchange((volatile long*)m,v,c);
}
#elif defined(__JAVASCRIPT__)
INLINE s32 atomic_add(s32 volatile* value, s32 input) {
  const s32 initial = value;
  *value += input;
  return initial;
}
INLINE s32 atomic_cmpxchg(volatile s32* m, const s32 v, const s32 c) {
  const s32 initial = *m;
  if (*m == c) *m = v;
  return initial;
}
#else
INLINE s32 atomic_add(s32 volatile* value, s32 input) {
  asm volatile("lock xadd %0,%1" : "+r"(input), "+m"(*value) : "r"(input), "m"(*value));
  return input;
}

INLINE s32 atomic_cmpxchg(s32 volatile* value, const s32 input, s32 comparand) {
  asm volatile("lock cmpxchg %2,%0" : "=m"(*value), "=a"(comparand) : "r"(input), "m"(*value), "a"(comparand) : "flags");
  return comparand;
}
#endif // __MSVC__

#if defined(__X86__) || defined(__X86_64__) || defined(__JAVASCRIPT__)
template <typename T>
INLINE T loadacquire(volatile T *ptr) {
  COMPILER_READ_WRITE_BARRIER;
  T x = *ptr;
  COMPILER_READ_WRITE_BARRIER;
  return x;
}
template <typename T>
INLINE void storerelease(volatile T *ptr, T x) {
  COMPILER_READ_WRITE_BARRIER;
  *ptr = x;
  COMPILER_READ_WRITE_BARRIER;
}
#else
#error "unknown platform"
#endif

struct atomic : noncopyable {
public:
  INLINE atomic(void) {}
  INLINE atomic(s32 data) : data(data) {}
  INLINE atomic& operator =(const s32 input) { data = input; return *this; }
  INLINE operator s32(void) const { return data; }
  INLINE friend void storerelease(atomic &value, s32 x) { storerelease(&value.data, x); }
  INLINE friend s32 operator+=   (atomic &value, s32 input) { return atomic_add(&value.data, input) + input; }
  INLINE friend s32 operator++   (atomic &value) { return atomic_add(&value.data,  1) + 1; }
  INLINE friend s32 operator--   (atomic &value) { return atomic_add(&value.data, -1) - 1; }
  INLINE friend s32 operator++   (atomic &value, s32) { return atomic_add(&value.data,  1); }
  INLINE friend s32 operator--   (atomic &value, s32) { return atomic_add(&value.data, -1); }
  INLINE friend s32 cmpxchg      (atomic &value, s32 v, s32 c) { return atomic_cmpxchg(&value.data,v,c); }
private:
  volatile s32 data;
};
} // namespace cube

