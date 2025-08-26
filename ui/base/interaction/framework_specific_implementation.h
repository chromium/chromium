// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_FRAMEWORK_SPECIFIC_IMPLEMENTATION_H_
#define UI_BASE_INTERACTION_FRAMEWORK_SPECIFIC_IMPLEMENTATION_H_

#include <ostream>
#include <string>

#include "base/component_export.h"
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
//     // <-- interface methods and common implementation here
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
// If you want to have a derived class that also reports as one of its ancestor
// classes, instead use this:
//
//   DEFINE_FRAMEWORK_SPECIFIC_METADATA_SUBCLASS(
//       SubclassInMyFramework, SuperclassInMyFramework)
//
// In this case, SubclassInMyFramework::IsA<SuperclassInMyFramework>() will
// return true. The superclass must also DECLARE_FRAMEWORK_SPECIFIC_METADATA().
//
// This is transitive, so if C is declared as a framework specific subclass of
// B, and B of A, then C::IsA<A>() will return true.
//
// While the subclass must be a descendant of the superclass by inheritance,
// not every intermediate class need be registered. Furthermore, inheritance is
// not automatic; DEFINE_FRAMEWORK_SPECIFIC_METADATA_SUBCLASS() is required to
// establish the association.
class COMPONENT_EXPORT(UI_BASE_INTERACTION) FrameworkSpecificImplementation {
 public:
  // Used by IsA() and AsA() methods to do runtime type-checking.
  using FrameworkIdentifier = ElementIdentifier;

  FrameworkSpecificImplementation() = default;
  FrameworkSpecificImplementation(const FrameworkSpecificImplementation&) =
      delete;
  virtual ~FrameworkSpecificImplementation() = default;
  void operator=(const FrameworkSpecificImplementation&) = delete;

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
    return CheckInstanceFrameworkHierarchy(T::GetFrameworkIdentifier())
               ? static_cast<T*>(this)
               : nullptr;
  }

  // Dynamically casts this object to a specific subtype, returning null if the
  // object is the wrong type. This version converts const objects.
  template <typename T>
  const T* AsA() const {
    return CheckInstanceFrameworkHierarchy(T::GetFrameworkIdentifier())
               ? static_cast<const T*>(this)
               : nullptr;
  }

  // Gets the class name of the implementation.
  virtual const char* GetImplementationName() const = 0;

  // Gets a string representation of this element.
  virtual std::string ToString() const;

 protected:
  // Checks that `id` corresponds to something in this class' hierarchy.
  // Use DECLARE/DEFINE_FRAMEWORK_SPECIFIC_METADATA() - see below - to
  // implement this method in your framework-specific derived classes.
  virtual bool CheckInstanceFrameworkHierarchy(
      FrameworkIdentifier id) const = 0;
};

// These macros can be used to help define platform-specific subclasses of
// base classes derived from FrameworkSpecificImplementation.

// Put this at the top of the class declaration, in the public section.
#define DECLARE_FRAMEWORK_SPECIFIC_METADATA()          \
  const char* GetImplementationName() const override;  \
  static FrameworkIdentifier GetFrameworkIdentifier(); \
  bool CheckInstanceFrameworkHierarchy(FrameworkIdentifier) const override;

// This is used internally; don't use it directly.
#define DEFINE_FRAMEWORK_SPECIFIC_METADATA_COMMON(ClassName)         \
  const char* ClassName::GetImplementationName() const {             \
    return #ClassName;                                               \
  }                                                                  \
  ui::FrameworkSpecificImplementation::FrameworkIdentifier           \
  ClassName::GetFrameworkIdentifier() {                              \
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(k##ClassName##Identifier); \
    return k##ClassName##Identifier;                                 \
  }

// Use to define an implementation that will only report as itself. Put this in
// `ClassName`'s .cc file.
#define DEFINE_FRAMEWORK_SPECIFIC_METADATA(ClassName)                     \
  DEFINE_FRAMEWORK_SPECIFIC_METADATA_COMMON(ClassName)                    \
  bool ClassName::CheckInstanceFrameworkHierarchy(FrameworkIdentifier id) \
      const {                                                             \
    return id == GetFrameworkIdentifier();                                \
  }

// Use to define an implementation that will report as both itself and as
// `BaseClassName`. Put this in `ClassName`'s .cc file.
#define DEFINE_FRAMEWORK_SPECIFIC_METADATA_SUBCLASS(ClassName, BaseClassName) \
  static_assert(std::derived_from<ClassName, BaseClassName>);                 \
  DEFINE_FRAMEWORK_SPECIFIC_METADATA_COMMON(ClassName)                        \
  bool ClassName::CheckInstanceFrameworkHierarchy(FrameworkIdentifier id)     \
      const {                                                                 \
    return id == GetFrameworkIdentifier() ||                                  \
           BaseClassName::CheckInstanceFrameworkHierarchy(id);                \
  }

COMPONENT_EXPORT(UI_BASE_INTERACTION)
extern void PrintTo(const FrameworkSpecificImplementation& impl,
                    std::ostream* os);

COMPONENT_EXPORT(UI_BASE_INTERACTION)
extern std::ostream& operator<<(std::ostream& os,
                                const FrameworkSpecificImplementation& impl);

}  // namespace ui

#endif  // UI_BASE_INTERACTION_FRAMEWORK_SPECIFIC_IMPLEMENTATION_H_
