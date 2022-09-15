// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_THEMED_VECTOR_ICON_H_
#define UI_BASE_THEMED_VECTOR_ICON_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"

namespace gfx {
class ImageSkia;
struct VectorIcon;
}  // namespace gfx

namespace ui {

class ColorProvider;

class COMPONENT_EXPORT(UI_BASE) ThemedVectorIcon {
 public:
  ThemedVectorIcon();
  explicit ThemedVectorIcon(const gfx::VectorIcon* icon,
                            ColorId color_id = kColorMenuIcon,
                            int icon_size = 0,
                            const gfx::VectorIcon* badge = nullptr);
  explicit ThemedVectorIcon(const VectorIconModel& vector_icon_model);
  // TODO (kylixrd): Remove this once all the hard-coded uses of color are
  // removed.
  ThemedVectorIcon(const gfx::VectorIcon* icon,
                   SkColor color,
                   int icon_size = 0,
                   const gfx::VectorIcon* badge = nullptr);

  // Copyable and moveable
  ThemedVectorIcon(const ThemedVectorIcon& other);
  ThemedVectorIcon& operator=(const ThemedVectorIcon&);
  ThemedVectorIcon(ThemedVectorIcon&&);
  ThemedVectorIcon& operator=(ThemedVectorIcon&&);

  ~ThemedVectorIcon();

  void clear() { icon_ = nullptr; }
  bool empty() const { return !icon_; }
  gfx::ImageSkia GetImageSkia(const ColorProvider* color_provider) const;
  gfx::ImageSkia GetImageSkia(const ColorProvider* color_provider,
                              int icon_size) const;
  gfx::ImageSkia GetImageSkia(SkColor color) const;

 private:
  SkColor GetColor(const ColorProvider* color_provider) const;
  gfx::ImageSkia GetImageSkia(SkColor color, int icon_size) const;

  raw_ptr<const gfx::VectorIcon> icon_ = nullptr;
  int icon_size_ = 0;
  absl::variant<ColorId, SkColor> color_ = gfx::kPlaceholderColor;
  raw_ptr<const gfx::VectorIcon> badge_ = nullptr;
};

}  // namespace ui

#endif  // UI_BASE_THEMED_VECTOR_ICON_H_
