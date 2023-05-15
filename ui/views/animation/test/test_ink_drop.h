// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_H_
#define UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_H_

#include "ui/views/animation/ink_drop.h"

namespace views::test {

// A InkDrop test double that tracks the last requested state changes.
//
// NOTE: This does not auto transition between any of the InkDropStates.
//
class TestInkDrop : public InkDrop {
 public:
  TestInkDrop();

  TestInkDrop(const TestInkDrop&) = delete;
  TestInkDrop& operator=(const TestInkDrop&) = delete;

  ~TestInkDrop() override;

  bool is_hovered() const { return is_hovered_; }

  // InkDrop:
  void HostSizeChanged(const gfx::Size& new_size) override;
  void HostViewThemeChanged() override;
  void HostTransformChanged(const gfx::Transform& new_transform) override;
  InkDropState GetTargetInkDropState() const override;
  void AnimateToState(InkDropState ink_drop_state) override;
  void SetHoverHighlightFadeDuration(base::TimeDelta duration) override;
  void UseDefaultHoverHighlightFadeDuration() override;
  void SnapToActivated() override;
  void SnapToHidden() override;
  void SetHovered(bool is_hovered) override;
  void SetFocused(bool is_focused) override;
  bool IsHighlightFadingInOrVisible() const override;
  void SetShowHighlightOnHover(bool show_highlight_on_hover) override;
  void SetShowHighlightOnFocus(bool show_highlight_on_focus) override;

 private:
  InkDropState state_ = InkDropState::HIDDEN;
  bool is_hovered_ = false;
};

}  // namespace views::test

#endif  // UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_H_
