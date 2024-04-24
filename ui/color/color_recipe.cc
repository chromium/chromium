// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_recipe.h"

#include <utility>

#include "base/logging.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_utils.h"

namespace ui {

ColorRecipe::ColorRecipe() = default;

ColorRecipe::ColorRecipe(const ColorTransform& transform) {
  *this += transform;
}

ColorRecipe::ColorRecipe(const ColorRecipe&) = default;

ColorRecipe& ColorRecipe::operator=(const ColorRecipe&) = default;

ColorRecipe::ColorRecipe(ColorRecipe&&) noexcept = default;

ColorRecipe& ColorRecipe::operator=(ColorRecipe&&) noexcept = default;

ColorRecipe::~ColorRecipe() = default;

ColorRecipe& ColorRecipe::operator+=(const ColorTransform& transform) {
  if (transform.invariant()) {
    // If a transform is invariant, previous transform results will be
    // discarded when the output color is computed. So they can be safely
    // dropped.
    transforms_.clear();
  }
  transforms_.push_back(transform);
  return *this;
}

SkColor ColorRecipe::GenerateResult(SkColor input,
                                    const ColorMixer& mixer) const {
  SkColor output_color = input;
  for (const auto& transform : transforms_)
    output_color = transform.Run(output_color, mixer);
  DVLOG(2) << "ColorRecipe::GenerateResult: Input Color " << SkColorName(input)
           << " Result Color " << SkColorName(output_color);
  return output_color;
}

bool ColorRecipe::Invariant() const {
  return !transforms_.empty() && transforms_.front().invariant();
}

ColorRecipe operator+(ColorRecipe recipe, const ColorTransform& transform) {
  recipe += transform;
  return recipe;
}

}  // namespace ui
