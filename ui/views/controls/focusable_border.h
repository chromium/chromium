// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_FOCUSABLE_BORDER_H_
#define UI_VIEWS_CONTROLS_FOCUSABLE_BORDER_H_

#include <optional>

#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace views {

// A Border class to draw a focused border around a field (e.g textfield).
class VIEWS_EXPORT FocusableBorder : public Border {
 public:
  FocusableBorder();

  FocusableBorder(const FocusableBorder&) = delete;
  FocusableBorder& operator=(const FocusableBorder&) = delete;

  ~FocusableBorder() override;

  // Sets the insets of the border.
  void SetInsets(const gfx::Insets& insets);

  // Sets the color id to use for this border. When unsupplied, the color will
  // depend on the focus state.
  void SetColorId(const std::optional<ui::ColorId>& color_id);

  // Sets the corner radius.
  void SetCornerRadius(float corner_radius);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 protected:
  SkColor GetCurrentColor(const View& view) const;

 private:
  gfx::Insets insets_;
  float corner_radius_;
  std::optional<ui::ColorId> override_color_id_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_FOCUSABLE_BORDER_H_
