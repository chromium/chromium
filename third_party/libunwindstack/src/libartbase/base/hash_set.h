/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_HASH_SET_H_
#define ART_LIBARTBASE_BASE_HASH_SET_H_

#include <stdint.h>

#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <android-base/logging.h>

#include "base/data_hash.h"
#include "bit_utils.h"
#include "macros.h"

namespace art {

template <class Elem, class HashSetType>
class HashSetIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = Elem;
  using difference_type = std::ptrdiff_t;
  using pointer = Elem*;
  using reference = Elem&;

  HashSetIterator(const HashSetIterator&) = default;
  HashSetIterator(HashSetIterator&&) noexcept = default;
  HashSetIterator(HashSetType* hash_set, size_t index) : index_(index), hash_set_(hash_set) {}

  // Conversion from iterator to const_iterator.
  template <class OtherElem,
            class OtherHashSetType,
            typename = std::enable_if_t<
                std::is_same_v<Elem, const OtherElem> &&
                std::is_same_v<HashSetType, const OtherHashSetType>>>
  HashSetIterator(const HashSetIterator<OtherElem, OtherHashSetType>& other)
      : index_(other.index_), hash_set_(other.hash_set_) {}

  HashSetIterator& operator=(const HashSetIterator&) = default;
  HashSetIterator& operator=(HashSetIterator&&) noexcept = default;

  bool operator==(const HashSetIterator& other) const {
    return hash_set_ == other.hash_set_ && this->index_ == other.index_;
  }

  bool operator!=(const HashSetIterator& other) const {
    return !(*this == other);
  }

  HashSetIterator operator++() {  // Value after modification.
    this->index_ = hash_set_->NextNonEmptySlot(index_);
    return *this;
  }

  HashSetIterator operator++(int) {
    HashSetIterator temp = *this;
    ++*this;
    return temp;
  }

  Elem& operator*() const {
    DCHECK(!hash_set_->IsFreeSlot(this->index_));
    return hash_set_->ElementForIndex(this->index_);
  }

  Elem* operator->() const {
    return &**this;
  }

 private:
  size_t index_;
  HashSetType* hash_set_;

  template <class Elem1, class HashSetType1, class Elem2, class HashSetType2>
  friend bool operator==(const HashSetIterator<Elem1, HashSetType1>& lhs,
                         const HashSetIterator<Elem2, HashSetType2>& rhs);
  template <class T, class EmptyFn, class HashFn, class Pred, class Alloc> friend class HashSet;
  template <class OtherElem, class OtherHashSetType> friend class HashSetIterator;
};

template <class Elem1, class HashSetType1, class Elem2, class HashSetType2>
bool operator==(const HashSetIterator<Elem1, HashSetType1>& lhs,
                const HashSetIterator<Elem2, HashSetType2>& rhs) {
  static_assert(
      std::is_convertible_v<HashSetIterator<Elem1, HashSetType1>,
                            HashSetIterator<Elem2, HashSetType2>> ||
      std::is_convertible_v<HashSetIterator<Elem2, HashSetType2>,
                            HashSetIterator<Elem1, HashSetType1>>, "Bad iterator types.");
  DCHECK_EQ(lhs.hash_set_, rhs.hash_set_);
  return lhs.index_ == rhs.index_;
}

template <class Elem1, class HashSetType1, class Elem2, class HashSetType2>
bool operator!=(const HashSetIterator<Elem1, HashSetType1>& lhs,
                const HashSetIterator<Elem2, HashSetType2>& rhs) {
  return !(lhs == rhs);
}

// Returns true if an item is empty.
template <class T>
class DefaultEmptyFn {
 public:
  void MakeEmpty(T& item) const {
    item = T();
  }
  bool IsEmpty(const T& item) const {
    return item == T();
  }
};

template <class T>
class DefaultEmptyFn<T*> {
 public:
  void MakeEmpty(T*& item) const {
    item = nullptr;
  }
  bool IsEmpty(T* const& item) const {
    return item == nullptr;
  }
};

