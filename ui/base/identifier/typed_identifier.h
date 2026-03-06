// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IDENTIFIER_TYPED_IDENTIFIER_H_
#define UI_BASE_IDENTIFIER_TYPED_IDENTIFIER_H_

#include "ui/base/identifier/unique_identifier.h"

namespace ui {

// Unique identifier that adds type information to an untyped identifier type.
// See README.md for more detailed usage instructions.
//
// Use the DECLARE/DEFINE macros below to create unique typed identifiers.
template <typename U, typename T>
  requires internal::IsUniqueIdentifierImpl<U>
class TypedIdentifier final {
 public:
  using Type = T;
  using UntypedIdentifier = U;

  constexpr TypedIdentifier() = default;

  explicit constexpr TypedIdentifier(UntypedIdentifier identifier)
      : identifier_(identifier) {}

  constexpr UntypedIdentifier identifier() const { return identifier_; }

  constexpr explicit operator bool() const {
    return static_cast<bool>(identifier_);
  }

  constexpr bool operator!() const { return !identifier_; }

  friend constexpr bool operator==(const TypedIdentifier<U, T>&,
                                   const TypedIdentifier<U, T>&) = default;
  friend constexpr auto operator<=>(const TypedIdentifier<U, T>&,
                                    const TypedIdentifier<U, T>&) = default;

 private:
  UntypedIdentifier identifier_;
};

template <typename U, typename T>
extern void PrintTo(TypedIdentifier<U, T> identifier, std::ostream* os) {
  *os << "TypedIdentifier [" << identifier.identifier().GetName() << "]";
}

template <typename U, typename T>
extern std::ostream& operator<<(std::ostream& os,
                                TypedIdentifier<U, T> identifier) {
  PrintTo(identifier, &os);
  return os;
}

}  // namespace ui

// The following macros create a typed identifier value, and mimic the similar
// macros for ElementIdentifier, except that they also include a type.

#define DECLARE_TYPED_IDENTIFIER_VALUE(UntypedIdentifier, Type, Name) \
  DECLARE_UNIQUE_IDENTIFIER_VALUE(UntypedIdentifier, Name##Impl);     \
  extern const ::ui::TypedIdentifier<UntypedIdentifier, Type> Name

#define DEFINE_TYPED_IDENTIFIER_VALUE(UntypedIdentifier, Type, Name) \
  DEFINE_UNIQUE_IDENTIFIER_VALUE(UntypedIdentifier, Name##Impl);     \
  constexpr ::ui::TypedIdentifier<UntypedIdentifier, Type> Name(Name##Impl)

#define DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(UntypedIdentifier, Type, Name) \
  DECLARE_CLASS_UNIQUE_IDENTIFIER_VALUE(UntypedIdentifier, Name##Impl);     \
  static constexpr ::ui::TypedIdentifier<UntypedIdentifier, Type> Name {    \
    Name##Impl                                                              \
  }

#define DEFINE_CLASS_TYPED_IDENTIFIER_VALUE(Class, UntypedIdentifier, Type,   \
                                            Name)                             \
  DEFINE_CLASS_UNIQUE_IDENTIFIER_VALUE(Class, UntypedIdentifier, Name##Impl); \
  constexpr ::ui::TypedIdentifier<UntypedIdentifier, Type> Class::Name

#define DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(UntypedIdentifier, Type, Name)   \
  DEFINE_MACRO_LOCAL_UNIQUE_IDENTIFIER_VALUE(__FILE__, __LINE__,             \
                                             UntypedIdentifier, Name##Impl); \
  constexpr ::ui::TypedIdentifier<UntypedIdentifier, Type> Name(Name##Impl)

#define DEFINE_MACRO_LOCAL_TYPED_IDENTIFIER_VALUE(                          \
    File, Line, UntypedIdentifier, Type, Name)                              \
  DEFINE_MACRO_LOCAL_UNIQUE_IDENTIFIER_VALUE(File, Line, UntypedIdentifier, \
                                             Name##Impl);                   \
  constexpr ::ui::TypedIdentifier<UntypedIdentifier, Type> Name(Name##Impl)

#endif  // UI_BASE_IDENTIFIER_TYPED_IDENTIFIER_H_
