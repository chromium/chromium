// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
 public:
  METADATA_HEADER(ToggleButton);

  explicit ToggleButton(PressedCallback callback = PressedCallback());

  ToggleButton(const ToggleButton&) = delete;
  ToggleButton& operator=(const ToggleButton&) = delete;

  ~ToggleButton() override;

  // AnimateIsOn() animates the state change to |is_on|; SetIsOn() doesn't.
  void AnimateIsOn(bool is_on);
  void SetIsOn(bool is_on);
  bool GetIsOn() const;

  void SetThumbOnColor(const absl::optional<SkColor>& thumb_on_color);
  absl::optional<SkColor> GetThumbOnColor() const;
  void SetThumbOffColor(const absl::optional<SkColor>& thumb_off_color);
  absl::optional<SkColor> GetThumbOffColor() const;
  void SetTrackOnColor(const absl::optional<SkColor>& track_on_color);
  absl::optional<SkColor> GetTrackOnColor() const;
  void SetTrackOffColor(const absl::optional<SkColor>& track_off_color);
  absl::optional<SkColor> GetTrackOffColor() const;

  void SetAcceptsEvents(bool accepts_events);
  bool GetAcceptsEvents() const;

  // views::View:
  void AddLayerToRegion(ui::Layer* layer, LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* layer) override;
  gfx::Size CalculatePreferredSize() const override;

 protected:
  // views::View:
  void OnThemeChanged() override;

  // views::Button:
  void NotifyClick(const ui::Event& event) override;
  void StateChanged(ButtonState old_state) override;

  // Returns the path to draw the focus ring around for this ToggleButton.
  SkPath GetFocusRingPath() const;

 private:
  friend class TestToggleButton;
  class FocusRingHighlightPathGenerator;
  class ThumbView;

  // Calculates and returns the bounding box for the track.
  gfx::Rect GetTrackBounds() const;

  // Calculates and returns the bounding box for the thumb (the circle).
  gfx::Rect GetThumbBounds() const;

  // Updates position of the thumb.
  void UpdateThumb();

  SkColor GetTrackColor(bool is_on) const;

  // views::View:
  bool CanAcceptEvent(const ui::Event& event) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnFocus() override;
  void OnBlur() override;

  // Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  gfx::SlideAnimation slide_animation_{this};
  raw_ptr<ThumbView> thumb_view_;
  absl::optional<SkColor> track_on_color_;
  absl::optional<SkColor> track_off_color_;

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