template <>
class DefaultEmptyFn<std::string> {
 public:
  void MakeEmpty(std::string& item) const {
    item = std::string();
  }
  bool IsEmpty(const std::string& item) const {
    return item.empty();
  }
};

template <class T>
using DefaultHashFn = std::conditional_t<std::is_same_v<T, std::string>, DataHash, std::hash<T>>;

struct DefaultStringEquals {
  // Allow comparison with anything that can be compared to std::string,
  // for example std::string_view.
  template <typename T>
  bool operator()(const std::string& lhs, const T& rhs) const {
    return lhs == rhs;
  }
};

template <class T>
using DefaultPred =
    std::conditional_t<std::is_same_v<T, std::string>, DefaultStringEquals, std::equal_to<T>>;

// Low memory version of a hash set, uses less memory than std::unordered_multiset since elements
// aren't boxed. Uses linear probing to resolve collisions.
// EmptyFn needs to implement two functions MakeEmpty(T& item) and IsEmpty(const T& item).
// TODO: We could get rid of this requirement by using a bitmap, though maybe this would be slower
// and more complicated.
template <class T,
          class EmptyFn = DefaultEmptyFn<T>,
          class HashFn = DefaultHashFn<T>,
          class Pred = DefaultPred<T>,
          class Alloc = std::allocator<T>>
class HashSet {
 public:
  using value_type = T;
  using allocator_type = Alloc;
  using reference = T&;
  using const_reference = const T&;
  using pointer = T*;
  using const_pointer = const T*;
  using iterator = HashSetIterator<T, HashSet>;
  using const_iterator = HashSetIterator<const T, const HashSet>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  static constexpr double kDefaultMinLoadFactor = 0.4;
  static constexpr double kDefaultMaxLoadFactor = 0.7;
  static constexpr size_t kMinBuckets = 1000;

  // If we don't own the data, this will create a new array which owns the data.
  void clear() {
    DeallocateStorage();
    num_elements_ = 0;
    elements_until_expand_ = 0;
  }

  HashSet() : HashSet(kDefaultMinLoadFactor, kDefaultMaxLoadFactor) {}
  explicit HashSet(const allocator_type& alloc) noexcept
      : HashSet(kDefaultMinLoadFactor, kDefaultMaxLoadFactor, alloc) {}

  HashSet(double min_load_factor, double max_load_factor) noexcept
      : HashSet(min_load_factor, max_load_factor, allocator_type()) {}
  HashSet(double min_load_factor, double max_load_factor, const allocator_type& alloc) noexcept
      : HashSet(min_load_factor, max_load_factor, HashFn(), Pred(), alloc) {}

  HashSet(const HashFn& hashfn,
          const Pred& pred) noexcept
      : HashSet(kDefaultMinLoadFactor, kDefaultMaxLoadFactor, hashfn, pred) {}
  HashSet(const HashFn& hashfn,
          const Pred& pred,
          const allocator_type& alloc) noexcept
      : HashSet(kDefaultMinLoadFactor, kDefaultMaxLoadFactor, hashfn, pred, alloc) {}

  HashSet(double min_load_factor,
          double max_load_factor,
          const HashFn& hashfn,
          const Pred& pred) noexcept
      : HashSet(min_load_factor, max_load_factor, hashfn, pred, allocator_type()) {}
  HashSet(double min_load_factor,
          double max_load_factor,
          const HashFn& hashfn,
          const Pred& pred,
          const allocator_type& alloc) noexcept
      : allocfn_(alloc),
        hashfn_(hashfn),
        emptyfn_(),
        pred_(pred),
        num_elements_(0u),
        num_buckets_(0u),
        elements_until_expand_(0u),
        owns_data_(false),
        data_(nullptr),
        min_load_factor_(min_load_factor),
        max_load_factor_(max_load_factor) {
    DCHECK_GT(min_load_factor, 0.0);
    DCHECK_LT(max_load_factor, 1.0);
  }

