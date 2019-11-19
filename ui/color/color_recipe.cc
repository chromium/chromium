// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_recipe.h"

#include <utility>

#include "ui/color/color_mixer.h"

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
  transforms_.push_back(transform);
  return *this;
}

SkColor ColorRecipe::GenerateResult(SkColor input,
                                    const ColorMixer& mixer) const {
  for (const auto& transform : transforms_)
    input = transform.Run(input, mixer);
  return input;
}

ColorRecipe operator+(ColorRecipe recipe, const ColorTransform& transform) {
  recipe += transform;
  return recipe;
}

}  // namespace ui