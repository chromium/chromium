// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_FRAMEWORK_SPECIFIC_IMPLEMENTATION_H_
#define UI_BASE_INTERACTION_FRAMEWORK_SPECIFIC_IMPLEMENTATION_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {

// Represents a type that has different implementations in different frameworks.
// This provides a very simple RTTI implementation so that we can retrieve
// platform-specific implementations from platform-agnostic systems (such as
// ElementTracker).
//
// To use this class, implement a base class for your implementations:
//
//   class ThingBase : public FrameworkSpecificImplementation {
//     ~ThingBase() override;  // optional, include if class has local data
//     // <-- common implementation here
//   };
//
// Then, in your framework-specific .h file:
//
//   class ThingInMyFramework : public ThingBase {
//    public:
//     ~ThingInMyFramework() override;
//     DECLARE_FRAMEWORK_SPECIFIC_METADATA()
//   };
//
// In the corresponding .cc file:
//
//   DEFINE_FRAMEWORK_SPECIFIC_METADATA(ThingInMyFramework)
//
// For factory classes or other singleton-type classes that need to be listed
// in a registry, use FrameworkSpecificRegistrationList<ThingBase> (see below).
//
class COMPONENT_EXPORT(UI_BASE) FrameworkSpecificImplementation {
 public:
  // Used by IsA() and AsA() methods to do runtime type-checking.
  using FrameworkIdentifier = ElementIdentifier;

  FrameworkSpecificImplementation() = default;
  FrameworkSpecificImplementation(const FrameworkSpecificImplementation&) =
      delete;
  virtual ~FrameworkSpecificImplementation() = default;
  void operator=(const FrameworkSpecificImplementation&) = delete;

  // Returns whether this element is a specific subtype - for example, a
  // views::ViewsTrackedElement.
  template <typename T>
  bool IsA() const {
    return AsA<T>();
  }

  // Dynamically casts this element to a specific subtype, such as a
  // views::ViewsTrackedElement, returning null if the element is the
  // wrong type.
  template <typename T>
  T* AsA() {
    return GetInstanceFrameworkIdentifier() == T::GetFrameworkIdentifier()
               ? static_cast<T*>(this)
               : nullptr;
  }

  // Dynamically casts this element to a specific subtype, such as a
  // views::ViewsTrackedElement, returning null if the element is the
  // wrong type. This version converts const objects.
  template <typename T>
  const T* AsA() const {
    return GetInstanceFrameworkIdentifier() == T::GetFrameworkIdentifier()
               ? static_cast<const T*>(this)
               : nullptr;
  }

  // Gets the class name of the implementation.
  virtual const char* GetImplementationName() const = 0;

  // Gets a string representation of this element.
  virtual std::string ToString() const;

 protected:
  // Use DECLARE/DEFINE_FRAMEWORK_SPECIFIC_METADATA() - see below - to
  // implement this method in your framework-specific derived classes.
  virtual FrameworkIdentifier GetInstanceFrameworkIdentifier() const = 0;
};

// These macros can be used to help define platform-specific subclasses of
// base classes derived from FrameworkSpecificImplementation.
#define DECLARE_FRAMEWORK_SPECIFIC_METADATA()          \
  const char* GetImplementationName() const override;  \
  static FrameworkIdentifier GetFrameworkIdentifier(); \
  FrameworkIdentifier GetInstanceFrameworkIdentifier() const override;
#define DEFINE_FRAMEWORK_SPECIFIC_METADATA(ClassName)                \
  const char* ClassName::GetImplementationName() const {             \
    return #ClassName;                                               \
  }                                                                  \
  ui::FrameworkSpecificImplementation::FrameworkIdentifier           \
  ClassName::GetFrameworkIdentifier() {                              \
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(k##ClassName##Identifier); \
    return k##ClassName##Identifier;                                 \
  }                                                                  \
  ui::FrameworkSpecificImplementation::FrameworkIdentifier           \
  ClassName::GetInstanceFrameworkIdentifier() const {                \
    return GetFrameworkIdentifier();                                 \
  }

// Holds a list of framework-specific singletons, which all derive from
// `BaseClass` (which must in turn be derived from
// FrameworkSpecificImplementation) and which must be default-constructible.
//
// Semantically, this behaves as a very simple ordered STL collection of
// `BaseClass`. Insertion is only done via MaybeRegister().
template <class BaseClass>
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
    bool operator==(const Iterator& other) const { return it_ == other.it_; }
    bool operator!=(const Iterator& other) const { return it_ != other.it_; }

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
      if (instance->template IsA<DerivedClass>())
        return;
    }
    instances_.push_back(
        std::make_unique<DerivedClass>(std::forward<Args>(args)...));
  }

 private:
  ListType instances_;
};

COMPONENT_EXPORT(UI_BASE)
extern void PrintTo(const FrameworkSpecificImplementation& impl,
                    std::ostream* os);

COMPONENT_EXPORT(UI_BASE)
extern std::ostream& operator<<(std::ostream& os,
                                const FrameworkSpecificImplementation& impl);

}  // namespace ui

#endif  // UI_BASE_INTERACTION_FRAMEWORK_SPECIFIC_IMPLEMENTATION_H_
