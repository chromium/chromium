// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_DOT_INDICATOR_H_
#define UI_VIEWS_CONTROLS_DOT_INDICATOR_H_

#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"

namespace views {

// Dot indicator that can be added to a view, usually used as a status
// indicator.
class VIEWS_EXPORT DotIndicator : public View {
  METADATA_HEADER(DotIndicator, View)

 public:
  DotIndicator(DotIndicator&) = delete;
  DotIndicator& operator=(const DotIndicator&) = delete;
  ~DotIndicator() override;

  // Create a DotIndicator and adds it to |parent|. The returned dot indicator
  // is owned by the |parent|.
  static DotIndicator* Install(View* parent);

  void SetColor(SkColor dot_color, SkColor border_color);

  void Show();
  void Hide();

 private:
  DotIndicator();

  // View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  std::optional<SkColor> dot_color_;
  std::optional<SkColor> border_color_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_DOT_INDICATOR_H_
