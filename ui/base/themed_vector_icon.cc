// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/themed_vector_icon.h"

#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/gfx/vector_icon_utils.h"

namespace ui {

ThemedVectorIcon::ThemedVectorIcon() = default;

ThemedVectorIcon::ThemedVectorIcon(const gfx::VectorIcon* icon,
                                   ColorId color_id,
                                   int icon_size,
                                   const gfx::VectorIcon* badge)
    : icon_(icon), icon_size_(icon_size), color_(color_id), badge_(badge) {}

ThemedVectorIcon::ThemedVectorIcon(const VectorIconModel& vector_icon_model)
    : icon_(vector_icon_model.vector_icon()),
      icon_size_(vector_icon_model.icon_size()),
      badge_(vector_icon_model.badge_icon()) {
  if (vector_icon_model.has_color()) {
    color_ = vector_icon_model.color();
  } else {
    color_ = vector_icon_model.color_id();
  }
}

ThemedVectorIcon::ThemedVectorIcon(const gfx::VectorIcon* icon,
                                   SkColor color,
                                   int icon_size,
                                   const gfx::VectorIcon* badge)
    : icon_(icon), icon_size_(icon_size), color_(color), badge_(badge) {}

ThemedVectorIcon::ThemedVectorIcon(const ThemedVectorIcon&) = default;

ThemedVectorIcon& ThemedVectorIcon::operator=(const ThemedVectorIcon&) =
    default;

ThemedVectorIcon::ThemedVectorIcon(ThemedVectorIcon&&) = default;

ThemedVectorIcon& ThemedVectorIcon::operator=(ThemedVectorIcon&&) = default;

ThemedVectorIcon::~ThemedVectorIcon() = default;

gfx::ImageSkia ThemedVectorIcon::GetImageSkia(
    const ColorProvider* color_provider) const {
  DCHECK(!empty());
  return GetImageSkia(color_provider, (icon_size_ > 0)
                                          ? icon_size_
                                          : GetDefaultSizeOfVectorIcon(*icon_));
}

gfx::ImageSkia ThemedVectorIcon::GetImageSkia(
    const ColorProvider* color_provider,
    int icon_size) const {
  DCHECK(!empty());
  return GetImageSkia(GetColor(color_provider), icon_size);
}

gfx::ImageSkia ThemedVectorIcon::GetImageSkia(SkColor color) const {
  DCHECK(!empty());
  return GetImageSkia(color, (icon_size_ > 0)
                                 ? icon_size_
                                 : GetDefaultSizeOfVectorIcon(*icon_));
}

SkColor ThemedVectorIcon::GetColor(const ColorProvider* color_provider) const {
  return absl::holds_alternative<ColorId>(color_)
             ? color_provider->GetColor(absl::get<ColorId>(color_))
             : absl::get<SkColor>(color_);
}

gfx::ImageSkia ThemedVectorIcon::GetImageSkia(SkColor color,
                                              int icon_size) const {
  return badge_ ? CreateVectorIconWithBadge(*icon_, icon_size, color, *badge_)
                : CreateVectorIcon(*icon_, icon_size, color);
}

}  // namespace ui
