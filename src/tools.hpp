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
#define DEBUGBREAK    __debugbreak()
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
#if defined(DELETE)
#undef DELETE
#endif
#endif
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

// easy safe strings
char *path(char *s);
char *loadfile(char *fn, int *size);
void endianswap(void *, int, int);
int islittleendian(void);
void initendiancheck(void);
void writebmp(const int *data, int w, int h, const char *name);

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

/*-------------------------------------------------------------------------
 - simple collections
 -------------------------------------------------------------------------*/

// easy safe strings
#define _MAXDEFSTR 260
typedef char string[_MAXDEFSTR];

INLINE void strn0cpy(char *d, const char *s, size_t m) { strncpy(d,s,m); d[(m)-1] = 0; }
INLINE void strcpy_s(char *d, const char *s) { strn0cpy(d,s,_MAXDEFSTR); }
INLINE void strcat_s(char *d, const char *s) { size_t n = strlen(d); strn0cpy(d+n,s,_MAXDEFSTR-n); }
INLINE void formatstring(char *d, const char *fmt, va_list v) {
  _vsnprintf(d, _MAXDEFSTR, fmt, v);
  d[_MAXDEFSTR-1] = 0;
}

struct sprintf_s_f {
  char *d;
  sprintf_s_f(char *str): d(str) {};
  void operator()(const char* fmt, ...) {
    va_list v;
    va_start(v, fmt);
    _vsnprintf(d, _MAXDEFSTR, fmt, v);
    va_end(v);
    d[_MAXDEFSTR-1] = 0;
  }
};

#define sprintf_s(d) sprintf_s_f((char *)d)
#define sprintf_sd(d) string d; sprintf_s(d)
#define sprintf_sdlv(d,last,fmt) string d; { va_list ap; va_start(ap, last); formatstring(d, fmt, ap); va_end(ap); }
#define sprintf_sdv(d,fmt) sprintf_sdlv(d,fmt,fmt)
#define ATOI(s) strtol(s, NULL, 0) // supports hexadecimal numbers

char *newstring(const char *s, const char *filename, int linenum);
char *newstring(const char *s, size_t sz, const char *filename, int linenum);
char *newstringbuf(const char *s, const char *filename, int linenum);

#define NEWSTRING(...) newstring(__VA_ARGS__, __FILE__, __LINE__)
#define NEWSTRINGBUF(S) newstringbuf(S, __FILE__, __LINE__)

// growable vector
template <class T> struct vector : noncopyable {
  T *buf;
  int alen;
  int ulen;

  INLINE vector(u32 size) {
    ulen = alen = size;
    buf = (T*) MALLOC(alen*sizeof(T));
    loopi(ulen) buf[i]=T();
  }
  INLINE vector(void) {
    alen = 8;
    buf = (T*) MALLOC(alen*sizeof(T));
    ulen = 0;
  }
  INLINE vector(vector &&other) {
    this->buf = other.buf;
    this->alen = other.alen;
    this->ulen = other.ulen;
    this->p = other.p;
  }
  ~vector(void) {
    setsize(0);
    FREE(buf);
    buf = NULL;
    ulen = alen = 0;
  }
  INLINE T &add(const T &x) {
    if(ulen==alen) this->realloc();
    new (&buf[ulen]) T(x);
    return buf[ulen++];
  }
  INLINE T &add(void) {
    if(ulen==alen) this->realloc();
    new (&buf[ulen]) T;
    return buf[ulen++];
  }
  INLINE T &pop(void) { return buf[--ulen]; }
  INLINE T &last(void) { return buf[ulen-1]; }
  INLINE bool empty(void) { return ulen==0; }
  INLINE int length(void) const { return ulen; }
  INLINE int size(void) const { return ulen*sizeof(T); }
  INLINE const T &operator[](int i) const { assert(i>=0 && i<ulen); return buf[i]; }
  INLINE T &operator[](int i) { assert(i>=0 && i<ulen); return buf[i]; }
  INLINE T *getbuf(void) { return buf; }
  void setsize(int i) { for(; ulen>i; ulen--) buf[ulen-1].~T(); }
  void clear(void) { setsize(0); }
  void sort(void *cf) {
    qsort(buf, ulen, sizeof(T), (int (__cdecl *)(const void *,const void *))cf);
  }
  void realloc(void) {
    buf = (T*) REALLOC(buf, (alen *= 2)*sizeof(T));
  }
  T remove(int i) {
    T e = buf[i];
    for(int p = i+1; p<ulen; p++) buf[p-1] = buf[p];
    ulen--;
    return e;
  }
  T &insert(int i, const T &e) {
    add(T());
    for(int p = ulen-1; p>i; p--) buf[p] = buf[p-1];
    buf[i] = e;
    return buf[i];
  }
};

#define ENUMERATE(HT,E,B) loopi((HT)->size)\
  for((HT)->enumc = (HT)->table[i]; (HT)->enumc; (HT)->enumc = (HT)->enumc->next) {\
    auto E = &(HT)->enumc->data; B;\
  }

// hash map
template <class T> struct hashtable {
  struct chain { chain *next; const char *key; T data; };
  int size;
  int numelems;
  chain **table;
  chain *enumc;

  hashtable(void) : size(1<<10), numelems(0) {
    table = (chain**) MALLOC(size*sizeof(chain*));
    loopi(size) table[i] = NULL;
  }
  ~hashtable(void) {
    ENUMERATE(this,e,e->~T());
    loopi(size)
      if (table[i]) for (auto c = table[i], next = (chain*) NULL; c; c = next) {
        next = c->next;
        FREE(c);
      }
    FREE(table);
  }
  hashtable(hashtable<T> &v);
  void operator=(hashtable<T> &v);

  T *access(const char *key, const T *data = NULL) {
    unsigned int h = 5381;
    for(int i = 0, k; (k = key[i]) != 0; i++) h = ((h<<5)+h)^k; // bernstein k=33 xor
    h = h&(size-1); // primes not much of an advantage
    for(chain *c = table[h]; c; c = c->next) {
      char ch = 0;
      for(const char *p1 = key, *p2 = c->key; (ch = *p1++)==*p2++; ) if(!ch) {
        T *d = &c->data;
        if(data) c->data = *data;
        return d;
      }
    }
    if(data) {
      chain *c = (chain*) MALLOC(sizeof(chain));
      c->data = *data;
      c->key = key;
      c->next = table[h];
      table[h] = c;
      numelems++;
    }
    return NULL;
  }
};

// vertex array format
struct vertex { float u, v, x, y, z; uchar r, g, b, a; };
typedef vector<char *> cvector;
typedef vector<int> ivector;

void fatal(const char *s, const char *o = "");
void keyrepeat(bool on);

static const int KB = 1024;
static const int MB = KB*KB;

} // namespace cube

