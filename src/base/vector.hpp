#pragma once
#include "stl.hpp"
#include "algorithm.hpp"

namespace cube
{
struct base_vector {
  typedef int size_type;
  static const size_type npos = size_type(-1);
};

/*-------------------------------------------------------------------------
 - resizeable vector
 -------------------------------------------------------------------------*/
template<typename T, class TAllocator>
struct standard_vector_storage {
  explicit standard_vector_storage(const TAllocator& allocator)
    : m_begin(0), m_end(0), m_capacityEnd(0), m_allocator(allocator)
  {}
  explicit standard_vector_storage(niltype) {}

  void reallocate(base_vector::size_type newCapacity, base_vector::size_type oldSize) {
    T* newBegin = static_cast<T*>(m_allocator.allocate(newCapacity * sizeof(T)));
    const base_vector::size_type newSize = oldSize < newCapacity ? oldSize : newCapacity;
    // copy old data if needed
    if (m_begin) {
      cube::copy_construct_n(m_begin, newSize, newBegin);
      destroy(m_begin, oldSize);
    }
    m_begin = newBegin;
    m_end = m_begin + newSize;
    m_capacityEnd = m_begin + newCapacity;
    ASSERT(invariant());
  }

  // reallocates memory, does not copy contents of old buffer
  void reallocate_discard_old(base_vector::size_type newCapacity) {
    ASSERT(newCapacity > base_vector::size_type(m_capacityEnd - m_begin));
    T* newBegin = static_cast<T*>(m_allocator.allocate(newCapacity * sizeof(T)));
    const base_vector::size_type currSize((base_vector::size_type)(m_end - m_begin));
    if (m_begin) destroy(m_begin, currSize);
    m_begin = newBegin;
    m_end = m_begin + currSize;
    m_capacityEnd = m_begin + newCapacity;
    ASSERT(invariant());
  }
  void destroy(T* ptr, base_vector::size_type n) {
    cube::destruct_n(ptr, n);
    m_allocator.deallocate(ptr, n * sizeof(T));
  }
  void reset(void) {
    if (m_begin) m_allocator.deallocate(m_begin, (m_end-m_begin)*sizeof(T));
    m_begin = m_end = 0;
    m_capacityEnd = 0;
  }
  bool invariant(void) const {
    return m_end >= m_begin;
  }
  INLINE void record_high_watermark(void) {}
  // assume same allocator!
  INLINE void swap(standard_vector_storage<T, TAllocator> &other) {
    cube::swap(m_begin, other.m_begin);
    cube::swap(m_end, other.m_end);
    cube::swap(m_capacityEnd, other.m_capacityEnd);
  }
  T *m_begin;
  T *m_end;
  T *m_capacityEnd;
  TAllocator m_allocator;
};

template<typename T, class TAllocator = cube::allocator, class TStorage = standard_vector_storage<T, TAllocator>>
class vector : public base_vector, private TStorage
{
private:
  using TStorage::m_begin;
  using TStorage::m_end;
  using TStorage::m_capacityEnd;
  using TStorage::m_allocator;
  using TStorage::invariant;
  using TStorage::reallocate;

public:
  typedef T value_type;
  typedef T *iterator;
  typedef const T *const_iterator;
  typedef TAllocator allocator_type;
  static const size_type kInitialCapacity = 16;

  explicit vector(const allocator_type& allocator = allocator_type())
    : TStorage(allocator)
  {}
  explicit vector(size_type initialSize, const allocator_type& allocator = allocator_type()) :
    TStorage(allocator) { resize(initialSize); }
  vector(const T* first, const T* last, const allocator_type& allocator = allocator_type()) :
    TStorage(allocator) { assign(first, last); }

  // @note: allocator is not copied from rhs.
  // @note: will not perform default constructor for newly created objects.
  vector(const vector& rhs, const allocator_type& allocator = allocator_type())
    : TStorage(allocator) {
    if(rhs.size() == 0) // nothing to do
      return;
    TStorage::reallocate_discard_old(rhs.capacity());
    cube::copy_construct_n(rhs.m_begin, rhs.size(), m_begin);
    m_end = m_begin + rhs.size();
    TStorage::record_high_watermark();
    ASSERT(invariant());
  }
  explicit vector(niltype) : TStorage(nil) {}
  ~vector(void) {
    if (TStorage::m_begin != 0)
      TStorage::destroy(TStorage::m_begin, size());
  }

  // @note: allocator is not copied!
  // @note: will not perform default constructor for newly created objects,
  //        just initialize with copy ctor of elements of rhs.
  vector& operator=(const vector& rhs) {
    copy(rhs);
    return *this;
  }

  void copy(const vector& rhs) {
    const size_type newSize = rhs.size();
    if (newSize > capacity())
      TStorage::reallocate_discard_old(rhs.capacity());
    cube::copy_construct_n(rhs.m_begin, newSize, m_begin);
    m_end = m_begin + newSize;
    TStorage::record_high_watermark();
    ASSERT(invariant());
  }

