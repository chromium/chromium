// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/button/button.h"

namespace ui {
class Event;
}  // namespace ui

namespace views {

// This view presents a button that has two states: on and off. This is similar
// to a checkbox but has no text and looks more like a two-state horizontal
// slider.
class VIEWS_EXPORT ToggleButton : public Button {
  METADATA_HEADER(ToggleButton, Button)

 public:
  explicit ToggleButton(PressedCallback callback = PressedCallback());
  ToggleButton(PressedCallback callback, bool has_thumb_shadow);

  ToggleButton(const ToggleButton&) = delete;
  ToggleButton& operator=(const ToggleButton&) = delete;

  ~ToggleButton() override;

  // AnimateIsOn() animates the state change to |is_on|; SetIsOn() doesn't.
  void AnimateIsOn(bool is_on);
  void SetIsOn(bool is_on);
  bool GetIsOn() const;

  // Sets and gets custom thumb and track colors.
  void SetThumbOnColor(SkColor thumb_on_color);
  std::optional<SkColor> GetThumbOnColor() const;
  void SetThumbOffColor(SkColor thumb_off_color);
  std::optional<SkColor> GetThumbOffColor() const;
  void SetTrackOnColor(SkColor track_on_color);
  std::optional<SkColor> GetTrackOnColor() const;
  void SetTrackOffColor(SkColor track_off_color);
  std::optional<SkColor> GetTrackOffColor() const;

  // Sets if the inner border is drawn. If `enabled`, it is drawn when the
  // switch is off. If `enabled` is false, it's never drawn.
  void SetInnerBorderEnabled(bool enabled);
  bool GetInnerBorderEnabled() const;

  void SetAcceptsEvents(bool accepts_events);
  bool GetAcceptsEvents() const;

  // views::View:
  void AddLayerToRegion(ui::Layer* layer, LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* layer) override;
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;

 protected:
  // views::View:
  void OnThemeChanged() override;

  // views::Button:
  void NotifyClick(const ui::Event& event) override;
  void StateChanged(ButtonState old_state) override;

  // Returns the path to draw the focus ring around for this ToggleButton.
  virtual SkPath GetFocusRingPath() const;

  // Calculates and returns the bounding box for the track.
  virtual gfx::Rect GetTrackBounds() const;

  // Calculates and returns the bounding box for the thumb (the circle).
  virtual gfx::Rect GetThumbBounds() const;

  // Gets current slide animation progress.
  double GetAnimationProgress() const;

 private:
  friend class TestToggleButton;
  class FocusRingHighlightPathGenerator;
  class ThumbView;

  // Updates position of the thumb.
  void UpdateThumb();

  SkColor GetTrackColor(bool is_on) const;

  SkColor GetHoverColor() const;
  SkColor GetPressedColor() const;

  // views::View:
  bool CanAcceptEvent(const ui::Event& event) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  bool inner_border_enabled_ = true;

  gfx::SlideAnimation slide_animation_{this};
  gfx::SlideAnimation hover_animation_{this};
  raw_ptr<ThumbView> thumb_view_;
  absl::variant<ui::ColorId, SkColor> track_on_color_ =
      ui::kColorToggleButtonTrackOn;
  absl::variant<ui::ColorId, SkColor> track_off_color_ =
      ui::kColorToggleButtonTrackOff;

  // When false, this button won't accept input. Different from View::SetEnabled
  // in that the view retains focus when this is false but not when disabled.
  bool accepts_events_ = true;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, ToggleButton, Button)
VIEW_BUILDER_PROPERTY(bool, IsOn)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, ToggleButton)

#endif  // UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_
