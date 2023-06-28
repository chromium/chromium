// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_PULSING_INK_DROP_MASK_H_
#define UI_VIEWS_ANIMATION_PULSING_INK_DROP_MASK_H_

#include "ui/gfx/animation/throb_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/animation/ink_drop_mask.h"

namespace views {

// An InkDropMask that animates into pulsing effect.
class VIEWS_EXPORT PulsingInkDropMask : public views::AnimationDelegateViews,
                                        public views::InkDropMask {
 public:
  explicit PulsingInkDropMask(views::View* layer_container);
  PulsingInkDropMask(const PulsingInkDropMask&) = delete;
  PulsingInkDropMask& operator=(const PulsingInkDropMask&) = delete;

 private:
  // views::InkDropMask:
  void OnPaintLayer(const ui::PaintContext& context) override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // The View that contains the InkDrop layer we're masking. This must outlive
  // our instance.
  const raw_ptr<views::View> layer_container_;

  // Normal corner radius of the ink drop without animation. This is also the
  // corner radius at the largest instant of the animation.
  const float normal_corner_radius_;

  gfx::ThrobAnimation throb_animation_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_PULSING_INK_DROP_MASK_H_