  INLINE void swap(vector<T, TAllocator, TStorage> &other) {
    TStorage::swap(other);
  }
  iterator begin(void) { return m_begin; }
  iterator end(void)   { return m_end; }
  const_iterator end(void) const   { return m_end; }
  const_iterator begin(void) const { return m_begin; }
  size_type size(void) const      { return size_type(m_end - m_begin); }
  bool empty(void) const          { return m_begin == m_end; }
  size_type capacity(void) const  { return size_type(m_capacityEnd - m_begin); }

  INLINE T* data(void)             { return empty() ? 0 : m_begin; }
  INLINE const T* data(void) const { return empty() ? 0 : m_begin; }

  INLINE T& front(void) {
    ASSERT(!empty());
    return *begin();
  }
  INLINE const T &front(void) const {
    ASSERT(!empty());
    return *begin();
  }
  INLINE T &back(void) {
    ASSERT(!empty());
    return *(end() - 1);
  }
  const T &back(void) const {
    ASSERT(!empty());
    return *(end() - 1);
  }

  INLINE T &operator[](size_type i) { return at(i); }
  INLINE const T &operator[](size_type i) const { return at(i); }
  INLINE T &at(size_type i) {
    ASSERT(i < size());
    return m_begin[i];
  }
  INLINE const T& at(size_type i) const {
    ASSERT(i < size());
    return m_begin[i];
  }

  void push_back(const T& v) {
    if (m_end < m_capacityEnd)
      cube::copy_construct(m_end++, v);
    else {
      grow();
      cube::copy_construct(m_end++, v);
    }
    TStorage::record_high_watermark();
  }

  // @note: extension. use instead of push_back(t()) or resize(size() + 1).
  void push_back(void) {
    if (m_end == m_capacityEnd)
      grow();
    cube::construct(m_end);
    ++m_end;
    TStorage::record_high_watermark();
  }
  INLINE T &add(const T &x) {
    push_back(x);
    return back();
  }
  INLINE T &add(void) {
    push_back();
    return back();
  }
  void pop_back(void) {
    ASSERT(!empty());
    --m_end;
    cube::destruct(m_end);
  }

  void assign(const T* first, const T* last) {
    // iterators cannot be in range!
    ASSERT(!validate_iterator(first));
    ASSERT(!validate_iterator(last));

    const size_type count = size_type(last - first);
    ASSERT(count > 0);
    clear();
    if (m_begin + count > m_capacityEnd)
      TStorage::reallocate_discard_old(compute_new_capacity(count));

    cube::copy_n(first, count, m_begin);
    m_end = m_begin + count;
    TStorage::record_high_watermark();
    ASSERT(invariant());
  }

  void insert(size_type index, size_type n, const T& val) {
    ASSERT(invariant());
    const size_type indexEnd = index + n;
    const size_type prevSize = size();
    if (m_end + n > m_capacityEnd)
      reallocate(compute_new_capacity(prevSize + n), prevSize);

    // past 'end', needs to copy construct.
    if (indexEnd > prevSize) {
      const size_type numCopy  = prevSize - index;
      const size_type numAppend = indexEnd - prevSize;
      ASSERT(numCopy >= 0 && numAppend >= 0);
      ASSERT(numAppend + numCopy == n);
      iterator itOut = m_begin + prevSize;
      for (size_type i = 0; i < numAppend; ++i, ++itOut)
        cube::copy_construct(itOut, val);
      cube::copy_construct_n(m_begin + index, numCopy, itOut);
      for (size_type i = 0; i < numCopy; ++i)
        m_begin[index + i] = val;
    } else {
      cube::copy_construct_n(m_end - n, n, m_end);
      iterator insertPos = m_begin + index;
      cube::move_n(insertPos, prevSize - indexEnd, insertPos + n);
      for (size_type i = 0; i < n; ++i)
        insertPos[i] = val;
    }
    m_end += n; 
    TStorage::record_high_watermark();
  }

  // @pre validate_iterator(it)
  // @note use push_back for maximum efficiency if it == end()!
  void insert(iterator it, size_type n, const T& val) {
    ASSERT(validate_iterator(it));
    ASSERT(invariant());
    insert(size_type(it - m_begin), n, val);
  }

  iterator insert(iterator it, const T& val) {
    ASSERT(validate_iterator(it));
    ASSERT(invariant());
    // @todo: optimize for toMove==0 --> push_back here?
    const size_type index = (size_type)(it - m_begin);
    if (m_end == m_capacityEnd) {
      grow();
      it = m_begin + index;
    } else
      cube::construct(m_end);

    // @note: conditional vs empty loop, what's better?
    if (m_end > it) {
      if(!has_trivial_copy<T>::value) {
        const size_type prevSize = size();
        ASSERT(index <= prevSize);
        const size_type toMove = prevSize - index;

        cube::internal::move_n(it, toMove, it + 1, int_to_type<has_trivial_copy<T>::value>());
      } else {
        ASSERT(it < m_end);
        const size_t n = reinterpret_cast<uintptr>(m_end) - reinterpret_cast<uintptr>(it);
        memmove(it + 1, it, n);
      }
    }
    *it = val;
    ++m_end;
    ASSERT(invariant());
    TStorage::record_high_watermark();
    return it;
  }