  HashSet(const HashSet& other)
      : allocfn_(other.allocfn_),
        hashfn_(other.hashfn_),
        emptyfn_(other.emptyfn_),
        pred_(other.pred_),
        num_elements_(other.num_elements_),
        num_buckets_(0),
        elements_until_expand_(other.elements_until_expand_),
        owns_data_(false),
        data_(nullptr),
        min_load_factor_(other.min_load_factor_),
        max_load_factor_(other.max_load_factor_) {
    AllocateStorage(other.NumBuckets());
    for (size_t i = 0; i < num_buckets_; ++i) {
      ElementForIndex(i) = other.data_[i];
    }
  }

  // noexcept required so that the move constructor is used instead of copy constructor.
  // b/27860101
  HashSet(HashSet&& other) noexcept
      : allocfn_(std::move(other.allocfn_)),
        hashfn_(std::move(other.hashfn_)),
        emptyfn_(std::move(other.emptyfn_)),
        pred_(std::move(other.pred_)),
        num_elements_(other.num_elements_),
        num_buckets_(other.num_buckets_),
        elements_until_expand_(other.elements_until_expand_),
        owns_data_(other.owns_data_),
        data_(other.data_),
        min_load_factor_(other.min_load_factor_),
        max_load_factor_(other.max_load_factor_) {
    other.num_elements_ = 0u;
    other.num_buckets_ = 0u;
    other.elements_until_expand_ = 0u;
    other.owns_data_ = false;
    other.data_ = nullptr;
  }

  // Construct with pre-existing buffer, usually stack-allocated,
  // to avoid malloc/free overhead for small HashSet<>s.
  HashSet(value_type* buffer, size_t buffer_size)
      : HashSet(kDefaultMinLoadFactor, kDefaultMaxLoadFactor, buffer, buffer_size) {}
  HashSet(value_type* buffer, size_t buffer_size, const allocator_type& alloc)
      : HashSet(kDefaultMinLoadFactor, kDefaultMaxLoadFactor, buffer, buffer_size, alloc) {}
  HashSet(double min_load_factor, double max_load_factor, value_type* buffer, size_t buffer_size)
      : HashSet(min_load_factor, max_load_factor, buffer, buffer_size, allocator_type()) {}
  HashSet(double min_load_factor,
          double max_load_factor,
          value_type* buffer,
          size_t buffer_size,
          const allocator_type& alloc)
      : HashSet(min_load_factor, max_load_factor, HashFn(), Pred(), buffer, buffer_size, alloc) {}
  HashSet(double min_load_factor,
          double max_load_factor,
          const HashFn& hashfn,
          const Pred& pred,
          value_type* buffer,
          size_t buffer_size,
          const allocator_type& alloc)
      : allocfn_(alloc),
        hashfn_(hashfn),
        pred_(pred),
        num_elements_(0u),
        num_buckets_(buffer_size),
        elements_until_expand_(buffer_size * max_load_factor),
        owns_data_(false),
        data_(buffer),
        min_load_factor_(min_load_factor),
        max_load_factor_(max_load_factor) {
    DCHECK_GT(min_load_factor, 0.0);
    DCHECK_LT(max_load_factor, 1.0);
    for (size_t i = 0; i != buffer_size; ++i) {
      emptyfn_.MakeEmpty(buffer[i]);
    }
  }

