/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_ITERATION_RANGE_H_
#define ART_LIBARTBASE_BASE_ITERATION_RANGE_H_

#include <iterator>
#include <type_traits>

namespace art {

// Helper class that acts as a container for range-based loops, given an iteration
// range [first, last) defined by two iterators.
template <typename Iter>
class IterationRange {
 public:
  using iterator        = Iter;
  using difference_type = typename std::iterator_traits<Iter>::difference_type;
  using value_type      = typename std::iterator_traits<Iter>::value_type;
  using pointer         = typename std::iterator_traits<Iter>::pointer;
  using reference       = typename std::iterator_traits<Iter>::reference;

  IterationRange(iterator first, iterator last) : first_(first), last_(last) { }

  iterator begin() const { return first_; }
  iterator end() const { return last_; }
  iterator cbegin() const { return first_; }
  iterator cend() const { return last_; }

 protected:
  iterator first_;
  iterator last_;
};

template <typename Iter>
inline IterationRange<Iter> MakeIterationRange(const Iter& begin_it, const Iter& end_it) {
  return IterationRange<Iter>(begin_it, end_it);
}

template <typename List>
inline auto MakeIterationRange(List& list) -> IterationRange<decltype(list.begin())> {
  static_assert(std::is_same_v<decltype(list.begin()), decltype(list.end())>,
                "Different iterator types");
  return MakeIterationRange(list.begin(), list.end());
}

template <typename Iter>
inline IterationRange<Iter> MakeEmptyIterationRange(const Iter& it) {
  return IterationRange<Iter>(it, it);
}

template <typename Container>
inline auto ReverseRange(Container&& c) {
  using riter = typename std::reverse_iterator<decltype(c.begin())>;
  return MakeIterationRange(riter(c.end()), riter(c.begin()));
}

template <typename T, size_t size>
inline auto ReverseRange(T (&array)[size]) {
  return ReverseRange(MakeIterationRange<T*>(array, array+size));
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_ITERATION_RANGE_H_
