// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_FRAME_CAPTION_BUTTON_H_
#define UI_VIEWS_WINDOW_FRAME_CAPTION_BUTTON_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/views_export.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/caption_button_types.h"

namespace cc {
class PaintFlags;
}  // namespace cc

namespace gfx {
class SlideAnimation;
struct VectorIcon;
}  // namespace gfx

namespace views {

// Base class for the window caption buttons (minimize, maximize, restore,
// close).
class VIEWS_EXPORT FrameCaptionButton : public Button {
  METADATA_HEADER(FrameCaptionButton, Button)

 public:
  enum class Animate { kYes, kNo };

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

  // Sets the image to use to paint the button. If |animate| is Animate::kYes,
  // the button crossfades to the new visuals. If the image matches the one
  // currently used by the button and |animate| is Animate::kNo, the crossfade
  // animation is progressed to the end.
  void SetImage(CaptionButtonIcon icon,
                Animate animate,
                const gfx::VectorIcon& icon_image);

  // Returns true if the button is crossfading to new visuals set in
  // SetImage().
  bool IsAnimatingImageSwap() const;

  // Sets the alpha to use for painting. Used to animate visibility changes.
  void SetAlpha(SkAlpha alpha);

  // views::Button:
  void OnGestureEvent(ui::GestureEvent* event) override;
  views::PaintInfo::ScaleType GetPaintScaleType() const override;

  // TODO(b/292154873): Replace them to set and get the foreground color.
  void SetBackgroundColor(SkColor background_color);
  SkColor GetBackgroundColor() const;

  void SetIconColorId(ui::ColorId icon_color_id);

  void SetPaintAsActive(bool paint_as_active);
  bool GetPaintAsActive() const;

  void SetInkDropCornerRadius(int ink_drop_corner_radius);
  int GetInkDropCornerRadius() const;

  base::CallbackListSubscription AddBackgroundColorChangedCallback(
      PropertyChangedCallback callback);

  CaptionButtonIcon GetIcon() const { return icon_; }

  const gfx::ImageSkia& icon_image() const { return icon_image_; }

  const gfx::VectorIcon* icon_definition_for_test() const {
    return icon_definition_;
  }

 protected:
  // views::Button override:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  virtual void DrawHighlight(gfx::Canvas* canvas, cc::PaintFlags flags);
  virtual void DrawIconContents(gfx::Canvas* canvas,
                                gfx::ImageSkia image,
                                int x,
                                int y,
                                cc::PaintFlags flags);
  // Returns the size of the inkdrop ripple.
  virtual gfx::Size GetInkDropSize() const;

  // Returns the amount by which the inkdrop ripple and mask should be insetted
  // from the button size in order to draw the inkdrop with a size returned by
  // GetInkDropSize().
  gfx::Insets GetInkdropInsets(const gfx::Size& button_size) const;

  // Called when the `background_color_` or `icon_color_id_` is updated to
  // reflect the color change on icon and inkdrop.
  void MaybeRefreshIconAndInkdropBaseColor();

 private:
  class HighlightPathGenerator;

  // Determines what alpha to use for the icon based on animation and
  // active state.
  SkAlpha GetAlphaForIcon(SkAlpha base_alpha) const;

  void UpdateInkDropBaseColor();

  // The button's current icon.
  CaptionButtonIcon icon_;

  // The color used to compute the icon's color. If it's SkColor type, it's the
  // background color of the container view, call `GetButtonColor` to get
  // contrast color. If it's ColorId type, directly resolve the color from color
  // id.
  // TODO(b/292154873): Store the foreground color instead of the background
  // color for the SkColor type.
  absl::variant<ui::ColorId, SkColor> color_ = gfx::kPlaceholderColor;

  // Whether the button should be painted as active.
  bool paint_as_active_ = false;

  // Current alpha to use for painting.
  SkAlpha alpha_ = SK_AlphaOPAQUE;

  // Radius of the ink drop highlight and mask.
  int ink_drop_corner_radius_ = kCaptionButtonInkDropDefaultCornerRadius;

  // The image id (kept for the purposes of testing) and image used to paint the
  // button's icon.
  raw_ptr<const gfx::VectorIcon> icon_definition_ = nullptr;
  gfx::ImageSkia icon_image_;

  // The icon image to crossfade from.
  gfx::ImageSkia crossfade_icon_image_;

  // Crossfade animation started when the button's images are changed by
  // SetImage().
  std::unique_ptr<gfx::SlideAnimation> swap_images_animation_;
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_FRAME_CAPTION_BUTTON_H_
