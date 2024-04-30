// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_INFO_BUBBLE_H_
#define UI_VIEWS_BUBBLE_INFO_BUBBLE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {

class InfoBubbleFrame;
class Label;

// Class to create and manage an information bubble for errors or tooltips.
class VIEWS_EXPORT InfoBubble : public BubbleDialogDelegateView {
  METADATA_HEADER(InfoBubble, BubbleDialogDelegateView)

 public:
  InfoBubble(View* anchor,
             BubbleBorder::Arrow arrow,
             const std::u16string& message);

  InfoBubble(const InfoBubble&) = delete;
  InfoBubble& operator=(const InfoBubble&) = delete;

  ~InfoBubble() override;

  // Shows the bubble.
  void Show();

  // Hides and closes the bubble.
  void Hide();

  // BubbleDialogDelegateView:
  std::unique_ptr<NonClientFrameView> CreateNonClientFrameView(
      Widget* widget) override;
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  void OnWidgetBoundsChanged(Widget* widget,
                             const gfx::Rect& new_bounds) override;

  void set_preferred_width(int preferred_width) {
    preferred_width_ = preferred_width;
  }

  const Label* label_for_testing() const { return label_; }

 private:
  // Updates the position of the bubble.
  void UpdatePosition();

  raw_ptr<InfoBubbleFrame> frame_ = nullptr;
  raw_ptr<Label> label_ = nullptr;

  // The width this bubble prefers to be. Default is 0 (no preference).
  int preferred_width_ = 0;
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_INFO_BUBBLE_H_
