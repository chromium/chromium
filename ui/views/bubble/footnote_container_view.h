// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_FOOTNOTE_CONTAINER_VIEW_H_
#define UI_VIEWS_BUBBLE_FOOTNOTE_CONTAINER_VIEW_H_

#include "ui/views/view.h"

namespace views {

// A container that changes visibility with its contents, and draws a solid
// background with rounded corners at the bottom.
class FootnoteContainerView : public View {
 public:
  METADATA_HEADER(FootnoteContainerView);

  FootnoteContainerView(const gfx::Insets& margins,
                        std::unique_ptr<View> child_view,
                        float corner_radius);
  ~FootnoteContainerView() override;

  void SetCornerRadius(float corner_radius);

  // View:
  void OnThemeChanged() override;
  void ChildVisibilityChanged(View* child) override;

 private:
  void ResetBackground();
  void ResetBorder();

  float corner_radius_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FootnoteContainerView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_FOOTNOTE_CONTAINER_VIEW_H_
