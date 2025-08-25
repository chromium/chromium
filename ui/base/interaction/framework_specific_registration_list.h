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
  // Implements a simple forward iterator over instances.
  template <class T>
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
    friend class FrameworkSpecificRegistrationList<T>;
    explicit Iterator(
        typename FrameworkSpecificRegistrationList<T>::ListType::iterator it)
        : it_(it) {}

    typename FrameworkSpecificRegistrationList<T>::ListType::iterator it_;
  };

  // STL collection properties.
  using ListType = std::vector<std::unique_ptr<BaseClass>>;
  using value_type = BaseClass;
  using reference = BaseClass&;
  using pointer = BaseClass*;
  using iterator = Iterator<BaseClass>;
  using size_type = typename ListType::size_type;

  FrameworkSpecificRegistrationList() = default;
  ~FrameworkSpecificRegistrationList() = default;

  // STL collection implementation.
  size_type size() const { return instances_.size(); }
  iterator begin() { return Iterator<BaseClass>(instances_.begin()); }
  iterator end() { return Iterator<BaseClass>(instances_.end()); }
  BaseClass& operator[](size_type index) { return *instances_[index]; }
  const BaseClass& operator[](size_type index) const {
    return *instances_[index];
  }

  // Adds an instance of `DerivedClass` if it is not already present.
  // Additional arguments in `params` will only be consumed if the class needs
  // to be added, so do not allocate resources that are not scoped and movable
  // (i.e. pass a std::unique_ptr rather than "new X", so the object will be
  // properly cleaned up if it is not used).
  template <class DerivedClass, typename... Args>
  void MaybeRegister(Args&&... args) {
    for (const auto& instance : instances_) {
      if (instance->template IsA<DerivedClass>()) {
        return;
      }
    }
    instances_.push_back(
        std::make_unique<DerivedClass>(std::forward<Args>(args)...));
  }

 private:
  ListType instances_;
};

}  // namespace ui

#endif  // UI_BASE_INTERACTION_FRAMEWORK_SPECIFIC_REGISTRATION_LIST_H_
