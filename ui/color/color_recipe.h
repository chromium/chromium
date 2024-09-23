// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_RECIPE_H_
#define UI_COLOR_COLOR_RECIPE_H_

#include <list>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_transform.h"

namespace ui {

class ColorMixer;

// A ColorRecipe describes how to construct an output color from a series of
// transforms.  Recipes take an input color and mixer, then apply their
// transforms in order to produce the output.  This means that a recipe with no
// transforms will return its input color unchanged.
class COMPONENT_EXPORT(COLOR) ColorRecipe {
 public:
  ColorRecipe();
  // This constructor acts as a shorthand initialization of a recipe with a
  // transform which is by far the most common means of recipe initialization.
  ColorRecipe(const ColorTransform& transform);  // NOLINT
  ColorRecipe(const ColorRecipe&);
  ColorRecipe& operator=(const ColorRecipe&);
  ColorRecipe(ColorRecipe&&) noexcept;
  ColorRecipe& operator=(ColorRecipe&&) noexcept;
  ~ColorRecipe();

  ColorRecipe& operator+=(const ColorTransform& transform);

  // Generates the output color for |input| by applying all transforms.  |mixer|
  // is passed to each transform, since it might need to request other colors.
  SkColor GenerateResult(SkColor input, const ColorMixer& mixer) const;

  // Returns true if this recipe is invariant to input color.
  bool Invariant() const;

 private:
  std::list<ColorTransform> transforms_;
};

COMPONENT_EXPORT(COLOR)
ColorRecipe operator+(ColorRecipe recipe, const ColorTransform& transform);

}  // namespace ui

#endif  // UI_COLOR_COLOR_RECIPE_H_
