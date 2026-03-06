// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_TYPED_IDENTIFIER_H_
#define UI_BASE_INTERACTION_TYPED_IDENTIFIER_H_

#include "ui/base/identifier/typed_identifier.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {

// Identifier that also carries type information.
// Currently being migrated to the new format.
template <typename T>
using TypedIdentifierOld = TypedIdentifier<ElementIdentifier, T>;

}  // namespace ui

// The following macros create a typed identifier value, and mimic the similar
// macros for ElementIdentifier, except that they also include a type.

#define DECLARE_TYPED_IDENTIFIER_VALUE_OLD(Type, Name) \
  DECLARE_TYPED_IDENTIFIER_VALUE(::ui::ElementIdentifier, Type, Name)

#define DEFINE_TYPED_IDENTIFIER_VALUE_OLD(Type, Name) \
  DEFINE_TYPED_IDENTIFIER_VALUE(::ui::ElementIdentifier, Type, Name)

#define DECLARE_CLASS_TYPED_IDENTIFIER_VALUE_OLD(Type, Name) \
  DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(::ui::ElementIdentifier, Type, Name)

#define DEFINE_CLASS_TYPED_IDENTIFIER_VALUE_OLD(Class, Type, Name)          \
  DEFINE_CLASS_TYPED_IDENTIFIER_VALUE(Class, ::ui::ElementIdentifier, Type, \
                                      Name)

#define DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE_OLD(Type, Name) \
  DEFINE_MACRO_LOCAL_TYPED_IDENTIFIER_VALUE(                \
      __FILE__, __LINE__, ::ui::ElementIdentifier, Type, Name)

#define DEFINE_MACRO_TYPED_IDENTIFIER_VALUE_OLD(File, Line, Type, Name) \
  DEFINE_MACRO_LOCAL_TYPED_IDENTIFIER_VALUE(                            \
      File, Line, ::ui::ElementIdentifier, Type, Name)

#endif  // UI_BASE_INTERACTION_TYPED_IDENTIFIER_H_
