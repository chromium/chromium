// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_STUB_H_
#define UI_VIEWS_ANIMATION_INK_DROP_STUB_H_

#include "base/macros.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/views_export.h"

namespace views {

// A stub implementation of an InkDrop that can be used when no visuals should
// be shown. e.g. material design is enabled.
class VIEWS_EXPORT InkDropStub : public InkDrop {
 public:
  InkDropStub();
  ~InkDropStub() override;

  // InkDrop:
  void HostSizeChanged(const gfx::Size& new_size) override;
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

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropStub);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_STUB_H_
