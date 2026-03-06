// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_ELEMENT_IDENTIFIER_H_
#define UI_BASE_INTERACTION_ELEMENT_IDENTIFIER_H_

#include <stdint.h>

#include <concepts>
#include <cstdint>
#include <ostream>
#include <set>

#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/pass_key.h"
#include "ui/base/identifier/unique_identifier.h"

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
//   EXPECT_FALSE(map.contains(id1));
//   ASSERT_EQ(4, map[id2]);
//
// -----
//
// [1] Please note that while operator < will provide a strict ordering, the
//   specific order of two ElementIdentifier constants may vary by build and
//   should not be relied upon; operator < is only provided for compatibility
//   with sorted STL containers.

namespace views {
class ElementTrackerViews;
}

namespace user_education {
class HelpBubbleHandler;
}

namespace ui {

namespace internal {
class ElementIdentifierPropertyCasterHelper;
}

class ElementTracker;

DECLARE_UNIQUE_IDENTIFIER_TYPE(ElementIdentifier,
                               ElementTracker,
                               internal::ElementIdentifierPropertyCasterHelper);

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
class COMPONENT_EXPORT(UI_BASE_INTERACTION) ElementContext {
 public:
  ElementContext() = default;

  // Only specific classes are allowed to be authoritative sources of element
  // contexts. All other code should defer to these classes.
  template <class T, class U>
    requires std::same_as<U, views::ElementTrackerViews> ||
             std::same_as<U, user_education::HelpBubbleHandler>
  explicit ElementContext(T* value, base::PassKey<U>)
      : value_(reinterpret_cast<uintptr_t>(value)) {}

  explicit operator const void*() const {
    return reinterpret_cast<const void*>(value_);
  }
  explicit operator uintptr_t() const { return value_; }
  explicit operator intptr_t() const { return static_cast<intptr_t>(value_); }
  explicit operator bool() const { return value_ != 0; }
  bool operator!() const { return !value_; }
  friend bool operator==(const ElementContext&,
                         const ElementContext&) = default;
  friend auto operator<=>(const ElementContext&,
                          const ElementContext&) = default;

  // Use this to create a fake context for testing. For normal contexts, rely
  // on one of the classes explicitly allowed by `ElementContext(T*)` above.
  template <typename T>
  static ElementContext CreateFakeContextForTesting(T* value) {
    return ElementContext(reinterpret_cast<uintptr_t>(value));
  }
  template <typename T>
    requires std::convertible_to<T, uintptr_t>
  static consteval ElementContext CreateFakeContextForTesting(T value) {
    return ElementContext(static_cast<uintptr_t>(value));
  }

 private:
  explicit constexpr ElementContext(uintptr_t value) : value_(value) {}

  uintptr_t value_ = 0;
};

COMPONENT_EXPORT(UI_BASE_INTERACTION)
extern void PrintTo(ElementContext element_context, std::ostream* os);

COMPONENT_EXPORT(UI_BASE_INTERACTION)
extern std::ostream& operator<<(std::ostream& os,
                                ElementContext element_context);

template <typename T>
class ClassPropertyCaster;

namespace internal {

// Helper for the property caster below.
class ElementIdentifierPropertyCasterHelper {
 private:
  friend class ClassPropertyCaster<ElementIdentifier>;
  static int64_t GetRawValue(ElementIdentifier id) {
    return id.GetRawValue(
        base::PassKey<ElementIdentifierPropertyCasterHelper>());
  }
};

}  // namespace internal

// Required for interoperability with PropertyHandler.
template <>
class ClassPropertyCaster<ElementIdentifier> {
 public:
  static int64_t ToInt64(ElementIdentifier x) {
    return internal::ElementIdentifierPropertyCasterHelper::GetRawValue(x);
  }
  static ElementIdentifier FromInt64(int64_t x) {
    return ElementIdentifier::FromRawValue(base::checked_cast<intptr_t>(x));
  }
};

}  // namespace ui

// Declaring identifiers outside a scope:
//
// Note: if you need to use the identifier outside the current component, use
// DECLARE/DEFINE_EXPORTED_... below.

// Use this code in the .h file to declare a new identifier.
#define DECLARE_ELEMENT_IDENTIFIER_VALUE(IdentifierName) \
  DECLARE_UNIQUE_IDENTIFIER_VALUE(::ui::ElementIdentifier, IdentifierName)

// Use this code in the .cc file to define a new identifier.
#define DEFINE_ELEMENT_IDENTIFIER_VALUE(IdentifierName) \
  DEFINE_UNIQUE_IDENTIFIER_VALUE(::ui::ElementIdentifier, IdentifierName)

// Declaring identifiers that can be used in other components:
//
// Note: unlike other declarations, this identifier will not be constexpr in
// most cases.

// Use this code in the .h file to declare a new exported identifier.
#define DECLARE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(ExportName, IdentifierName) \
  DECLARE_EXPORTED_UNIQUE_IDENTIFIER_VALUE(                                   \
      ExportName, ::ui::ElementIdentifier, IdentifierName)

// Use this code in the .cc file to define a new exported identifier.
#define DEFINE_EXPORTED_ELEMENT_IDENTIFIER_VALUE(IdentifierName)   \
  DEFINE_EXPORTED_UNIQUE_IDENTIFIER_VALUE(::ui::ElementIdentifier, \
                                          IdentifierName)

// Declaring identifiers in a class:

// Use this code in your class declaration in its .h file to declare an
// identifier that is scoped to your class.
#define DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(IdentifierName) \
  DECLARE_CLASS_UNIQUE_IDENTIFIER_VALUE(::ui::ElementIdentifier, IdentifierName)

// Use this code in your class definition .cc file to define the member
// variables
#define DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ClassName, IdentifierName)   \
  DEFINE_CLASS_UNIQUE_IDENTIFIER_VALUE(ClassName, ::ui::ElementIdentifier, \
                                       IdentifierName)

// Declaring local identifiers in functions, class methods, or local to a .cc
// file (often used in tests). File and line are included to guarantee that the
// text of the name generated is unique, though that makes the exact text
// harder to predict.

// Use this code to declare a local identifier in a function body or module
// scope. The name will be mangled with the file and line so that it can be used
// (typically in tests) without having to worry about name collisions.
#define DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(IdentifierName) \
  DEFINE_MACRO_LOCAL_UNIQUE_IDENTIFIER_VALUE(                 \
      __FILE__, __LINE__, ::ui::ElementIdentifier, IdentifierName)

#define DEFINE_MACRO_ELEMENT_IDENTIFIER_VALUE(File, Line, IdentifierName) \
  DEFINE_MACRO_LOCAL_UNIQUE_IDENTIFIER_VALUE(                             \
      __FILE__, __LINE__, ::ui::ElementIdentifier, IdentifierName)

#endif  // UI_BASE_INTERACTION_ELEMENT_IDENTIFIER_H_
