#pragma once
#include "math.hpp"
#include "tools.hpp"
#include <cstring>
#include <cstdarg>
#define STL_DEBUG 0

namespace cube {

/*-------------------------------------------------------------------------
 - type traits
 -------------------------------------------------------------------------*/
template<typename T> struct is_integral { enum { value = false }; };
template<typename T> struct is_floating_point { enum { value = false }; };

#define MAKE_INTEGRAL(TYPE) template<> struct is_integral<TYPE> { enum { value = true }; }
MAKE_INTEGRAL(char);
MAKE_INTEGRAL(unsigned char);
MAKE_INTEGRAL(bool);
MAKE_INTEGRAL(short);
MAKE_INTEGRAL(unsigned short);
MAKE_INTEGRAL(int);
MAKE_INTEGRAL(unsigned int);
MAKE_INTEGRAL(long);
MAKE_INTEGRAL(unsigned long);
MAKE_INTEGRAL(wchar_t);
#undef MAKE_INTEGRAL

template<> struct is_floating_point<float> { enum { value = true }; };
template<> struct is_floating_point<double> { enum { value = true }; };
template<typename T> struct is_pointer { enum { value = false }; };
template<typename T> struct is_pointer<T*> { enum { value = true }; };
template<typename T> struct is_pod { enum { value = false }; };
template<typename T> struct is_fundamental {
  enum {
    value = is_integral<T>::value || is_floating_point<T>::value
  };
};
template<typename T> struct has_trivial_constructor {
  enum {
    value = is_fundamental<T>::value || is_pointer<T>::value || is_pod<T>::value
  };
};
template<typename T> struct has_trivial_copy {
  enum {
    value = is_fundamental<T>::value || is_pointer<T>::value || is_pod<T>::value
  };
};

template<typename T> struct has_trivial_assign {
  enum {
    value = is_fundamental<T>::value || is_pointer<T>::value || is_pod<T>::value
  };
};
template<typename T> struct has_trivial_destructor {
  enum {
    value = is_fundamental<T>::value || is_pointer<T>::value || is_pod<T>::value
  };
};
template<typename T> struct has_cheap_compare {
  enum {
    value = has_trivial_copy<T>::value && sizeof(T) <= 4
  };
};
template<class T> struct isclass {
  template<class C> static char test(void (C::*)(void));
  template<class C> static int test(...);
  enum { yes = sizeof(test<T>(0)) == 1 ? 1 : 0, no = yes^1 };
};

/*-------------------------------------------------------------------------
 - iterators
 -------------------------------------------------------------------------*/
struct input_iterator_tag {};
struct output_iterator_tag {};
struct forward_iterator_tag: public input_iterator_tag {};
struct bidirectional_iterator_tag: public forward_iterator_tag {};
struct random_access_iterator_tag: public bidirectional_iterator_tag {};

template<typename IterT> struct iterator_traits {
   typedef typename IterT::iterator_category iterator_category;
};
template<typename T> struct iterator_traits<T*> {
   typedef random_access_iterator_tag iterator_category;
};

namespace internal {
  template<typename TIter, typename TDist>
  INLINE void distance(TIter first, TIter last, TDist& dist, cube::random_access_iterator_tag) {
    dist = TDist(last - first);
  }
  template<typename TIter, typename TDist>
  INLINE void distance(TIter first, TIter last, TDist& dist, cube::input_iterator_tag) {
    dist = 0;
    while (first != last) {
      ++dist;
      ++first;
    }
  }
  template<typename TIter, typename TDist>
  INLINE void advance(TIter& iter, TDist d, cube::random_access_iterator_tag) {
    iter += d;
  }
  template<typename TIter, typename TDist>
  INLINE void advance(TIter& iter, TDist d, cube::bidirectional_iterator_tag)
  {
    if (d >= 0)
      while (d--) ++iter;
    else
      while (d++) --iter;
  }
  template<typename TIter, typename TDist>
  INLINE void advance(TIter& iter, TDist d, cube::input_iterator_tag) {
    ASSERT(d >= 0);
    while (d--) ++iter;
  }
} // namespace internal

/*-------------------------------------------------------------------------
 - allocator concept
 -------------------------------------------------------------------------*/
class allocator {
public:
  explicit allocator(const char* name = "DEFAULT"):  m_name(name) {}
  ~allocator(void) {}
  void *allocate(u32 bytes, int flags = 0);
  void *allocate_aligned(u32 bytes, u32 alignment, int flags = 0);
  void deallocate(void* ptr, u32 bytes);
  const char *get_name(void) const { return m_name; }

private:
  const char *m_name;
};

INLINE bool operator==(const allocator&, const allocator&) {
  return true;
}
INLINE bool operator!=(const allocator& lhs, const allocator& rhs) {
  return !(lhs == rhs);
}
INLINE void* allocator::allocate(u32 bytes, int) {
  return memalloc(bytes, __FILE__, __LINE__);
}
INLINE void allocator::deallocate(void* ptr, u32) {
  memfree(ptr);
}

/*-------------------------------------------------------------------------
 - algorithms, helper classes and functions
 -------------------------------------------------------------------------*/
template<class T> INLINE float heapscore(const T &n) { return n; }
template<class T> INLINE bool compareless(const T &x, const T &y) { return x < y; }
static struct niltype {} nil MAYBE_UNUSED;
static struct infype {} inf MAYBE_UNUSED;
static struct neginfype {} neginf MAYBE_UNUSED;
template<int TVal> struct int_to_type { enum { value = TVal }; };

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

template <typename T0, typename T1> struct pair {
  INLINE pair(void) {}
  INLINE pair(const T0 &t0, const T1 &t1) : first(t0), second(t1) {}
  INLINE pair(const pair &other) : first(other.first), second(other.second) {}
  INLINE pair &operator= (const pair &other) {
    first=other.first;
    second=other.second;
    return *this;
  }
  T0 first;
  T1 second;
};
template<typename T0, typename T1> struct is_pod<pair<T0, T1> > {
  enum { value = (is_pod<T0>::value || is_fundamental<T0>::value) && 
    (is_pod<T1>::value || is_fundamental<T1>::value) };
};
template <typename T0, typename T1>
INLINE pair<T0,T1> makepair(const T0 &t0, const T1 &t1) { return pair<T0,T1>(t0,t1); }

/*-------------------------------------------------------------------------
 - fast growing pool allocation
 -------------------------------------------------------------------------*/
template <typename T> class growingpool {
public:
  growingpool(void) : current(NEW(growingpoolelem, 1)) {}
  ~growingpool(void) { SAFE_DELETE(current); }
  T *allocate(void) {
    if (current->allocated == current->maxelemnum) {
      growingpoolelem *elem = NEW(growingpoolelem, 2*current->maxelemnum);
      elem->next = current;
      current = elem;
    }
    T *data = current->data + current->allocated++;
    return data;
  }
private:
  // chunk of elements to allocate
  struct growingpoolelem {
    growingpoolelem(size_t elemnum) {
      data = NEWAE(T, elemnum);
      next = NULL;
      maxelemnum = elemnum;
      allocated = 0;
    }
    ~growingpoolelem(void) {
      SAFE_DELETEA(data);
      SAFE_DELETE(next);
    }
    T *data;
    growingpoolelem *next;
    size_t allocated, maxelemnum;
  };
  growingpoolelem *current;
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

bool strequal(const char *s1, const char *s2);
bool contains(const char *haystack, const char *needle);
char *tokenize(char *s1, const char *s2, char **lasts);
char *path(char *s);
char *loadfile(char *fn, int *size);
void endianswap(void *, int, int);
int islittleendian(void);
void initendiancheck(void);
void writebmp(const int *data, int w, int h, const char *name);

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
void link(intrusive_list_node *node, intrusive_list_node *next);
void unlink(intrusive_list_node *node);

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
template<typename T> struct ref {
  INLINE ref(void) : ptr(NULL) {}
  INLINE ref(const ref &input) : ptr(input.ptr) { if (ptr) ptr->acquire(); }
  INLINE ref(T* const input) : ptr(input) { if (ptr) ptr->acquire(); }
  INLINE ~ref(void) { if (ptr) ptr->release();  }
  INLINE operator bool(void) const       { return ptr != NULL; }
  INLINE operator T* (void)  const       { return ptr; }
  INLINE const T& operator* (void) const { return *ptr; }
  INLINE const T* operator->(void) const { return  ptr;}
  INLINE T& operator* (void) {return *ptr;}
  INLINE T* operator->(void) {return  ptr;}
  INLINE ref &operator= (const ref &input);
  INLINE ref &operator= (niltype);
  template<typename U> INLINE ref<U> cast(void) { return ref<U>((U*)(ptr)); }
  template<typename U> INLINE const ref<U> cast(void) const { return ref<U>((U*)(ptr)); }
  T* ptr;
};

struct refcount {
  INLINE refcount(void) : refcounter(0) {}
  virtual ~refcount(void) {}
  INLINE void acquire(void) { refcounter++; }
  INLINE void release(void) {
    if (--refcounter == 0) {
      auto ptr = this;
      SAFE_DELETE(ptr);
    }
  }
  atomic refcounter;
};

template <typename T>
INLINE ref<T> &ref<T>::operator= (const ref<T> &input) {
  if (input.ptr) input.ptr->acquire();
  if (ptr) ptr->release();
  *(T**)&ptr = input.ptr;
  return *this;
}
template <typename T>
INLINE ref<T> &ref<T>::operator= (niltype) {
  if (ptr) ptr->release();
  *(T**)&ptr = NULL;
  return *this;
}
} // namespace cube

