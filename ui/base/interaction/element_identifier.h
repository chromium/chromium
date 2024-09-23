// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_ELEMENT_IDENTIFIER_H_
#define UI_BASE_INTERACTION_ELEMENT_IDENTIFIER_H_

#include <stdint.h>

#include <ostream>
#include <set>

#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/numerics/safe_conversions.h"
#include "ui/base/class_property.h"

// Overview:
// ElementIdentifier provides a named opaque value that can be used to identify
// individual (or potentially groups of) elements in the UI.
//
// Unique identifier constants must be both declared and defined. To create a
// publicly-visible identifier, declare a new unique value in your .h file,
// with the following; the string name of the identifier will be the same as
// the identifier's C++ identifier (in this case, kMyIdentifierName):
//
//   DECLARE_ELEMENT_IDENTIFIER_VALUE(kMyIdentifierName);
//
// If the identifier should be exported, declare it with the following instead:
//
//   DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(MY_EXPORT, kMyIdentifierName);
//
// Regardless of whether the declared identifier is exported or not, make sure
// it is defined in the corresponding .cc file:
//
//   DEFINE_ELEMENT_IDENTIFIER_VALUE(kMyIdentifierName);
//
// If you want to add an identifier as a class member, use the following; the
// string name of the identifier will be in the form
// "MyClass::kMyIdentifierName":
//
//   class MyClass {
//     DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMyIdentifierName);
//   };
//
// Then in the corresponding .cc file, add the following:
//
//   DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MyClass, kMyIdentifierValue);
//
// If you want to create an identifier local to a .cc file or to a method, you
// can instead use the following all-in-one declaration. Note that this is only
// really useful in tests and that the resulting identifier name is mangled
// with the file and line number:
//
//   DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kMyIdentifierName);
//
// That's it! You can now initialize an ElementIdentifier using this value, or
// pass it directly to a method:
//
//   ElementIdentifier my_id = kMyIdentifierName;
//   ElementIdentifier my_id2 = my_id;
//   MyFuncThatTakesAnIdentifier(kMyIdentifierName);
//   MyFuncThatTakesAnIdentifier(my_id2);
//
// ElementIdentifier behaves more or less like other mutable primitive types; it
// is default-constructable (producing a null value) and supports the ==, !=, <,
// !, and (bool) operators as well as assignment and copy [1]. This means you
// can use ElementIdentifier as a key in std::set and std::map, and it is safe
// to use in both DCHECK and test assertions:
//
//   ElementIdentifier id1;
//   ElementIdentifier id2 = kMyIdentifierName;
//   std::map<ElementIdentifier, int> map;
//   map.emplace(id2, 4);
//   DCHECK(!id1);
//   EXPECT_TRUE(static_cast<bool>(id2));
//   DCHECK_NE(id1, id2);
//   EXPECT_FALSE(base::Contains(map, id1));
//   ASSERT_EQ(4, map[id2]);
//
// -----
//
// [1] Please note that while operator < will provide a strict ordering, the
//   specific order of two ElementIdentifier constants may vary by build and
//   should not be relied upon; operator < is only provided for compatibility
//   with sorted STL containers.

namespace ui {

namespace internal {

// Defines the underlying value that an ElementIdentifier holds (namely, the
// address of an instance of this class). Because these objects are only
// declared statically, the value of an ElementIdentifier is always valid and
// two ElementIdentifiers are equal if and only if they hold the address of the
// same instance of this class.
//
// Instances of this object are named for debugging/logging purposes only, the
// value of name() should never be used for any other purpose.
struct ElementIdentifierImpl {
  // The name of the identifier; only used in testing via PrintTo().
  const char* const name;
};

}  // namespace internal

class ElementTracker;

// Holds a globally-unique, value-typed identifier from a set of identifiers
// which can be declared in any static scope.
//
// This type is comparable and supports operator bool and negation, where
// default-constructed instances have false value and all other values evaluate
// as true. It can also be used as the key in std::set, std::map, and similar
// collections.
class COMPONENT_EXPORT(UI_BASE) ElementIdentifier final {
 public:
  // Creates a null identifier.
  constexpr ElementIdentifier() = default;

  // Avoid this constructor - it is used internally by the
  // DECLARE_ELEMENT_IDENTIFIER_VALUE() macro.
  explicit constexpr ElementIdentifier(
      const internal::ElementIdentifierImpl* provider)
      : handle_(provider) {}

