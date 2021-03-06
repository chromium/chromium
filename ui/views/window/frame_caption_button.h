// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_FRAME_CAPTION_BUTTON_H_
#define UI_VIEWS_WINDOW_FRAME_CAPTION_BUTTON_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/views_export.h"
#include "ui/views/window/caption_button_types.h"

namespace gfx {
class SlideAnimation;
struct VectorIcon;
}  // namespace gfx

namespace views {

// Base class for the window caption buttons (minimize, maximize, restore,
// close).
class VIEWS_EXPORT FrameCaptionButton : public views::Button {
 public:
  METADATA_HEADER(FrameCaptionButton);
  enum Animate { ANIMATE_YES, ANIMATE_NO };

  FrameCaptionButton(PressedCallback callback,
                     CaptionButtonIcon icon,
                     int hit_test_type);
  FrameCaptionButton(const FrameCaptionButton&) = delete;
  FrameCaptionButton& operator=(const FrameCaptionButton&) = delete;
  ~FrameCaptionButton() override;

  // Gets the color to use for a frame caption button.
  static SkColor GetButtonColor(SkColor background_color);

  // Gets the alpha ratio for the colors of inactive frame caption buttons.
  static float GetInactiveButtonColorAlphaRatio();

  // Sets the image to use to paint the button. If |animate| is ANIMATE_YES,
  // the button crossfades to the new visuals. If the image matches the one
  // currently used by the button and |animate| is ANIMATE_NO, the crossfade
  // animation is progressed to the end.
  void SetImage(CaptionButtonIcon icon,
                Animate animate,
                const gfx::VectorIcon& icon_image);

  // Returns true if the button is crossfading to new visuals set in
  // SetImage().
  bool IsAnimatingImageSwap() const;

  // Sets the alpha to use for painting. Used to animate visibility changes.
  void SetAlpha(int alpha);

  // views::Button:
  void OnGestureEvent(ui::GestureEvent* event) override;
  views::PaintInfo::ScaleType GetPaintScaleType() const override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;

  void SetBackgroundColor(SkColor background_color);
  SkColor GetBackgroundColor() const;

  void set_paint_as_active(bool paint_as_active) {
    paint_as_active_ = paint_as_active;
  }

  bool paint_as_active() const { return paint_as_active_; }

  void SetInkDropCornerRadius(int ink_drop_corner_radius);
  int GetInkDropCornerRadius() const;

  CaptionButtonIcon GetIcon() const { return icon_; }

  const gfx::ImageSkia& icon_image() const { return icon_image_; }

  const gfx::VectorIcon* icon_definition_for_test() const {
    return icon_definition_;
  }

 protected:
  // views::Button override:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  class HighlightPathGenerator;

  // Determines what alpha to use for the icon based on animation and
  // active state.
  int GetAlphaForIcon(int base_alpha) const;

  // Returns the amount by which the inkdrop ripple and mask should be insetted
  // from the button size in order to achieve a circular inkdrop with a size
  // equals to kInkDropHighlightSize.
  gfx::Insets GetInkdropInsets(const gfx::Size& button_size) const;

  void UpdateInkDropBaseColor();

  // The button's current icon.
  CaptionButtonIcon icon_;

  // The current background color.
  SkColor background_color_;

  // Whether the button should be painted as active.
  bool paint_as_active_;

  // Current alpha to use for painting.
  int alpha_;

  // Radius of the ink drop highlight and mask.
  int ink_drop_corner_radius_;

  // The image id (kept for the purposes of testing) and image used to paint the
  // button's icon.
  const gfx::VectorIcon* icon_definition_ = nullptr;
  gfx::ImageSkia icon_image_;

  // The icon image to crossfade from.
  gfx::ImageSkia crossfade_icon_image_;

  // Crossfade animation started when the button's images are changed by
  // SetImage().
  std::unique_ptr<gfx::SlideAnimation> swap_images_animation_;
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_FRAME_CAPTION_BUTTON_H_