  // @pre validate_iterator(it)
  // @pre it != end()
  iterator erase(iterator it) {
    ASSERT(validate_iterator(it));
    ASSERT(it != end());
    ASSERT(invariant());

    // move everything down, overwriting *it
    if (it + 1 < m_end)
      move_down_1(it, int_to_type<has_trivial_copy<T>::value>());
    --m_end;
    cube::destruct(m_end);
    return it;
  }
  iterator erase(iterator first, iterator last) {
    ASSERT(validate_iterator(first));
    ASSERT(validate_iterator(last));
    ASSERT(invariant());
    if (last <= first) return end();

    const size_type indexFirst = size_type(first - m_begin);
    const size_type toRemove = size_type(last - first);
    if (toRemove > 0) {
      move_down(last, first, int_to_type<has_trivial_copy<T>::value>());
      shrink(size() - toRemove);
    }
    return m_begin + indexFirst;
  }

  // *much* quicker version of erase, does not preserve collection order.
  INLINE void erase_unordered(iterator it) {
    ASSERT(validate_iterator(it));
    ASSERT(it != end());
    ASSERT(invariant());
    const iterator itNewEnd = end() - 1;
    if (it != itNewEnd)
      *it = *itNewEnd;
    pop_back();
  }

  void resize(size_type n) {
    if (n > size())
      insert(m_end, n - size(), value_type());
    else
      shrink(n);
  }
  void reserve(size_type n) {
    if (n > capacity())
      reallocate(n, size());
  }

  // removes all elements from this vector (calls their destructors).
  // does not release memory.
  void clear(void) {
    shrink(0);
    ASSERT(invariant());
  }

  // ea stl concept.
  // resets container to an initialized, unallocated state.
  // safe only for value types with trivial destructor.
  void reset(void) {
    TStorage::reset();
    ASSERT(invariant());
  }

  // extension: allows to limit amount of allocated memory.
  void set_capacity(size_type newCapacity) {
    reallocate(newCapacity, size());
  }

  size_type index_of(const T& item, size_type index = 0) const {
    ASSERT(index >= 0 && index < size());
    for (; index < size(); ++index)
      if (m_begin[index] == item)
        return index;
    return npos;
  }
  iterator find(const T& item) {
    iterator itEnd = end();
    for (iterator it = begin(); it != itEnd; ++it)
      if (*it == item)
        return it;
    return itEnd;
  }

  const allocator_type& get_allocator() const { return m_allocator; }
  void set_allocator(const allocator_type& allocator) { m_allocator = allocator; }

  INLINE bool validate_iterator(const_iterator it) const {
    return it >= begin() && it <= end();
  }
  INLINE size_type get_high_watermark() const {
    return TStorage::get_high_watermark();
  }

private:
  size_type compute_new_capacity(size_type newMinCapacity) const {
    const size_type c = capacity();
    return (newMinCapacity > c * 2 ? newMinCapacity : (c == 0 ? kInitialCapacity : c * 2));
  }
  inline void grow(void) {
    ASSERT(m_end == m_capacityEnd); // size == capacity!
    const size_type c = capacity();
    reallocate((m_capacityEnd == 0 ? kInitialCapacity : c * 2), c);
  }
  INLINE void shrink(size_type newSize) {
    ASSERT(newSize <= size());
    const size_type toShrink = size() - newSize;
    cube::destruct_n(m_begin + newSize, toShrink);
    m_end = m_begin + newSize;
  }

  // the following two methods are only to get better cache behavior.  we do not
  // really need 'move' here if copying one-by-one, only for memcpy/memmove (on
  // some platforms, see
  // http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.kui0002a/c51_memcpy.htm for example).
  INLINE void move_down_1(iterator it, int_to_type<true> itt) {
    internal::move(it + 1, m_end, it, itt);
  }
  INLINE void move_down_1(iterator it, int_to_type<false> itt) {
    internal::copy(it + 1, m_end, it, itt);
  }

  INLINE void move_down(iterator it_start, iterator it_result, int_to_type<true> itt) {
    ASSERT(it_start > it_result);
    internal::move(it_start, m_end, it_result, itt);
  }
  INLINE void move_down(iterator it_start, iterator it_result, int_to_type<false> itt) {
    ASSERT(it_start > it_result);
    internal::copy(it_start, m_end, it_result, itt);
  }
};

// commonly used typedefs
typedef vector<char *> cvector;
typedef vector<int> ivector;
} // namespace cube

