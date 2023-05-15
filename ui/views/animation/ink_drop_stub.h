// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_STUB_H_
#define UI_VIEWS_ANIMATION_INK_DROP_STUB_H_

#include "ui/views/animation/ink_drop.h"
#include "ui/views/views_export.h"

namespace views {

// A stub implementation of an InkDrop that can be used when no visuals should
// be shown. e.g. material design is enabled.
class VIEWS_EXPORT InkDropStub : public InkDrop {
 public:
  InkDropStub();

  InkDropStub(const InkDropStub&) = delete;
  InkDropStub& operator=(const InkDropStub&) = delete;

  ~InkDropStub() override;

  // InkDrop:
  void HostSizeChanged(const gfx::Size& new_size) override;
  void HostViewThemeChanged() override;
  void HostTransformChanged(const gfx::Transform& new_transform) override;
  InkDropState GetTargetInkDropState() const override;
  void AnimateToState(InkDropState state) override;
  void SetHoverHighlightFadeDuration(base::TimeDelta duration) override;
  void UseDefaultHoverHighlightFadeDuration() override;
  void SnapToActivated() override;
  void SnapToHidden() override;
  void SetHovered(bool is_hovered) override;
  void SetFocused(bool is_hovered) override;
  bool IsHighlightFadingInOrVisible() const override;
  void SetShowHighlightOnHover(bool show_highlight_on_hover) override;
  void SetShowHighlightOnFocus(bool show_highlight_on_focus) override;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_STUB_H_
