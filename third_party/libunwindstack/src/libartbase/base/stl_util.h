/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_LIBARTBASE_BASE_STL_UTIL_H_
#define ART_LIBARTBASE_BASE_STL_UTIL_H_

#include <algorithm>
#include <iterator>
#include <set>
#include <sstream>

#include <android-base/logging.h>

#include "base/iteration_range.h"

namespace art {

// STLDeleteContainerPointers()
//  For a range within a container of pointers, calls delete
//  (non-array version) on these pointers.
// NOTE: for these three functions, we could just implement a DeleteObject
// functor and then call for_each() on the range and functor, but this
// requires us to pull in all of algorithm.h, which seems expensive.
// For hash_[multi]set, it is important that this deletes behind the iterator
// because the hash_set may call the hash function on the iterator when it is
// advanced, which could result in the hash function trying to deference a
// stale pointer.
template <class ForwardIterator>
void STLDeleteContainerPointers(ForwardIterator begin,
                                ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete *temp;
  }
}

// STLDeleteElements() deletes all the elements in an STL container and clears
// the container.  This function is suitable for use with a vector, set,
// hash_set, or any other STL container which defines sensible begin(), end(),
// and clear() methods.
//
// If container is null, this function is a no-op.
//
// As an alternative to calling STLDeleteElements() directly, consider
// using a container of std::unique_ptr, which ensures that your container's
// elements are deleted when the container goes out of scope.
template <class T>
void STLDeleteElements(T *container) {
  if (container != nullptr) {
    STLDeleteContainerPointers(container->begin(), container->end());
    container->clear();
  }
}

// Given an STL container consisting of (key, value) pairs, STLDeleteValues
// deletes all the "value" components and clears the container.  Does nothing
// in the case it's given a null pointer.
template <class T>
void STLDeleteValues(T *v) {
  if (v != nullptr) {
    for (typename T::iterator i = v->begin(); i != v->end(); ++i) {
      delete i->second;
    }
    v->clear();
  }
}

// Deleter using free() for use with std::unique_ptr<>. See also UniqueCPtr<> below.
struct FreeDelete {
  // NOTE: Deleting a const object is valid but free() takes a non-const pointer.
  void operator()(const void* ptr) const {
    free(const_cast<void*>(ptr));
  }
};

// Alias for std::unique_ptr<> that uses the C function free() to delete objects.
template <typename T>
using UniqueCPtr = std::unique_ptr<T, FreeDelete>;

// Find index of the first element with the specified value known to be in the container.
template <typename Container, typename T>
size_t IndexOfElement(const Container& container, const T& value) {
  auto it = std::find(container.begin(), container.end(), value);
  DCHECK(it != container.end());  // Must exist.
  return std::distance(container.begin(), it);
}

// Remove the first element with the specified value known to be in the container.
template <typename Container, typename T>
void RemoveElement(Container& container, const T& value) {
  auto it = std::find(container.begin(), container.end(), value);
  DCHECK(it != container.end());  // Must exist.
  container.erase(it);
}

// Replace the first element with the specified old_value known to be in the container.
template <typename Container, typename T>
void ReplaceElement(Container& container, const T& old_value, const T& new_value) {
  auto it = std::find(container.begin(), container.end(), old_value);
  DCHECK(it != container.end());  // Must exist.
  *it = new_value;
}

// Search for an element with the specified value and return true if it was found, false otherwise.
template <typename Container, typename T>
bool ContainsElement(const Container& container, const T& value, size_t start_pos = 0u) {
  DCHECK_LE(start_pos, container.size());
  auto start = container.begin();
  std::advance(start, start_pos);
  auto it = std::find(start, container.end(), value);
  return it != container.end();
}

template <typename T>
bool ContainsElement(const std::set<T>& container, const T& value) {
  return container.count(value) != 0u;
}

// 32-bit FNV-1a hash function suitable for std::unordered_map.
// It can be used with any container which works with range-based for loop.
// See http://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
template <typename Vector>
struct FNVHash {
  size_t operator()(const Vector& vector) const {
    uint32_t hash = 2166136261u;
    for (const auto& value : vector) {
      hash = (hash ^ value) * 16777619u;
    }
    return hash;
  }
};

