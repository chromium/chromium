// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_FRAMEWORK_SPECIFIC_REGISTRATION_LIST_H_
#define UI_BASE_INTERACTION_FRAMEWORK_SPECIFIC_REGISTRATION_LIST_H_

#include <concepts>
#include <memory>
#include <vector>

#include "ui/base/interaction/framework_specific_implementation.h"

namespace ui {

// Holds a list of framework-specific singletons, which all derive from
// `BaseClass` (which must in turn be derived from
// FrameworkSpecificImplementation) and which must be default-constructible.
//
// Semantically, this behaves as a very simple ordered STL collection of
// `BaseClass`. Insertion is only done via MaybeRegister().
template <class BaseClass>
  requires std::derived_from<BaseClass, FrameworkSpecificImplementation>
class FrameworkSpecificRegistrationList {
 public:
  using ListType = std::vector<std::unique_ptr<BaseClass>>;

  // Implements a simple forward iterator over instances.
  template <class It = typename ListType::iterator>
  class Iterator {
   public:
    Iterator() = default;
    ~Iterator() = default;
    Iterator(const Iterator&) = default;
    Iterator& operator=(const Iterator&) = default;

    BaseClass& operator*() { return *(it_->get()); }
    BaseClass* operator->() { return it_->get(); }
    Iterator& operator++() {
      ++it_;
      return *this;
    }
    Iterator operator++(int) { return Iterator(it_++); }
    friend bool operator==(const Iterator&, const Iterator&) = default;

   private:
    friend class FrameworkSpecificRegistrationList<BaseClass>;
    explicit Iterator(It it) : it_(it) {}

    It it_;
  };

  // STL collection properties.
  using value_type = BaseClass;
  using reference = BaseClass&;
  using pointer = BaseClass*;
  using iterator = Iterator<>;
  using const_iterator = Iterator<typename ListType::const_iterator>;
  using reverse_iterator = Iterator<typename ListType::reverse_iterator>;
  using const_reverse_iterator =
      Iterator<typename ListType::const_reverse_iterator>;
  using size_type = typename ListType::size_type;

  FrameworkSpecificRegistrationList() = default;
  ~FrameworkSpecificRegistrationList() = default;

  // STL collection implementation.
  size_type size() const { return instances_.size(); }
  iterator begin() { return iterator(instances_.begin()); }
  iterator end() { return iterator(instances_.end()); }
  reverse_iterator rbegin() { return reverse_iterator(instances_.rbegin()); }
  reverse_iterator rend() { return reverse_iterator(instances_.rend()); }
  const_iterator begin() const { return const_iterator(instances_.begin()); }
  const_iterator end() const { return const_iterator(instances_.end()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(instances_.rbegin());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(instances_.rend());
  }
  BaseClass& operator[](size_type index) { return *instances_[index]; }
  const BaseClass& operator[](size_type index) const {
    return *instances_[index];
  }

  // Adds an instance of `DerivedClass` if it is not already present.
  // Additional arguments in `params` will only be consumed if the class needs
  // to be added, so do not allocate resources that are not scoped and movable
  // (i.e. pass a std::unique_ptr rather than "new X", so the object will be
  // properly cleaned up if it is not used).
  //
  // Returns the object of the specified type if created, or null if already
  // present (you can still use GetImplementation() to retrieve it).
  template <class DerivedClass, typename... Args>
    requires std::derived_from<DerivedClass, BaseClass>
  DerivedClass* MaybeRegister(Args&&... args) {
    if (auto* derived = GetImplementation<DerivedClass>()) {
      return nullptr;
    }
    auto ptr = std::make_unique<DerivedClass>(std::forward<Args>(args)...);
    DerivedClass* const result = ptr.get();
    instances_.push_back(std::move(ptr));
    return result;
  }

  // Retrieves a particular implementation by type, or null if none is
  // registered. Uses the `IsA` relation to determine if `DerivedClass` is
  // present.
  template <class DerivedClass>
    requires std::derived_from<DerivedClass, BaseClass>
  DerivedClass* GetImplementation() {
    for (const auto& instance : instances_) {
      if (instance->template IsA<DerivedClass>()) {
        return static_cast<DerivedClass*>(instance.get());
      }
    }
    return nullptr;
  }

 private:
  ListType instances_;
};

}  // namespace ui

#endif  // UI_BASE_INTERACTION_FRAMEWORK_SPECIFIC_REGISTRATION_LIST_H_
