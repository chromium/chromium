// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SEPARATOR_H_
#define UI_VIEWS_CONTROLS_SEPARATOR_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "ui/views/view.h"

namespace views {

// The Separator class is a view that shows a line used to visually separate
// other views.
class VIEWS_EXPORT Separator : public View {
 public:
  METADATA_HEADER(Separator);

  // The separator's thickness in dip.
  static constexpr int kThickness = 1;

  Separator();
  ~Separator() override;

  SkColor GetColor() const;
  void SetColor(SkColor color);

  int GetPreferredHeight() const;
  void SetPreferredHeight(int height);

  // Overridden from View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  int preferred_height_ = kThickness;
  base::Optional<SkColor> overridden_color_;

  DISALLOW_COPY_AND_ASSIGN(Separator);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SEPARATOR_H_