// Returns a copy of the passed vector that doesn't memory-own its entries.
template <typename T>
static inline std::vector<T*> MakeNonOwningPointerVector(const std::vector<std::unique_ptr<T>>& src) {
  std::vector<T*> result;
  result.reserve(src.size());
  for (const std::unique_ptr<T>& t : src) {
    result.push_back(t.get());
  }
  return result;
}

template <typename IterLeft, typename IterRight>
class ZipLeftIter {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = std::pair<typename IterLeft::value_type, typename IterRight::value_type>;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = void;

  ZipLeftIter(IterLeft left, IterRight right) : left_iter_(left), right_iter_(right) {}
  ZipLeftIter<IterLeft, IterRight>& operator++() {
    ++left_iter_;
    ++right_iter_;
    return *this;
  }
  ZipLeftIter<IterLeft, IterRight> operator++(int) {
    ZipLeftIter<IterLeft, IterRight> ret(left_iter_, right_iter_);
    ++(*this);
    return ret;
  }
  bool operator==(const ZipLeftIter<IterLeft, IterRight>& other) const {
    return left_iter_ == other.left_iter_;
  }
  bool operator!=(const ZipLeftIter<IterLeft, IterRight>& other) const {
    return !(*this == other);
  }
  std::pair<typename IterLeft::value_type, typename IterRight::value_type> operator*() const {
    return std::make_pair(*left_iter_, *right_iter_);
  }

 private:
  IterLeft left_iter_;
  IterRight right_iter_;
};

class CountIter {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = size_t;
  using difference_type = size_t;
  using pointer = void;
  using reference = void;

  CountIter() : count_(0) {}
  explicit CountIter(size_t count) : count_(count) {}
  CountIter& operator++() {
    ++count_;
    return *this;
  }
  CountIter operator++(int) {
    size_t ret = count_;
    ++count_;
    return CountIter(ret);
  }
  bool operator==(const CountIter& other) const {
    return count_ == other.count_;
  }
  bool operator!=(const CountIter& other) const {
    return !(*this == other);
  }
  size_t operator*() const {
    return count_;
  }

 private:
  size_t count_;
};

// Make an iteration range that returns a pair of the element and the index of the element.
template <typename Iter>
static inline IterationRange<ZipLeftIter<Iter, CountIter>> ZipCount(IterationRange<Iter> iter) {
  return IterationRange(ZipLeftIter(iter.begin(), CountIter(0)),
                        ZipLeftIter(iter.end(), CountIter(-1)));
}

// Make an iteration range that returns a pair of the outputs of two iterators. Stops when the first
// (left) one is exhausted. The left iterator must be at least as long as the right one.
template <typename IterLeft, typename IterRight>
static inline IterationRange<ZipLeftIter<IterLeft, IterRight>> ZipLeft(
    IterationRange<IterLeft> iter_left, IterationRange<IterRight> iter_right) {
  return IterationRange(ZipLeftIter(iter_left.begin(), iter_right.begin()),
                        ZipLeftIter(iter_left.end(), iter_right.end()));
}

static inline IterationRange<CountIter> Range(size_t start, size_t end) {
  return IterationRange(CountIter(start), CountIter(end));
}

static inline IterationRange<CountIter> Range(size_t end) {
  return Range(0, end);
}