  // Construct from existing data.
  // Read from a block of memory, if make_copy_of_data is false, then data_ points to within the
  // passed in ptr_.
  HashSet(const uint8_t* ptr, bool make_copy_of_data, size_t* read_count) noexcept {
    uint64_t temp;
    size_t offset = 0;
    offset = ReadFromBytes(ptr, offset, &temp);
    num_elements_ = static_cast<uint64_t>(temp);
    offset = ReadFromBytes(ptr, offset, &temp);
    num_buckets_ = static_cast<uint64_t>(temp);
    CHECK_LE(num_elements_, num_buckets_);
    offset = ReadFromBytes(ptr, offset, &temp);
    elements_until_expand_ = static_cast<uint64_t>(temp);
    offset = ReadFromBytes(ptr, offset, &min_load_factor_);
    offset = ReadFromBytes(ptr, offset, &max_load_factor_);
    if (!make_copy_of_data) {
      owns_data_ = false;
      data_ = const_cast<T*>(reinterpret_cast<const T*>(ptr + offset));
      offset += sizeof(*data_) * num_buckets_;
    } else {
      AllocateStorage(num_buckets_);
      // Write elements, not that this may not be safe for cross compilation if the elements are
      // pointer sized.
      for (size_t i = 0; i < num_buckets_; ++i) {
        offset = ReadFromBytes(ptr, offset, &data_[i]);
      }
    }
    // Caller responsible for aligning.
    *read_count = offset;
  }

  // Returns how large the table is after being written. If target is null, then no writing happens
  // but the size is still returned. Target must be 8 byte aligned.
  size_t WriteToMemory(uint8_t* ptr) const {
    size_t offset = 0;
    offset = WriteToBytes(ptr, offset, static_cast<uint64_t>(num_elements_));
    offset = WriteToBytes(ptr, offset, static_cast<uint64_t>(num_buckets_));
    offset = WriteToBytes(ptr, offset, static_cast<uint64_t>(elements_until_expand_));
    offset = WriteToBytes(ptr, offset, min_load_factor_);
    offset = WriteToBytes(ptr, offset, max_load_factor_);
    // Write elements, not that this may not be safe for cross compilation if the elements are
    // pointer sized.
    for (size_t i = 0; i < num_buckets_; ++i) {
      offset = WriteToBytes(ptr, offset, data_[i]);
    }
    // Caller responsible for aligning.
    return offset;
  }

  ~HashSet() {
    DeallocateStorage();
  }

  HashSet& operator=(HashSet&& other) noexcept {
    HashSet(std::move(other)).swap(*this);  // NOLINT [runtime/explicit] [5]
    return *this;
  }

  HashSet& operator=(const HashSet& other) {
    HashSet(other).swap(*this);  // NOLINT(runtime/explicit) - a case of lint gone mad.
    return *this;
  }

  // Lower case for c++11 for each.
  iterator begin() {
    iterator ret(this, 0);
    if (num_buckets_ != 0 && IsFreeSlot(ret.index_)) {
      ++ret;  // Skip all the empty slots.
    }
    return ret;
  }

  // Lower case for c++11 for each. const version.
  const_iterator begin() const {
    const_iterator ret(this, 0);
    if (num_buckets_ != 0 && IsFreeSlot(ret.index_)) {
      ++ret;  // Skip all the empty slots.
    }
    return ret;
  }

  // Lower case for c++11 for each.
  iterator end() {
    return iterator(this, NumBuckets());
  }

  // Lower case for c++11 for each. const version.
  const_iterator end() const {
    return const_iterator(this, NumBuckets());
  }

  size_t size() const {
    return num_elements_;
  }

  bool empty() const {
    return size() == 0;
  }

