// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_THEMED_VECTOR_ICON_H_
#define UI_NATIVE_THEME_THEMED_VECTOR_ICON_H_

#include "base/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_export.h"

namespace gfx {
class ImageSkia;
struct VectorIcon;
}  // namespace gfx

namespace ui {

class NATIVE_THEME_EXPORT ThemedVectorIcon {
 public:
  ThemedVectorIcon();
  explicit ThemedVectorIcon(
      const gfx::VectorIcon* icon,
      NativeTheme::ColorId color_id = NativeTheme::kColorId_MenuIconColor,
      int icon_size = 0);
  explicit ThemedVectorIcon(const VectorIconModel& vector_icon_model);
  // TODO (kylixrd): Remove this once all the hard-coded uses of color are
  // removed.
  ThemedVectorIcon(const gfx::VectorIcon* icon,
                   SkColor color,
                   int icon_size = 0);

  // Copyable and moveable
  ThemedVectorIcon(const ThemedVectorIcon& other);
  ThemedVectorIcon& operator=(const ThemedVectorIcon&);
  ThemedVectorIcon(ThemedVectorIcon&&);
  ThemedVectorIcon& operator=(ThemedVectorIcon&&);

  void clear() { icon_ = nullptr; }
  bool empty() const { return !icon_; }
  gfx::ImageSkia GetImageSkia(const NativeTheme* theme) const;
  gfx::ImageSkia GetImageSkia(const NativeTheme* theme, int icon_size) const;
  gfx::ImageSkia GetImageSkia(SkColor color) const;

 private:
  SkColor GetColor(const NativeTheme* theme) const;

  const gfx::VectorIcon* icon_ = nullptr;
  int icon_size_ = 0;
  base::Optional<NativeTheme::ColorId> color_id_;
  base::Optional<SkColor> color_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_THEMED_VECTOR_ICON_H_
