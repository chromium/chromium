// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_SAFE_CASTABLE_H_
#define UI_BASE_INTERACTION_SAFE_CASTABLE_H_

#include <ostream>
#include <string>

#include "base/component_export.h"
#include "ui/base/identifier/unique_identifier.h"

namespace ui {

// Provides very simple RTTI so that we can retrieve a specific implementations
// (e.g. from platform-agnostic systems such as ElementTracker). The
// implementations should follow the inheritance hierarchy but not every class
// in the inheritance chain need participate.
//
// To use this class, implement a base class for your implementations:
//
//   class Thing : public SafeCastable {
//     ~Thing() override;  // optional, include if class has local data
//     // <-- interface methods and common implementation here
//   };
//
// Then, in your implementation .h file:
//
//   class ThingImplementation : public Thing {
//    public:
//     ~ThingImplementation() override;
//     DECLARE_SAFE_CAST_TARGET()
//   };
//
// In the corresponding .cc file:
//
//   DEFINE_SAFE_CAST_TARGET(ThingInMyFramework)
//
// If you want to have a derived class that also reports as one of its ancestor
// classes, instead use this:
//
//   DEFINE_SAFE_CAST_SUBCLASS(
//       SubclassImplementation, SuperclassImplementation)
//
// In this case, SubclassImplementation::IsA<SuperclassImplemnentation>() will
// return true. The superclass must also DECLARE_SAFE_CAST_TARGET().
//
// This is transitive, so if C is declared as a implementation subclass of B,
// and B of A, then C::IsA<A>() will return true.
//
// While the subclass must be a descendant of the superclass by inheritance,
// not every intermediate class need be registered. Furthermore, inheritance is
// not automatic; DEFINE_SAFE_CAST_SUBCLASS() is required to
// establish the association.
class COMPONENT_EXPORT(UI_BASE_INTERACTION) SafeCastable {
 public:
  // Used by IsA() and AsA() methods to do runtime type-checking.
  DECLARE_UNIQUE_IDENTIFIER_TYPE(SafeCastTargetIdentifier);

  SafeCastable() = default;
  SafeCastable(const SafeCastable&) = delete;
  virtual ~SafeCastable() = default;
  void operator=(const SafeCastable&) = delete;

  // Returns whether this object is a specific subtype - for example, whether a
  // `TrackedElement` is a `views::TrackedElementViews`.
  template <typename T>
  bool IsA() const {
    return AsA<T>();
  }

  // Dynamically casts this object to a specific subtype, returning null if the
  // element is the wrong type. This version converts non-const objects.
  template <typename T>
  T* AsA() {
    return CheckImplementationHierarchy(T::GetSafeCastTargetIdentifier())
               ? static_cast<T*>(this)
               : nullptr;
  }

  // Dynamically casts this object to a specific subtype, returning null if the
  // object is the wrong type. This version converts const objects.
  template <typename T>
  const T* AsA() const {
    return CheckImplementationHierarchy(T::GetSafeCastTargetIdentifier())
               ? static_cast<const T*>(this)
               : nullptr;
  }

  // Gets the class name of the implementation.
  virtual const char* GetSafeCastableClassName() const = 0;

  // Gets a string representation of this element.
  virtual std::string ToString() const;

 protected:
  // Checks that `id` corresponds to something in this class' hierarchy.
  // Use DECLARE/DEFINE_SAFE_CAST_TARGET() - see below - to
  // implement this method in your framework-specific derived classes.
  virtual bool CheckImplementationHierarchy(
      SafeCastTargetIdentifier id) const = 0;
};

// These macros can be used to help define platform-specific subclasses of
// base classes derived from SafeCastable.

// Put this at the top of the class declaration, in the public section.
#define DECLARE_SAFE_CAST_TARGET()                               \
  const char* GetSafeCastableClassName() const override;         \
  static SafeCastTargetIdentifier GetSafeCastTargetIdentifier(); \
  bool CheckImplementationHierarchy(SafeCastTargetIdentifier) const override;

// This is used internally; don't use it directly.
#define _DEFINE_SAFE_CAST_COMMON(ClassName)                             \
  const char* ClassName::GetSafeCastableClassName() const {             \
    return #ClassName;                                                  \
  }                                                                     \
  ui::SafeCastable::SafeCastTargetIdentifier                            \
  ClassName::GetSafeCastTargetIdentifier() {                            \
    DEFINE_MACRO_LOCAL_UNIQUE_IDENTIFIER_VALUE(                         \
        __FILE__, __LINE__, ui::SafeCastable::SafeCastTargetIdentifier, \
        k##ClassName##Identifier);                                      \
    return k##ClassName##Identifier;                                    \
  }

// Use to define an implementation that will only report as itself. Put this in
// `ClassName`'s .cc file.
#define DEFINE_SAFE_CAST_TARGET(ClassName)                                  \
  _DEFINE_SAFE_CAST_COMMON(ClassName)                                       \
  bool ClassName::CheckImplementationHierarchy(SafeCastTargetIdentifier id) \
      const {                                                               \
    return id == GetSafeCastTargetIdentifier();                             \
  }

// Use to define an implementation that will report as both itself and as
// `BaseClassName`. Put this in `ClassName`'s .cc file.
#define DEFINE_SAFE_CAST_SUBCLASS(ClassName, BaseClassName)                 \
  static_assert(std::derived_from<ClassName, BaseClassName>);               \
  _DEFINE_SAFE_CAST_COMMON(ClassName)                                       \
  bool ClassName::CheckImplementationHierarchy(SafeCastTargetIdentifier id) \
      const {                                                               \
    return id == GetSafeCastTargetIdentifier() ||                           \
           BaseClassName::CheckImplementationHierarchy(id);                 \
  }

COMPONENT_EXPORT(UI_BASE_INTERACTION)
extern void PrintTo(const SafeCastable& impl, std::ostream* os);

COMPONENT_EXPORT(UI_BASE_INTERACTION)
extern std::ostream& operator<<(std::ostream& os, const SafeCastable& impl);

}  // namespace ui

#endif  // UI_BASE_INTERACTION_SAFE_CASTABLE_H_