  // Erase algorithm:
  // Make an empty slot where the iterator is pointing.
  // Scan forwards until we hit another empty slot.
  // If an element in between doesn't rehash to the range from the current empty slot to the
  // iterator. It must be before the empty slot, in that case we can move it to the empty slot
  // and set the empty slot to be the location we just moved from.
  // Relies on maintaining the invariant that there's no empty slots from the 'ideal' index of an
  // element to its actual location/index.
  // Note that since erase shuffles back elements, it may result in the same element being visited
  // twice during HashSet iteration. This happens when an element already visited during iteration
  // gets shuffled to the end of the bucket array.
  iterator erase(iterator it) {
    // empty_index is the index that will become empty.
    size_t empty_index = it.index_;
    DCHECK(!IsFreeSlot(empty_index));
    size_t next_index = empty_index;
    bool filled = false;  // True if we filled the empty index.
    while (true) {
      next_index = NextIndex(next_index);
      T& next_element = ElementForIndex(next_index);
      // If the next element is empty, we are done. Make sure to clear the current empty index.
      if (emptyfn_.IsEmpty(next_element)) {
        emptyfn_.MakeEmpty(ElementForIndex(empty_index));
        break;
      }
      // Otherwise try to see if the next element can fill the current empty index.
      const size_t next_hash = hashfn_(next_element);
      // Calculate the ideal index, if it is within empty_index + 1 to next_index then there is
      // nothing we can do.
      size_t next_ideal_index = IndexForHash(next_hash);
      // Loop around if needed for our check.
      size_t unwrapped_next_index = next_index;
      if (unwrapped_next_index < empty_index) {
        unwrapped_next_index += NumBuckets();
      }
      // Loop around if needed for our check.
      size_t unwrapped_next_ideal_index = next_ideal_index;
      if (unwrapped_next_ideal_index < empty_index) {
        unwrapped_next_ideal_index += NumBuckets();
      }
      if (unwrapped_next_ideal_index <= empty_index ||
          unwrapped_next_ideal_index > unwrapped_next_index) {
        // If the target index isn't within our current range it must have been probed from before
        // the empty index.
        ElementForIndex(empty_index) = std::move(next_element);
        filled = true;  // TODO: Optimize
        empty_index = next_index;
      }
    }
    --num_elements_;
    // If we didn't fill the slot then we need go to the next non free slot.
    if (!filled) {
      ++it;
    }
    return it;
  }

  // Find an element, returns end() if not found.
  // Allows custom key (K) types, example of when this is useful:
  // Set of Class* indexed by name, want to find a class with a name but can't allocate
  // a temporary Class object in the heap for performance solution.
  template <typename K>
  iterator find(const K& key) {
    return FindWithHash(key, hashfn_(key));
  }

  template <typename K>
  const_iterator find(const K& key) const {
    return FindWithHash(key, hashfn_(key));
  }

  template <typename K>
  iterator FindWithHash(const K& key, size_t hash) {
    return iterator(this, FindIndex(key, hash));
  }

  template <typename K>
  const_iterator FindWithHash(const K& key, size_t hash) const {
    return const_iterator(this, FindIndex(key, hash));
  }

  // Insert an element with hint.
  // Note: The hint is not very useful for a HashSet<> unless there are many hash conflicts
  // and in that case the use of HashSet<> itself should be reconsidered.
  std::pair<iterator, bool> insert([[maybe_unused]] const_iterator hint, const T& element) {
    return insert(element);
  }
  std::pair<iterator, bool> insert([[maybe_unused]] const_iterator hint, T&& element) {
    return insert(std::move(element));
  }

  // Insert an element.
  std::pair<iterator, bool> insert(const T& element) {
    return InsertWithHash(element, hashfn_(element));
  }
  std::pair<iterator, bool> insert(T&& element) {
    return InsertWithHash(std::move(element), hashfn_(element));
  }

  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  std::pair<iterator, bool> InsertWithHash(U&& element, size_t hash) {
    DCHECK_EQ(hash, hashfn_(element));
    if (num_elements_ >= elements_until_expand_) {
      Expand();
      DCHECK_LT(num_elements_, elements_until_expand_);
    }
    bool find_failed = false;
    auto find_fail_fn = [&](size_t index) ALWAYS_INLINE {
      find_failed = true;
      return index;
    };
    size_t index = FindIndexImpl(element, hash, find_fail_fn);
    if (find_failed) {
      data_[index] = std::forward<U>(element);
      ++num_elements_;
    }
    return std::make_pair(iterator(this, index), find_failed);
  }

