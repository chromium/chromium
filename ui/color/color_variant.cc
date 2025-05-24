// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_variant.h"

#include <optional>
#include <string>
#include <variant>

#include "base/check.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"
#include "ui/gfx/color_palette.h"

namespace ui {

ColorVariant::ColorVariant() = default;

ColorVariant::ColorVariant(SkColor color) : color_variant_(color) {}
ColorVariant::ColorVariant(ColorId color_id) : color_variant_(color_id) {}

ColorVariant::~ColorVariant() = default;

bool ColorVariant::IsSemantic() const {
  return !!GetColorId();
}

bool ColorVariant::IsPhysical() const {
  return !!GetSkColor();
}

std::optional<SkColor> ColorVariant::GetSkColor() const {
  return std::holds_alternative<SkColor>(color_variant_)
             ? std::make_optional(std::get<SkColor>(color_variant_))
             : std::nullopt;
}

SkColor ColorVariant::ResolveToSkColor(
    const ColorProvider* color_provider) const {
  if (auto color = GetSkColor()) {
    return color.value();
  }

  CHECK(color_provider);
  return color_provider->GetColor(GetColorId().value());
}

std::string ColorVariant::ToString() const {
  if (auto color = GetSkColor()) {
    return ui::SkColorName(*color);
  }

  return ui::ColorIdName(*GetColorId());
}

std::optional<ColorId> ColorVariant::GetColorId() const {
  return std::holds_alternative<ColorId>(color_variant_)
             ? std::make_optional(std::get<ColorId>(color_variant_))
             : std::nullopt;
}

}  // namespace ui