  constexpr explicit operator bool() const { return handle_ != nullptr; }

  constexpr bool operator!() const { return !handle_; }

  constexpr bool operator==(const ElementIdentifier& other) const = default;

  // TODO(crbug.com/333028921): Operator < cannot be constexpr because memory
  // order of Impl objects is not strictly known at compile time. Fix this...
  // somehow? Possibilities include compile-time hashing of identifier string.
  bool operator<(const ElementIdentifier& other) const {
    return handle_ < other.handle_;
  }

  // Retrieves the element name, or the empty string if none.
  std::string GetName() const;

  // Retrieve a known ElementIdentifier by name. An ElementIdentifier is *known*
  // if a TrackedElement has been created with the id, or if the value of the
  // identifier has been serialized using GetRawValue() or GetName().
  static ElementIdentifier FromName(const char* name);

  // Included for interoperability with PropertyHandler. Retrieves an element
  // identifier from the result of calling GetRawValue(). The `value` passed in
  // MUST either have been generated by calling GetRawValue() or be zero (this
  // is strictly enforced even in release builds).
  static ElementIdentifier FromRawValue(intptr_t value);

 private:
  using KnownIdentifiers = std::set<const internal::ElementIdentifierImpl*>;

  friend class ClassPropertyCaster<ElementIdentifier>;
  friend class ElementTracker;
  friend class ElementIdentifierTest;
  friend class ElementTrackerIdentifierTest;
  friend COMPONENT_EXPORT(UI_BASE) void PrintTo(
      ElementIdentifier element_identifier,
      std::ostream* os);

  // Included for interoperability with PropertyHandler.
  intptr_t GetRawValue() const;

  // Registers a non-null identifier as known. Has no effect if the element is
  // already registered.
  static void RegisterKnownIdentifier(ElementIdentifier element_dentifier);

  // Returns the singleton set of known identifiers.
  static KnownIdentifiers& GetKnownIdentifiers();

  // The value of the identifier. Because all non-null values point to static
  // ElementIdentifierImpl objects this can be treated as a value from a set of
  // unique, opaque handles.
  // RAW_PTR_EXCLUSION: #union, #global-scope
  RAW_PTR_EXCLUSION const internal::ElementIdentifierImpl* handle_ = nullptr;
};

// The context of an element is unique to the top-level, primary window that the
// element is associated with. Elements in secondary UI (bubbles, menus,
// drop-downs, etc.) will be associated with the same context as elements in the
// primary window itself.
//
// The value used should be consistent across a toolkit and unique between
// primary windows; a memory address or handle of the window object can
// typically be used (e.g. in Views, we use the address of the primary window's
// Widget). A zero or null value should always correspond to "no context".
//
// Please note that you must consistently use the same pointer or handle type
// across your framework when creating contexts; because of the vagaries of C++
// up- and down-casting (especially with multiple inheritance) constructing an
// ElementContext from different pointer types can produce different results,
// even for the same underlying object.
//
// ElementContext objects are assignable, have boolean value based on whether
// the underlying value is null, and support operator < for use in maps and
// sets.
class COMPONENT_EXPORT(UI_BASE) ElementContext {
 public:
  ElementContext() = default;

  template <class T>
  explicit ElementContext(T* value)
      : value_(reinterpret_cast<uintptr_t>(value)) {}

  template <class T>
  explicit ElementContext(T value) : value_(static_cast<uintptr_t>(value)) {}

  explicit operator const void*() const {
    return reinterpret_cast<const void*>(value_);
  }
  explicit operator uintptr_t() const { return value_; }
  explicit operator intptr_t() const { return static_cast<intptr_t>(value_); }
  explicit operator bool() const { return value_ != 0; }
  bool operator!() const { return !value_; }
  bool operator==(const ElementContext& other) const {
    return value_ == other.value_;
  }
  bool operator!=(const ElementContext& other) const {
    return value_ != other.value_;
  }
  bool operator<(const ElementContext& other) const {
    return value_ < other.value_;
  }

