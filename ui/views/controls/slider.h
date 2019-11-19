// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SLIDER_H_
#define UI_VIEWS_CONTROLS_SLIDER_H_

#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

using SkColor = unsigned int;

namespace views {

namespace test {
class SliderTestApi;
}

class Slider;

enum SliderChangeReason {
  VALUE_CHANGED_BY_USER,  // value was changed by the user (by clicking, e.g.)
  VALUE_CHANGED_BY_API,   // value was changed by a call to SetValue.
};

class VIEWS_EXPORT SliderListener {
 public:
  virtual void SliderValueChanged(Slider* sender,
                                  float value,
                                  float old_value,
                                  SliderChangeReason reason) = 0;

  // Invoked when a drag starts or ends (more specifically, when the mouse
  // button is pressed or released).
  virtual void SliderDragStarted(Slider* sender) {}
  virtual void SliderDragEnded(Slider* sender) {}

 protected:
  virtual ~SliderListener() = default;
};

class VIEWS_EXPORT Slider : public View, public gfx::AnimationDelegate {
 public:
  METADATA_HEADER(Slider);

  explicit Slider(SliderListener* listener);
  ~Slider() override;

  float GetValue() const;
  void SetValue(float value);

  bool GetEnableAccessibilityEvents() const;
  void SetEnableAccessibilityEvents(bool enabled);

  // Gets/Sets IsActive state
  bool GetIsActive() const;
  void SetIsActive(bool is_active);

 protected:
  // Returns the current position of the thumb on the slider.
  float GetAnimatingValue() const;

  // Shows or hides the highlight on the slider thumb. The default
  // implementation does nothing.
  void SetHighlighted(bool is_highlighted);

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  friend class test::SliderTestApi;

  void SetValueInternal(float value, SliderChangeReason reason);

  // Should be called on the Mouse Down event. Used to calculate relative
  // position of the mouse cursor (or the touch point) on the button to
  // accurately move the button using the MoveButtonTo() method.
  void PrepareForMove(const int new_x);

  // Moves the button to the specified point and updates the value accordingly.
  void MoveButtonTo(const gfx::Point& point);

  // Notify the listener_, if not NULL, that dragging started.
  void OnSliderDragStarted();

  // Notify the listener_, if not NULL, that dragging ended.
  void OnSliderDragEnded();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void AddedToWidget() override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  void set_listener(SliderListener* listener) {
    listener_ = listener;
  }

  void NotifyPendingAccessibilityValueChanged();

  SliderListener* listener_;

  std::unique_ptr<gfx::SlideAnimation> move_animation_;

  float value_ = 0.f;
  float keyboard_increment_ = 0.1f;
  float initial_animating_value_ = 0.f;
  bool value_is_valid_ = false;
  bool accessibility_events_enabled_ = true;

  // Relative position of the mouse cursor (or the touch point) on the slider's
  // button.
  int initial_button_offset_ = 0;

  // Record whether the slider is in the active state or the disabled state.
  bool is_active_ = true;

  // Animating value of the current radius of the thumb's highlight.
  float thumb_highlight_radius_ = 0.f;

  gfx::SlideAnimation highlight_animation_;

  bool pending_accessibility_value_change_;

  DISALLOW_COPY_AND_ASSIGN(Slider);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SLIDER_H_
