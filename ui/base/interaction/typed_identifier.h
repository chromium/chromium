// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_TYPED_IDENTIFIER_H_
#define UI_BASE_INTERACTION_TYPED_IDENTIFIER_H_

#include "ui/base/interaction/element_identifier.h"

namespace ui {

// Identifier that also carries type information.
//
// Use the DECLARE/DEFINE macros below to create unique identifiers, similarly
// to how ElementIdentifier, etc. work.
template <typename Type>
class TypedIdentifier final {
 public:
  constexpr TypedIdentifier() = default;

  explicit constexpr TypedIdentifier(ElementIdentifier identifier)
      : identifier_(identifier) {}

  constexpr ElementIdentifier identifier() const { return identifier_; }

  constexpr explicit operator bool() const {
    return static_cast<bool>(identifier_);
  }

  constexpr bool operator!() const { return !identifier_; }

  friend constexpr bool operator==(const TypedIdentifier<Type>&,
                                   const TypedIdentifier<Type>&) = default;
  friend constexpr auto operator<=>(const TypedIdentifier<Type>&,
                                    const TypedIdentifier<Type>&) = default;

 private:
  ElementIdentifier identifier_;
};

template <typename T>
extern void PrintTo(TypedIdentifier<T> identifier, std::ostream* os) {
  *os << "TypedIdentifier " << identifier.identifier().GetRawValue() << " ["
      << identifier.identifier().GetName() << "]";
}

template <typename T>
extern std::ostream& operator<<(std::ostream& os,
                                TypedIdentifier<T> identifier) {
  PrintTo(identifier, os);
  return os;
}

}  // namespace ui

// The following macros create a typed identifier value, and mimic the similar
// macros for ElementIdentifier, except that they also include a type.

#define DECLARE_TYPED_IDENTIFIER_VALUE(Type, Name) \
  DECLARE_ELEMENT_IDENTIFIER_VALUE(Name##Impl);    \
  extern const ui::TypedIdentifier<Type> Name

#define DEFINE_TYPED_IDENTIFIER_VALUE(Type, Name) \
  DEFINE_ELEMENT_IDENTIFIER_VALUE(Name##Impl);    \
  constexpr ui::TypedIdentifier<Type> Name(Name##Impl)

#define DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(Type, Name) \
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(Name##Impl);    \
  static constexpr ui::TypedIdentifier<Type> Name {      \
    Name##Impl                                           \
  }

#define DEFINE_CLASS_TYPED_IDENTIFIER_VALUE(Class, Type, Name) \
  DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(Class, Name##Impl);    \
  constexpr ui::TypedIdentifier<Type> Class::Name

#define DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(Type, Name)                  \
  DEFINE_MACRO_ELEMENT_IDENTIFIER_VALUE(__FILE__, __LINE__, Name##Impl); \
  constexpr ui::TypedIdentifier<Type> Name(Name##Impl)

#define DEFINE_MACRO_TYPED_IDENTIFIER_VALUE(File, Line, Type, Name) \
  DEFINE_MACRO_ELEMENT_IDENTIFIER_VALUE(File, Line, Name##Impl);    \
  constexpr ui::TypedIdentifier<Type> Name(Name##Impl)

#endif  // UI_BASE_INTERACTION_TYPED_IDENTIFIER_H_
