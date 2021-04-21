// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/themed_vector_icon.h"

#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/gfx/vector_icon_utils.h"

namespace ui {

ThemedVectorIcon::ThemedVectorIcon() = default;

ThemedVectorIcon::ThemedVectorIcon(const gfx::VectorIcon* icon,
                                   int color_id,
                                   int icon_size,
                                   const gfx::VectorIcon* badge)
    : icon_(icon), icon_size_(icon_size), color_(color_id), badge_(badge) {}

ThemedVectorIcon::ThemedVectorIcon(const VectorIconModel& vector_icon_model)
    : icon_(vector_icon_model.vector_icon()),
      icon_size_(vector_icon_model.icon_size()),
      badge_(vector_icon_model.badge_icon()) {
  if (vector_icon_model.has_color()) {
    color_ = vector_icon_model.color();
  } else if (vector_icon_model.color_id() >= 0) {
    color_ =
        static_cast<ui::NativeTheme::ColorId>(vector_icon_model.color_id());
  } else {
    color_ = ui::NativeTheme::kColorId_MenuIconColor;
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

gfx::ImageSkia ThemedVectorIcon::GetImageSkia(const NativeTheme* theme) const {
  DCHECK(!empty());
  return GetImageSkia(theme, (icon_size_ > 0)
                                 ? icon_size_
                                 : GetDefaultSizeOfVectorIcon(*icon_));
}

gfx::ImageSkia ThemedVectorIcon::GetImageSkia(const NativeTheme* theme,
                                              int icon_size) const {
  DCHECK(!empty());
  return GetImageSkia(GetColor(theme), icon_size);
}

gfx::ImageSkia ThemedVectorIcon::GetImageSkia(SkColor color) const {
  DCHECK(!empty());
  return GetImageSkia(color, (icon_size_ > 0)
                                 ? icon_size_
                                 : GetDefaultSizeOfVectorIcon(*icon_));
}

SkColor ThemedVectorIcon::GetColor(const NativeTheme* theme) const {
  return absl::holds_alternative<int>(color_)
             ? theme->GetSystemColor(static_cast<ui::NativeTheme::ColorId>(
                   absl::get<int>(color_)))
             : absl::get<SkColor>(color_);
}

gfx::ImageSkia ThemedVectorIcon::GetImageSkia(SkColor color,
                                              int icon_size) const {
  return badge_ ? CreateVectorIconWithBadge(*icon_, icon_size, color, *badge_)
                : CreateVectorIcon(*icon_, icon_size, color);
}

}  // namespace ui
