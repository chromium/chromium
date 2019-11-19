// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_

#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/button/button.h"

namespace views {

// This view presents a button that has two states: on and off. This is similar
// to a checkbox but has no text and looks more like a two-state horizontal
// slider.
class VIEWS_EXPORT ToggleButton : public Button {
 public:
  METADATA_HEADER(ToggleButton);

  explicit ToggleButton(ButtonListener* listener);
  ~ToggleButton() override;

  // AnimateIsOn() animates the state change to |is_on|; SetIsOn() doesn't.
  void AnimateIsOn(bool is_on);
  void SetIsOn(bool is_on);
  bool GetIsOn() const;

  void SetAcceptsEvents(bool accepts_events);
  bool GetAcceptsEvents() const;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

 private:
  friend class TestToggleButton;
  class ThumbView;

  // Calculates and returns the bounding box for the track.
  gfx::Rect GetTrackBounds() const;

  // Calculates and returns the bounding box for the thumb (the circle).
  gfx::Rect GetThumbBounds() const;

  // Updates position and color of the thumb.
  void UpdateThumb();

  SkColor GetTrackColor(bool is_on) const;

  // views::View:
  bool CanAcceptEvent(const ui::Event& event) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnThemeChanged() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnFocus() override;
  void OnBlur() override;

  // Button:
  void NotifyClick(const ui::Event& event) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<InkDrop> CreateInkDrop() override;
  std::unique_ptr<InkDropMask> CreateInkDropMask() const override;
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const override;
  SkColor GetInkDropBaseColor() const override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  gfx::SlideAnimation slide_animation_{this};
  ThumbView* thumb_view_;

  // When false, this button won't accept input. Different from View::SetEnabled
  // in that the view retains focus when this is false but not when disabled.
  bool accepts_events_ = true;

  DISALLOW_COPY_AND_ASSIGN(ToggleButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_