  // Insert an element known not to be in the `HashSet<>`.
  void Put(const T& element) {
    return PutWithHash(element, hashfn_(element));
  }
  void Put(T&& element) {
    return PutWithHash(std::move(element), hashfn_(element));
  }

  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  void PutWithHash(U&& element, size_t hash) {
    DCHECK_EQ(hash, hashfn_(element));
    if (num_elements_ >= elements_until_expand_) {
      Expand();
      DCHECK_LT(num_elements_, elements_until_expand_);
    }
    auto find_fail_fn = [](size_t index) ALWAYS_INLINE { return index; };
    size_t index = FindIndexImpl</*kCanFind=*/ false>(element, hash, find_fail_fn);
    data_[index] = std::forward<U>(element);
    ++num_elements_;
  }

  void swap(HashSet& other) {
    // Use argument-dependent lookup with fall-back to std::swap() for function objects.
    using std::swap;
    swap(allocfn_, other.allocfn_);
    swap(hashfn_, other.hashfn_);
    swap(emptyfn_, other.emptyfn_);
    swap(pred_, other.pred_);
    std::swap(data_, other.data_);
    std::swap(num_buckets_, other.num_buckets_);
    std::swap(num_elements_, other.num_elements_);
    std::swap(elements_until_expand_, other.elements_until_expand_);
    std::swap(min_load_factor_, other.min_load_factor_);
    std::swap(max_load_factor_, other.max_load_factor_);
    std::swap(owns_data_, other.owns_data_);
  }

  allocator_type get_allocator() const {
    return allocfn_;
  }

  void ShrinkToMaximumLoad() {
    Resize(size() / max_load_factor_);
  }

  // Reserve enough room to insert until Size() == num_elements without requiring to grow the hash
  // set. No-op if the hash set is already large enough to do this.
  void reserve(size_t num_elements) {
    size_t num_buckets = num_elements / max_load_factor_;
    // Deal with rounding errors. Add one for rounding.
    while (static_cast<size_t>(num_buckets * max_load_factor_) <= num_elements + 1u) {
      ++num_buckets;
    }
    if (num_buckets > NumBuckets()) {
      Resize(num_buckets);
    }
  }

  // To distance that inserted elements were probed. Used for measuring how good hash functions
  // are.
  size_t TotalProbeDistance() const {
    size_t total = 0;
    for (size_t i = 0; i < NumBuckets(); ++i) {
      const T& element = ElementForIndex(i);
      if (!emptyfn_.IsEmpty(element)) {
        size_t ideal_location = IndexForHash(hashfn_(element));
        if (ideal_location > i) {
          total += i + NumBuckets() - ideal_location;
        } else {
          total += i - ideal_location;
        }
      }
    }
    return total;
  }

  // Calculate the current load factor and return it.
  double CalculateLoadFactor() const {
    return static_cast<double>(size()) / static_cast<double>(NumBuckets());
  }

  // Make sure that everything reinserts in the right spot. Returns the number of errors.
  size_t Verify() NO_THREAD_SAFETY_ANALYSIS {
    size_t errors = 0;
    for (size_t i = 0; i < num_buckets_; ++i) {
      T& element = data_[i];
      if (!emptyfn_.IsEmpty(element)) {
        T temp;
        emptyfn_.MakeEmpty(temp);
        std::swap(temp, element);
        size_t first_slot = FirstAvailableSlot(IndexForHash(hashfn_(temp)));
        if (i != first_slot) {
          LOG(ERROR) << "Element " << i << " should be in slot " << first_slot;
          ++errors;
        }
        std::swap(temp, element);
      }
    }
    return errors;
  }

  double GetMinLoadFactor() const {
    return min_load_factor_;
  }

  double GetMaxLoadFactor() const {
    return max_load_factor_;
  }

