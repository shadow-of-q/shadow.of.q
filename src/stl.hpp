#pragma once
#include "math.hpp"
#include "tools.hpp"
#include <cstring>
#include <cstdarg>

namespace cube {

/*-------------------------------------------------------------------------
 - algorithms, helper classes and functions
 -------------------------------------------------------------------------*/
template<class T> INLINE float heapscore(const T &n) { return n; }
template<class T> INLINE bool compareless(const T &x, const T &y) { return x < y; }
static struct niltype {} nil MAYBE_UNUSED;
static struct infype {} inf MAYBE_UNUSED;
static struct neginfype {} neginf MAYBE_UNUSED;

#ifdef swap
#undef swap
#endif
template<class T> INLINE void swap(T &a, T &b) {
  T t = a;
  a = b;
  b = t;
}

template<class T, class F>
INLINE void insertionsort(T *start, T *end, F fun) {
  for(T *i = start+1; i < end; i++) {
    if(fun(*i, i[-1])) {
      T tmp = *i;
      *i = i[-1];
      T *j = i-1;
      for(; j > start && fun(tmp, j[-1]); --j)
        *j = j[-1];
      *j = tmp;
    }
  }
}

template<class T, class F>
INLINE void insertionsort(T *buf, int n, F fun) {
  insertionsort(buf, buf+n, fun);
}

template<class T>
INLINE void insertionsort(T *buf, int n) {
  insertionsort(buf, buf+n, compareless<T>);
}

template<class T, class F> void quicksort(T *start, T *end, F fun) {
  while(end-start > 10) {
    T *mid = &start[(end-start)/2], *i = start+1, *j = end-2, pivot;
    if(fun(*start, *mid)) { // start < mid 
      if(fun(end[-1], *start)) { pivot = *start; *start = end[-1]; end[-1] = *mid; } // end < start < mid 
      else if(fun(end[-1], *mid)) { pivot = end[-1]; end[-1] = *mid; } // start <= end < mid 
      else { pivot = *mid; } // start < mid <= end
    }
    else if(fun(*start, end[-1])) { pivot = *start; *start = *mid; } // mid <= start < end 
    else if(fun(*mid, end[-1])) { pivot = end[-1]; end[-1] = *start; *start = *mid; } //  mid < end <= start 
    else { pivot = *mid; swap(*start, end[-1]); }  // end <= mid <= start 
    *mid = end[-2];
    do {
      while(fun(*i, pivot)) if(++i >= j) goto partitioned;
      while(fun(pivot, *--j)) if(i >= j) goto partitioned;
      swap(*i, *j);
    } while(++i < j);
partitioned:
    end[-2] = *i;
    *i = pivot;

    if(i-start < end-(i+1)) {
      quicksort(start, i, fun);
      start = i+1;
    } else {
      quicksort(i+1, end, fun);
      end = i;
    }
  }

  insertionsort(start, end, fun);
}

template<class T, class F> INLINE void quicksort(T *buf, int n, F fun) {
  quicksort(buf, buf+n, fun);
}

template<class T> INLINE void quicksort(T *buf, int n) {
  quicksort(buf, buf+n, compareless<T>);
}

template<class T> struct isclass {
  template<class C> static char test(void (C::*)(void));
  template<class C> static int test(...);
  enum { yes = sizeof(test<T>(0)) == 1 ? 1 : 0, no = yes^1 };
};

/*-------------------------------------------------------------------------
 - easy safe strings
 -------------------------------------------------------------------------*/
#define _MAXDEFSTR 260
typedef char string[_MAXDEFSTR];

INLINE void strn0cpy(char *d, const char *s, size_t m) { strncpy(d,s,m); d[(m)-1] = 0; }
INLINE void strcpy_s(char *d, const char *s) { strn0cpy(d,s,_MAXDEFSTR); }
INLINE void strcat_s(char *d, const char *s) { size_t n = strlen(d); strn0cpy(d+n,s,_MAXDEFSTR-n); }
void formatstring(char *d, const char *fmt, va_list v);

struct sprintf_s_f {
  INLINE sprintf_s_f(char *str): d(str) {};
  void operator()(const char* fmt, ...);
  char *d;
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

char *path(char *s);
char *loadfile(char *fn, int *size);
void endianswap(void *, int, int);
int islittleendian(void);
void initendiancheck(void);
void writebmp(const int *data, int w, int h, const char *name);

/*-------------------------------------------------------------------------
 - data buffer
 -------------------------------------------------------------------------*/
template <class T> struct databuf {
  enum {
    OVERREAD  = 1<<0,
    OVERWROTE = 1<<1
  };

  T *buf;
  s32 len, maxlen;
  u8 flags;

  databuf(void) : buf(NULL), len(0), maxlen(0), flags(0) {}

  template<class U>
    databuf(T *buf, U maxlen) : buf(buf), len(0), maxlen((int)maxlen), flags(0) {}

  const T &get(void) {
    static T overreadval = 0;
    if (len<maxlen) return buf[len++];
    flags |= OVERREAD;
    return overreadval;
  }

  databuf subbuf(int sz) {
    sz = min(sz, maxlen-len);
    len += sz;
    return databuf(&buf[len-sz], sz);
  }

  void put(const T &val) {
    if (len<maxlen) buf[len++] = val;
    else flags |= OVERWROTE;
  }

  void put(const T *vals, int numvals) {
    if (maxlen-len<numvals) flags |= OVERWROTE;
    memcpy(&buf[len], (const void *)vals, min(maxlen-len, numvals)*sizeof(T));
    len += min(maxlen-len, numvals);
  }

  int get(T *vals, int numvals) {
    int read = min(maxlen-len, numvals);
    if (read<numvals) flags |= OVERREAD;
    memcpy(vals, (void *)&buf[len], read*sizeof(T));
    len += read;
    return read;
  }

  void offset(int n) {
    n = min(n, maxlen);
    buf += n;
    maxlen -= n;
    len = max(len-n, 0);
  }

  bool empty() const { return len==0; }
  int length() const { return len; }
  int remaining() const { return maxlen-len; }
  bool overread() const { return (flags&OVERREAD)!=0; }
  bool overwrote() const { return (flags&OVERWROTE)!=0; }

  void forceoverread(void) {
    len = maxlen;
    flags |= OVERREAD;
  }
};

typedef databuf<char> charbuf;
typedef databuf<u8> u8buf;

/*-------------------------------------------------------------------------
 - resizeable vector
 -------------------------------------------------------------------------*/
template <class T> struct vector {
  static const int MINSIZE = 8;
  T *buf;
  s32 alen, ulen;

  INLINE vector(void) : buf(NULL), alen(0), ulen(0) { }
  INLINE vector(const vector &v) : buf(NULL), alen(0), ulen(0) { *this = v; }
  INLINE ~vector(void) {
    shrink(0);
    auto u8buf = (u8*)buf;
    SAFE_DELETEA(u8buf);
  }
  INLINE vector(u32 sz) : buf(NULL), alen(0), ulen(0) { pad(sz); }

  vector<T> &operator=(const vector<T> &v) {
    shrink(0);
    if (v.length() > alen) growbuf(v.length());
    loopv(v) add(v[i]);
    return *this;
  }
  T &add(const T &x) {
    if (ulen==alen) growbuf(ulen+1);
    new (&buf[ulen]) T(x);
    return buf[ulen++];
  }
  T &add(void) {
    if (ulen==alen) growbuf(ulen+1);
    new (&buf[ulen]) T;
    return buf[ulen++];
  }
  T &dup(void) {
    if (ulen==alen) growbuf(ulen+1);
    new (&buf[ulen]) T(buf[ulen-1]);
    return buf[ulen++];
  }
  void move(vector<T> &v) {
    if (!ulen) {
      swap(buf, v.buf);
      swap(ulen, v.ulen);
      swap(alen, v.alen);
    } else {
      growbuf(ulen+v.ulen);
      if (v.ulen) memcpy(&buf[ulen], v.buf, v.ulen*sizeof(T));
      ulen += v.ulen;
      v.ulen = 0;
    }
  }

  INLINE bool inrange(size_t i) const { return i<size_t(ulen); }
  INLINE bool inrange(int i) const { return i>=0 && i<ulen; }

  INLINE T &pop(void) { return buf[--ulen]; }
  INLINE T &last(void) { return buf[ulen-1]; }
  INLINE bool empty(void) const { return ulen==0; }
  void drop(void) { ulen--; buf[ulen].~T(); }

  INLINE int capacity(void) const { return alen; }
  INLINE int length(void) const { return ulen; }
  INLINE T &operator[](int i) { ASSERT(i>=0 && i<ulen); return buf[i]; }
  INLINE const T &operator[](int i) const { ASSERT(i >= 0 && i<ulen); return buf[i]; }
  INLINE void disown(void) { buf = NULL; alen = ulen = 0; }
  INLINE void shrink(int i) { ASSERT(i<=ulen); if (isclass<T>::no) ulen = i; else while (ulen>i) drop(); }
  INLINE void setsize(int i) { ASSERT(i<=ulen); ulen = i; }
  INLINE T *getbuf(void) { return buf; }
  INLINE const T *getbuf(void) const { return buf; }
  INLINE bool inbuf(const T *e) const { return e >= buf && e < &buf[ulen]; }

  template<class F> void sort(F fun, int i = 0, int n = -1) {
    quicksort(&buf[i], n < 0 ? ulen-i : n, fun);
  }

  void sort(void) { sort(compareless<T>); }

  void growbuf(int sz) {
    int olen = alen;
    if (!alen)
      alen = max(MINSIZE, sz);
    else
      while (alen < sz) alen *= 2;
    if (alen <= olen) return;
    auto newbuf = NEWAE(u8,alen*sizeof(T));
    if (olen > 0) {
      memcpy(newbuf, buf, olen*sizeof(T));
      auto u8buf = (u8*) buf;
      SAFE_DELETEA(u8buf);
    }
    buf = (T*) newbuf;
  }

  databuf<T> reserve(int sz) {
    if (ulen+sz > alen) growbuf(ulen+sz);
    return databuf<T>(&buf[ulen], sz);
  }

  INLINE void advance(int sz) { ulen += sz; }
  INLINE void addbuf(const databuf<T> &p) { advance(p.length()); }

  T *pad(int n) {
    T *buf = reserve(n).buf;
    advance(n);
    return buf;
  }

  INLINE void put(const T &v) { add(v); }
  void put(const T *v, int n) {
    databuf<T> buf = reserve(n);
    buf.put(v, n);
    addbuf(buf);
  }

  void remove(int i, int n) {
    for (int p = i+n; p<ulen; p++) buf[p-n] = buf[p];
    ulen -= n;
  }

  T remove(int i) {
    T e = buf[i];
    for (int p = i+1; p<ulen; p++) buf[p-1] = buf[p];
    ulen--;
    return e;
  }

  T removeunordered(int i) {
    T e = buf[i];
    ulen--;
    if (ulen>0) buf[i] = buf[ulen];
    return e;
  }

  template<class U> int find(const U &o) {
    loopi(ulen) if (buf[i]==o) return i;
    return -1;
  }

  void removeobj(const T &o) {
    loopi(ulen) if (buf[i]==o) remove(i--);
  }

  void replacewithlast(const T &o) {
    if (!ulen) return;
    loopi(ulen-1) if (buf[i]==o) buf[i] = buf[ulen-1];
    ulen--;
  }

  T &insert(int i, const T &e) {
    add(T());
    for (int p = ulen-1; p>i; p--) buf[p] = buf[p-1];
    buf[i] = e;
    return buf[i];
  }

  T *insert(int i, const T *e, int n) {
    if (ulen+n>alen) growbuf(ulen+n);
    loopj(n) add(T());
    for (int p = ulen-1; p>=i+n; p--) buf[p] = buf[p-n];
    loopj(n) buf[i+j] = e[j];
    return &buf[i];
  }

  void reverse(void) {
    loopi(ulen/2) swap(buf[i], buf[ulen-1-i]);
  }

  static int heapparent(int i) { return (i - 1) >> 1; }
  static int heapchild(int i) { return (i << 1) + 1; }

  void buildheap(void) { for (int i = ulen/2; i >= 0; i--) downheap(i); }

  int upheap(int i) {
    float score = heapscore(buf[i]);
    while (i > 0) {
      int pi = heapparent(i);
      if (score >= heapscore(buf[pi])) break;
      swap(buf[i], buf[pi]);
      i = pi;
    }
    return i;
  }

  T &addheap(const T &x) {
    add(x);
    return buf[upheap(ulen-1)];
  }

  int downheap(int i) {
    float score = heapscore(buf[i]);
    for (;;) {
      int ci = heapchild(i);
      if (ci >= ulen) break;
      float cscore = heapscore(buf[ci]);
      if (score > cscore) {
        if (ci+1 < ulen && heapscore(buf[ci+1]) < cscore) {
          swap(buf[ci+1], buf[i]);
          i = ci+1;
        } else {
          swap(buf[ci], buf[i]);
          i = ci;
        }
      } else if (ci+1 < ulen && heapscore(buf[ci+1]) < score) {
        swap(buf[ci+1], buf[i]);
        i = ci+1;
      } else
        break;
    }
    return i;
  }

  T removeheap(void) {
    T e = removeunordered(0);
    if (ulen) downheap(0);
    return e;
  }

  template<class K> int htfind(const K &key) {
    loopi(ulen) if (htcmp(key, buf[i])) return i;
    return -1;
  }
};
typedef vector<char *> cvector;
typedef vector<int> ivector;

/*-------------------------------------------------------------------------
 - hash map
 -------------------------------------------------------------------------*/
#define ENUMERATE(HT,E,B) loopi((HT)->size)\
  for ((HT)->enumc = (HT)->table[i]; (HT)->enumc; (HT)->enumc = (HT)->enumc->next) {\
    auto E = &(HT)->enumc->data; B;\
  }

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
    for (int i = 0, k; (k = key[i]) != 0; i++) h = ((h<<5)+h)^k; // bernstein k=33 xor
    h = h&(size-1); // primes not much of an advantage
    for (chain *c = table[h]; c; c = c->next) {
      char ch = 0;
      for (const char *p1 = key, *p2 = c->key; (ch = *p1++)==*p2++; ) if (!ch) {
        T *d = &c->data;
        if (data) c->data = *data;
        return d;
      }
    }
    if (data) {
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

/*-------------------------------------------------------------------------
 - intrusive list
 -------------------------------------------------------------------------*/
struct intrusive_list_node {
  INLINE intrusive_list_node(void) { next = prev = this; }
  INLINE bool in_list(void) const  { return this != next; }
  intrusive_list_node *next;
  intrusive_list_node *prev;
};
void append(intrusive_list_node *node, intrusive_list_node *prev);
void prepend(intrusive_list_node *node, intrusive_list_node *next);
void link(intrusive_list_node* node, intrusive_list_node* nextNode);
void unlink(intrusive_list_node* node);

template<typename pointer, typename reference>
struct intrusive_list_iterator {
  INLINE intrusive_list_iterator(void): m_node(0) {}
  INLINE intrusive_list_iterator(pointer iterNode) : m_node(iterNode) {}
  INLINE reference operator*(void) const {
    GBE_ASSERT(m_node);
    return *m_node;
  }
  INLINE pointer operator->(void) const { return m_node; }
  INLINE pointer node(void) const { return m_node; }
  INLINE intrusive_list_iterator& operator++(void) {
    m_node = static_cast<pointer>(m_node->next);
    return *this;
  }
  INLINE intrusive_list_iterator& operator--(void) {
    m_node = static_cast<pointer>(m_node->prev);
    return *this;
  }
  INLINE intrusive_list_iterator operator++(int) {
    intrusive_list_iterator copy(*this);
    ++(*this);
    return copy;
  }
  INLINE intrusive_list_iterator operator--(int) {
    intrusive_list_iterator copy(*this);
    --(*this);
    return copy;
  }
  INLINE bool operator== (const intrusive_list_iterator& rhs) const {
    return rhs.m_node == m_node;
  }
  INLINE bool operator!= (const intrusive_list_iterator& rhs) const {
    return !(rhs == *this);
  }
private:
  pointer m_node;
};

struct intrusive_list_base {
public:
  typedef size_t size_type;
  INLINE void pop_back(void) { unlink(m_root.prev); }
  INLINE void pop_front(void) { unlink(m_root.next); }
  INLINE bool empty(void) const  { return !m_root.in_list(); }
  size_type size(void) const;
protected:
  intrusive_list_base(void);
  INLINE ~intrusive_list_base(void) {}
  intrusive_list_node m_root;
private:
  intrusive_list_base(const intrusive_list_base&);
  intrusive_list_base& operator=(const intrusive_list_base&);
};

template<class T> struct intrusive_list : public intrusive_list_base {
  typedef T node_type;
  typedef T value_type;
  typedef intrusive_list_iterator<T*, T&> iterator;
  typedef intrusive_list_iterator<const T*, const T&> const_iterator;

  intrusive_list(void) : intrusive_list_base() {
    intrusive_list_node* testNode((T*)0);
    static_cast<void>(sizeof(testNode));
  }

  void push_back(value_type* v) { link(v, &m_root); }
  void push_front(value_type* v) { link(v, m_root.next); }

  iterator begin(void)  { return iterator(upcast(m_root.next)); }
  iterator end(void)    { return iterator(upcast(&m_root)); }
  iterator rbegin(void) { return iterator(upcast(m_root.prev)); }
  iterator rend(void)   { return iterator(upcast(&m_root)); }
  const_iterator begin(void) const  { return const_iterator(upcast(m_root.next)); }
  const_iterator end(void) const    { return const_iterator(upcast(&m_root)); }
  const_iterator rbegin(void) const { return const_iterator(upcast(m_root.prev)); }
  const_iterator rend(void) const   { return const_iterator(upcast(&m_root)); }

  INLINE value_type *front(void) { return upcast(m_root.next); }
  INLINE value_type *back(void)  { return upcast(m_root.prev); }
  INLINE const value_type *front(void) const { return upcast(m_root.next); }
  INLINE const value_type *back(void) const  { return upcast(m_root.prev); }

  iterator insert(iterator pos, value_type* v) {
    link(v, pos.node());
    return iterator(v);
  }
  iterator erase(iterator it) {
    iterator it_erase(it);
    ++it;
    unlink(it_erase.node());
    return it;
  }
  iterator erase(iterator first, iterator last) {
    while (first != last) first = erase(first);
    return first;
  }

  INLINE void clear(void) { erase(begin(), end()); }
  INLINE void fastclear(void) { m_root.next = m_root.prev = &m_root; }
  static void remove(value_type* v) { unlink(v); }
private:
  static INLINE node_type* upcast(intrusive_list_node* n) {
    return static_cast<node_type*>(n);
  }
  static INLINE const node_type* upcast(const intrusive_list_node* n) {
    return static_cast<const node_type*>(n);
  }
};

/*-------------------------------------------------------------------------
 - reference counted object
 -------------------------------------------------------------------------*/
struct refcount {
  INLINE refcount(void) : refcounter(0) {}
  virtual ~refcount(void) {}
  INLINE void refinc(void) { refcounter++; }
  INLINE bool refdec(void) { return !(--refcounter); }
  atomic refcounter;
};

template<typename T> struct ref {
  INLINE ref(void) : ptr(NULL) {}
  INLINE ref(const ref& input) : ptr(input.ptr) { if (ptr) ptr->refinc(); }
  INLINE ref(T* const input) : ptr(input) { if (ptr) ptr->refinc(); }
  INLINE ~ref(void) { if (ptr && ptr->refdec()) SAFE_DELETE(ptr); }
  INLINE operator bool(void) const       { return ptr != NULL; }
  INLINE operator T* (void)  const       { return ptr; }
  INLINE const T& operator* (void) const { return *ptr ;}
  INLINE const T* operator->(void) const { return  ptr;}
  INLINE T& operator* (void) {return *ptr;}
  INLINE T* operator->(void) {return  ptr;}
  INLINE ref &operator= (const ref &input);
  INLINE ref &operator= (niltype);
  template<typename U> INLINE ref<U> cast(void) { return ref<U>((U*)(ptr)); }
  template<typename U> INLINE const ref<U> cast(void) const { return ref<U>((U*)(ptr)); }
  T* const ptr;
};

template <typename T>
INLINE ref<T> &ref<T>::operator= (const ref<T> &input) {
  if (input.ptr) input.ptr->refInc();
  if (ptr && ptr->refDec()) SAFE_DELETE(ptr);
  *(T**)&ptr = input.ptr;
  return *this;
}
template <typename T>
INLINE ref<T> &ref<T>::operator= (niltype) {
  if (ptr && ptr->refDec()) SAFE_DELETE(ptr);
  *(T**)&ptr = NULL;
  return *this;
}

} // namespace cube

