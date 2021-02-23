// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_DOT_INDICATOR_H_
#define UI_VIEWS_CONTROLS_DOT_INDICATOR_H_

#include "base/macros.h"
#include "ui/views/view.h"

namespace views {

// Dot indicator that can be added to a view, usually used as a status
// indicator.
class VIEWS_EXPORT DotIndicator : public View {
 public:
  METADATA_HEADER(DotIndicator);
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

  SkColor dot_color_ = SK_ColorRED;
  SkColor border_color_ = SK_ColorWHITE;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_DOT_INDICATOR_H_