  // Change the load factor of the hash set. If the current load factor is greater than the max
  // specified, then we resize the hash table storage.
  void SetLoadFactor(double min_load_factor, double max_load_factor) {
    DCHECK_LT(min_load_factor, max_load_factor);
    DCHECK_GT(min_load_factor, 0.0);
    DCHECK_LT(max_load_factor, 1.0);
    min_load_factor_ = min_load_factor;
    max_load_factor_ = max_load_factor;
    elements_until_expand_ = NumBuckets() * max_load_factor_;
    // If the current load factor isn't in the range, then resize to the mean of the minimum and
    // maximum load factor.
    const double load_factor = CalculateLoadFactor();
    if (load_factor > max_load_factor_) {
      Resize(size() / ((min_load_factor_ + max_load_factor_) * 0.5));
    }
  }

  // The hash set expands when Size() reaches ElementsUntilExpand().
  size_t ElementsUntilExpand() const {
    return elements_until_expand_;
  }

  size_t NumBuckets() const {
    return num_buckets_;
  }

 private:
  T& ElementForIndex(size_t index) {
    DCHECK_LT(index, NumBuckets());
    DCHECK(data_ != nullptr);
    return data_[index];
  }

  const T& ElementForIndex(size_t index) const {
    DCHECK_LT(index, NumBuckets());
    DCHECK(data_ != nullptr);
    return data_[index];
  }

  size_t IndexForHash(size_t hash) const {
    // Protect against undefined behavior (division by zero).
    if (UNLIKELY(num_buckets_ == 0)) {
      return 0;
    }
    return hash % num_buckets_;
  }

  size_t NextIndex(size_t index) const {
    if (UNLIKELY(++index >= num_buckets_)) {
      DCHECK_EQ(index, NumBuckets());
      return 0;
    }
    return index;
  }

  // Find the hash table slot for an element, or return NumBuckets() if not found.
  // This value for not found is important so that iterator(this, FindIndex(...)) == end().
  template <typename K>
  ALWAYS_INLINE
  size_t FindIndex(const K& element, size_t hash) const {
    // Guard against failing to get an element for a non-existing index.
    if (UNLIKELY(NumBuckets() == 0)) {
      return 0;
    }
    auto fail_fn = [&]([[maybe_unused]] size_t index) ALWAYS_INLINE { return NumBuckets(); };
    return FindIndexImpl(element, hash, fail_fn);
  }

  // Find the hash table slot for an element, or return an empty slot index if not found.
  template <bool kCanFind = true, typename K, typename FailFn>
  ALWAYS_INLINE
  size_t FindIndexImpl(const K& element, size_t hash, FailFn fail_fn) const {
    DCHECK_NE(NumBuckets(), 0u);
    DCHECK_EQ(hashfn_(element), hash);
    size_t index = IndexForHash(hash);
    while (true) {
      const T& slot = ElementForIndex(index);
      if (emptyfn_.IsEmpty(slot)) {
        return fail_fn(index);
      }
      if (!kCanFind) {
        DCHECK(!pred_(slot, element));
      } else if (pred_(slot, element)) {
        return index;
      }
      index = NextIndex(index);
    }
  }

  bool IsFreeSlot(size_t index) const {
    return emptyfn_.IsEmpty(ElementForIndex(index));
  }

  // Allocate a number of buckets.
  void AllocateStorage(size_t num_buckets) {
    num_buckets_ = num_buckets;
    data_ = allocfn_.allocate(num_buckets_);
    owns_data_ = true;
    for (size_t i = 0; i < num_buckets_; ++i) {
      std::allocator_traits<Alloc>::construct(allocfn_, std::addressof(data_[i]));
      emptyfn_.MakeEmpty(data_[i]);
    }
  }

  void DeallocateStorage() {
    if (owns_data_) {
      for (size_t i = 0; i < NumBuckets(); ++i) {
        std::allocator_traits<Alloc>::destroy(allocfn_, std::addressof(data_[i]));
      }
      if (data_ != nullptr) {
        allocfn_.deallocate(data_, NumBuckets());
      }
      owns_data_ = false;
    }
    data_ = nullptr;
    num_buckets_ = 0;
  }