 private:
  uintptr_t value_ = 0;
};

COMPONENT_EXPORT(UI_BASE)
extern void PrintTo(ElementIdentifier element_identifier, std::ostream* os);

COMPONENT_EXPORT(UI_BASE)
extern void PrintTo(ElementContext element_context, std::ostream* os);

COMPONENT_EXPORT(UI_BASE)
extern std::ostream& operator<<(std::ostream& os,
                                ElementIdentifier element_identifier);

COMPONENT_EXPORT(UI_BASE)
extern std::ostream& operator<<(std::ostream& os,
                                ElementContext element_context);

// Required for interoperability with PropertyHandler.
template <>
class ClassPropertyCaster<ui::ElementIdentifier> {
 public:
  static int64_t ToInt64(ui::ElementIdentifier x) { return x.GetRawValue(); }
  static ui::ElementIdentifier FromInt64(int64_t x) {
    return ui::ElementIdentifier::FromRawValue(base::checked_cast<intptr_t>(x));
  }
};

}  // namespace ui

// Declaring identifiers outside a scope:
//
// Note: if you need to use the identifier outside the current component, use
// DECLARE/DEFINE_EXPORTED_... below.

// Use this code in the .h file to declare a new identifier.
#define DECLARE_ELEMENT_IDENTIFIER_VALUE(IdentifierName)                     \
  extern const ui::internal::ElementIdentifierImpl IdentifierName##Provider; \
  inline constexpr ui::ElementIdentifier IdentifierName(                     \
      &IdentifierName##Provider)

// Use this code in the .cc file to define a new identifier.
#define DEFINE_ELEMENT_IDENTIFIER_VALUE(IdentifierName)                \
  const ui::internal::ElementIdentifierImpl IdentifierName##Provider { \
    #IdentifierName                                                    \
  }

// Declaring identifiers that can be used in other components:
//
// Note: unlike other declarations, this identifier will not be constexpr in
// most cases.

// Use this code in the .h file to declare a new exported identifier.
#define DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ExportName, IdentifierName) \
  ExportName extern const ui::internal::ElementIdentifierImpl                 \
      IdentifierName##Provider;                                               \
  ExportName extern const ui::ElementIdentifier IdentifierName

// Use this code in the .cc file to define a new exported identifier.
#define DEFINE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(IdentifierName)      \
  const ui::internal::ElementIdentifierImpl IdentifierName##Provider{ \
      #IdentifierName};                                               \
  const ui::ElementIdentifier IdentifierName(&IdentifierName##Provider)

// Declaring identifiers in a class:

// Use this code in your class declaration in its .h file to declare an
// identifier that is scoped to your class.
#define DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(IdentifierName)               \
  static const ui::internal::ElementIdentifierImpl IdentifierName##Provider; \
  static constexpr ui::ElementIdentifier IdentifierName {             \
    &IdentifierName##Provider                                                \
  }

// Use this code in your class definition .cc file to define the member
// variables
#define DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ClassName, IdentifierName)    \
  const ui::internal::ElementIdentifierImpl                                 \
      ClassName::IdentifierName##Provider{#ClassName "::" #IdentifierName}; \
  constexpr ui::ElementIdentifier ClassName::IdentifierName

// Declaring local identifiers in functions, class methods, or local to a .cc
// file (often used in tests). File and line are included to guarantee that the
// text of the name generated is unique, though that makes the exact text
// harder to predict.

// This helper macro is required because of how __LINE__ is handled when passed
// between macros, you need an intermediate macro in order to stringify it.
// DO NOT CALL DIRECTLY; used by DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE().
#define LOCAL_ELEMENT_IDENTIFIER_NAME(File, Line, Name) \
  File "::" #Line "::" #Name

// Use this code to declare a local identifier from within a macro; you should
// pass the __FILE__ and __LINE__ values for `File` and `Line`. The name will be
// mangled with the file and line so that it can be used in local or module
// scope (typically in tests) without having to worry about name collisions.
#define DEFINE_MACRO_ELEMENT_IDENTIFIER_VALUE(File, Line, IdentifierName) \
  static constexpr ui::internal::ElementIdentifierImpl                    \
      IdentifierName##Provider{                                           \
          LOCAL_ELEMENT_IDENTIFIER_NAME(File, Line, IdentifierName)};     \
  constexpr ui::ElementIdentifier IdentifierName(&IdentifierName##Provider)

// Use this code to declare a local identifier in a function body or module
// scope. The name will be mangled with the file and line so that it can be used
// (typically in tests) without having to worry about name collisions.
#define DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(IdentifierName) \
  DEFINE_MACRO_ELEMENT_IDENTIFIER_VALUE(__FILE__, __LINE__, IdentifierName)

#endif  // UI_BASE_INTERACTION_ELEMENT_IDENTIFIER_H_
