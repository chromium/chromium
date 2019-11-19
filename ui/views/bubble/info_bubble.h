// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_INFO_BUBBLE_H_
#define UI_VIEWS_BUBBLE_INFO_BUBBLE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {

class InfoBubbleFrame;

// Class to create and manage an information bubble for errors or tooltips.
class InfoBubble : public BubbleDialogDelegateView {
 public:
  METADATA_HEADER(InfoBubble);

  InfoBubble(View* anchor, const base::string16& message);
  ~InfoBubble() override;

  // Shows the bubble. |widget_| will be NULL until this is called.
  void Show();

  // Hides and closes the bubble.
  void Hide();

  // BubbleDialogDelegateView:
  NonClientFrameView* CreateNonClientFrameView(Widget* widget) override;
  gfx::Size CalculatePreferredSize() const override;
  void OnWidgetDestroyed(Widget* widget) override;
  void OnWidgetBoundsChanged(Widget* widget,
                             const gfx::Rect& new_bounds) override;

  View* anchor() { return anchor_; }
  const View* anchor() const { return anchor_; }

  void set_preferred_width(int preferred_width) {
    preferred_width_ = preferred_width;
  }

 private:
  // Updates the position of the bubble.
  void UpdatePosition();

  Widget* widget_;          // Weak, may be NULL.
  View* const anchor_;      // Weak.
  InfoBubbleFrame* frame_;  // Weak, owned by widget.

  // The width this bubble prefers to be. Default is 0 (no preference).
  int preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubble);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_INFO_BUBBLE_H_
