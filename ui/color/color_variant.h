// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_VARIANT_H_
#define UI_COLOR_COLOR_VARIANT_H_

#include "base/component_export.h"

namespace ui {

// ColorVariant represents common not-mutually-exclusive properties that affect
// many colors, such as "inactive" or "incognito".  This does not cover process-
// or theme-wide attributes (such as "dark mode enabled"), which should be
// handled by the code that fills in ColorSets or in individual ColorRecipes;
// nor does it cover more narrowly-scoped, mutually-exclusive properties (such
// as "pressed" vs. "hovered"), which should be tracked with separate ColorIds.
class COMPONENT_EXPORT(COLOR) ColorVariant {
 public:
  ColorVariant() = default;
  // ColorVariant is copyable because its underlying representation is small and
  // integral.
  ColorVariant(const ColorVariant&) = default;
  ColorVariant& operator=(const ColorVariant&) = default;
  ~ColorVariant() = default;

  // Setters return a non-const ref to allow chaining at construction.
  ColorVariant& set_inactive(bool inactive) {
    inactive_ = inactive;
    return *this;
  }
  ColorVariant& set_incognito(bool incognito) {
    incognito_ = incognito;
    return *this;
  }

  bool inactive() const { return inactive_; }
  bool incognito() const { return incognito_; }

 private:
  bool inactive_ = false;
  bool incognito_ = false;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_VARIANT_H_