  // Expand the set based on the load factors.
  void Expand() {
    size_t min_index = static_cast<size_t>(size() / min_load_factor_);
    // Resize based on the minimum load factor.
    Resize(min_index);
  }

  // Expand / shrink the table to the new specified size.
  void Resize(size_t new_size) {
    if (new_size < kMinBuckets) {
      new_size = kMinBuckets;
    }
    DCHECK_GE(new_size, size());
    T* const old_data = data_;
    size_t old_num_buckets = num_buckets_;
    // Reinsert all of the old elements.
    const bool owned_data = owns_data_;
    AllocateStorage(new_size);
    for (size_t i = 0; i < old_num_buckets; ++i) {
      T& element = old_data[i];
      if (!emptyfn_.IsEmpty(element)) {
        data_[FirstAvailableSlot(IndexForHash(hashfn_(element)))] = std::move(element);
      }
      if (owned_data) {
        std::allocator_traits<Alloc>::destroy(allocfn_, std::addressof(element));
      }
    }
    if (owned_data) {
      allocfn_.deallocate(old_data, old_num_buckets);
    }

    // When we hit elements_until_expand_, we are at the max load factor and must expand again.
    elements_until_expand_ = NumBuckets() * max_load_factor_;
  }

  ALWAYS_INLINE size_t FirstAvailableSlot(size_t index) const {
    DCHECK_LT(index, NumBuckets());  // Don't try to get a slot out of range.
    size_t non_empty_count = 0;
    while (!emptyfn_.IsEmpty(data_[index])) {
      index = NextIndex(index);
      non_empty_count++;
      DCHECK_LE(non_empty_count, NumBuckets());  // Don't loop forever.
    }
    return index;
  }

  size_t NextNonEmptySlot(size_t index) const {
    const size_t num_buckets = NumBuckets();
    DCHECK_LT(index, num_buckets);
    do {
      ++index;
    } while (index < num_buckets && IsFreeSlot(index));
    return index;
  }

  // Return new offset.
  template <typename Elem>
  static size_t WriteToBytes(uint8_t* ptr, size_t offset, Elem n) {
    DCHECK_ALIGNED(ptr + offset, sizeof(n));
    if (ptr != nullptr) {
      *reinterpret_cast<Elem*>(ptr + offset) = n;
    }
    return offset + sizeof(n);
  }

  template <typename Elem>
  static size_t ReadFromBytes(const uint8_t* ptr, size_t offset, Elem* out) {
    DCHECK(ptr != nullptr);
    DCHECK_ALIGNED(ptr + offset, sizeof(*out));
    *out = *reinterpret_cast<const Elem*>(ptr + offset);
    return offset + sizeof(*out);
  }

  Alloc allocfn_;  // Allocator function.
  HashFn hashfn_;  // Hashing function.
  EmptyFn emptyfn_;  // IsEmpty/SetEmpty function.
  Pred pred_;  // Equals function.
  size_t num_elements_;  // Number of inserted elements.
  size_t num_buckets_;  // Number of hash table buckets.
  size_t elements_until_expand_;  // Maximum number of elements until we expand the table.
  bool owns_data_;  // If we own data_ and are responsible for freeing it.
  T* data_;  // Backing storage.
  double min_load_factor_;
  double max_load_factor_;

  template <class Elem, class HashSetType>
  friend class HashSetIterator;

  ART_FRIEND_TEST(InternTableTest, CrossHash);
  ART_FRIEND_TEST(HashSetTest, Preallocated);
};

template <class T, class EmptyFn, class HashFn, class Pred, class Alloc>
void swap(HashSet<T, EmptyFn, HashFn, Pred, Alloc>& lhs,
          HashSet<T, EmptyFn, HashFn, Pred, Alloc>& rhs) {
  lhs.swap(rhs);
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_HASH_SET_H_
