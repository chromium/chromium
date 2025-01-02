// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <type_traits>

namespace onnxruntime {
/**
   Container has T* entries. e.g. std::vector<T*>, and this class provides const access to those
   via iterators and direct access, as the standard behavior only makes the pointer constant,
   and not what is pointed too. i.e. you get a const pointer to T not a pointer to const T without this wrapper.
   See https://stackoverflow.com/questions/8017036/understanding-const-iterator-with-pointers
*/
template <typename Container>
class ConstPointerContainer {
 public:
  using T = typename std::remove_pointer<typename Container::value_type>::type;

  class ConstIterator {
   public:
    using const_iterator = typename Container::const_iterator;
    using iterator_category = std::input_iterator_tag;
    using value_type = T*;
    using difference_type = std::ptrdiff_t;
    using pointer = T**;
    using reference = T*&;

    /** Construct iterator for container that will return const T* entries.*/
    explicit ConstIterator(const_iterator position) noexcept : current_{position}, item_{nullptr} {}
    ConstIterator(const ConstIterator& other) = default;
    ConstIterator& operator=(const ConstIterator& other) = default;

    bool operator==(const ConstIterator& other) const noexcept { return current_ == other.current_; }
    bool operator!=(const ConstIterator& other) const noexcept { return current_ != other.current_; }

    ConstIterator& operator++() {
      ++current_;
      return *this;
    }

    ConstIterator operator++(int) {
      ConstIterator tmp{*this};
      ++(*this);
      return tmp;
    }

    const T*& operator*() const {
      item_ = *current_;
      return item_;
    }

    const T** operator->() const { return &(operator*()); };

   private:
    const_iterator current_;
    mutable const T* item_;
  };

  /**
     Construct wrapper class that will provide const access to the pointers in a container of non-const pointers.
     @param data Container with non-const pointers. e.g. std::vector<T*>
  */
  explicit ConstPointerContainer(const Container& data) noexcept : data_(data) {}

  size_t size() const noexcept { return data_.size(); }
  bool empty() const noexcept { return data_.empty(); }

  ConstIterator cbegin() const noexcept { return ConstIterator(data_.cbegin()); }
  ConstIterator cend() const noexcept { return ConstIterator(data_.cend()); }

  ConstIterator begin() const noexcept { return ConstIterator(data_.cbegin()); }
  ConstIterator end() const noexcept { return ConstIterator(data_.cend()); }

  const T* operator[](size_t index) const { return data_[index]; }

  const T* at(size_t index) const {
    ORT_ENFORCE(index < data_.size());
    return data_[index];
  }

 private:
  const Container& data_;
};
}  // namespace onnxruntime
