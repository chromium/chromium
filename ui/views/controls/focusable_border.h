// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_FOCUSABLE_BORDER_H_
#define UI_VIEWS_CONTROLS_FOCUSABLE_BORDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
class Insets;
}  // namespace gfx

namespace views {

// A Border class to draw a focused border around a field (e.g textfield).
class VIEWS_EXPORT FocusableBorder : public Border {
 public:
  static constexpr float kCornerRadiusDp = 2.f;

  FocusableBorder();
  ~FocusableBorder() override;

  // Sets the insets of the border.
  void SetInsets(int top, int left, int bottom, int right);
  void SetInsets(int vertical, int horizontal);

  // Sets the color id to use for this border. When unsupplied, the color will
  // depend on the focus state.
  void SetColorId(const absl::optional<ui::NativeTheme::ColorId>& color_id);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 protected:
  SkColor GetCurrentColor(const View& view) const;

 private:
  gfx::Insets insets_;

  absl::optional<ui::NativeTheme::ColorId> override_color_id_;

  DISALLOW_COPY_AND_ASSIGN(FocusableBorder);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_FOCUSABLE_BORDER_H_