template <typename RealIter, typename Filter>
struct FilterIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = typename std::iterator_traits<RealIter>::value_type;
  using difference_type = ptrdiff_t;
  using pointer = typename std::iterator_traits<RealIter>::pointer;
  using reference = typename std::iterator_traits<RealIter>::reference;

  FilterIterator(RealIter rl,
                 Filter cond,
                 std::optional<RealIter> end = std::nullopt)
      : real_iter_(rl), cond_(cond), end_(end) {
    DCHECK(std::make_optional(rl) == end_ || cond_(*real_iter_));
  }

  FilterIterator<RealIter, Filter>& operator++() {
    DCHECK(std::make_optional(real_iter_) != end_);
    do {
      if (std::make_optional(++real_iter_) == end_) {
        break;
      }
    } while (!cond_(*real_iter_));
    return *this;
  }
  FilterIterator<RealIter, Filter> operator++(int) {
    FilterIterator<RealIter, Filter> ret(real_iter_, cond_, end_);
    ++(*this);
    return ret;
  }
  bool operator==(const FilterIterator<RealIter, Filter>& other) const {
    return real_iter_ == other.real_iter_;
  }
  bool operator!=(const FilterIterator<RealIter, Filter>& other) const {
    return !(*this == other);
  }
  typename RealIter::value_type operator*() const {
    return *real_iter_;
  }

 private:
  RealIter real_iter_;
  Filter cond_;
  std::optional<RealIter> end_;
};

template <typename BaseRange, typename FilterT>
static inline auto Filter(BaseRange&& range, FilterT cond) {
  auto end = range.end();
  auto start = std::find_if(range.begin(), end, cond);
  return MakeIterationRange(FilterIterator(start, cond, std::make_optional(end)),
                            FilterIterator(end, cond, std::make_optional(end)));
}

template <typename Val>
struct NonNullFilter {
 public:
  static_assert(std::is_pointer_v<Val>, "Must be pointer type!");
  constexpr bool operator()(Val v) const {
    return v != nullptr;
  }
};

template <typename InnerIter>
using FilterNull = FilterIterator<InnerIter, NonNullFilter<typename InnerIter::value_type>>;

template <typename InnerIter>
static inline IterationRange<FilterNull<InnerIter>> FilterOutNull(IterationRange<InnerIter> inner) {
  return Filter(inner, NonNullFilter<typename InnerIter::value_type>());
}

template <typename Val>
struct SafePrinter  {
  const Val* val_;
};

template<typename Val>
std::ostream& operator<<(std::ostream& os, const SafePrinter<Val>& v) {
  if (v.val_ == nullptr) {
    return os << "NULL";
  } else {
    return os << *v.val_;
  }
}

template<typename Val>
SafePrinter<Val> SafePrint(const Val* v) {
  return SafePrinter<Val>{v};
}

// Helper struct for iterating a split-string without allocation.
struct SplitStringIter {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = std::string_view;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = void;

  // Direct iterator constructor. The iteration state is only the current index.
  // We use that with the split char and the full string to get the current and
  // next segment.
  SplitStringIter(size_t index, char split, std::string_view sv)
      : cur_index_(index), split_on_(split), sv_(sv) {}
  SplitStringIter(const SplitStringIter&) = default;
  SplitStringIter(SplitStringIter&&) = default;
  SplitStringIter& operator=(SplitStringIter&&) = default;
  SplitStringIter& operator=(const SplitStringIter&) = default;

  SplitStringIter& operator++() {
    size_t nxt = sv_.find(split_on_, cur_index_);
    if (nxt == std::string_view::npos) {
      cur_index_ = std::string_view::npos;
    } else {
      cur_index_ = nxt + 1;
    }
    return *this;
  }

  SplitStringIter operator++(int) {
    SplitStringIter ret(cur_index_, split_on_, sv_);
    ++(*this);
    return ret;
  }

  bool operator==(const SplitStringIter& other) const {
    return sv_ == other.sv_ && split_on_ == other.split_on_ && cur_index_== other.cur_index_;
  }

  bool operator!=(const SplitStringIter& other) const {
    return !(*this == other);
  }

  typename std::string_view operator*() const {
    return sv_.substr(cur_index_, sv_.substr(cur_index_).find(split_on_));
  }

 private:
  size_t cur_index_;
  char split_on_;
  std::string_view sv_;
};

// Create an iteration range over the string 'sv' split at each 'target' occurrence.
// Eg: SplitString(":foo::bar") -> ["", "foo", "", "bar"]
inline IterationRange<SplitStringIter> SplitString(std::string_view sv, char target) {
  return MakeIterationRange(SplitStringIter(0, target, sv),
                            SplitStringIter(std::string_view::npos, target, sv));
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_STL_UTIL_H_
