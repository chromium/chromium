// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SLIDER_H_
#define UI_VIEWS_CONTROLS_SLIDER_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

namespace test {
class SliderTestApi;
}

class Slider;

enum class SliderChangeReason {
  kByUser,  // value was changed by the user (e.g. by clicking)
  kByApi,   // value was changed by a call to SetValue.
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

// Slider operates in interval [0,1] by default, but can also switch between a
// predefined set of values, see SetAllowedValues method below.
class VIEWS_EXPORT Slider : public View, public gfx::AnimationDelegate {
  METADATA_HEADER(Slider, View)

 public:
  explicit Slider(SliderListener* listener = nullptr);
  Slider(const Slider&) = delete;
  Slider& operator=(const Slider&) = delete;
  ~Slider() override;

  float GetValue() const;
  void SetValue(float value);

  // Getter and Setter of `value_indicator_radius_`.
  float GetValueIndicatorRadius() const;
  void SetValueIndicatorRadius(float radius);

  bool GetEnableAccessibilityEvents() const;
  void SetEnableAccessibilityEvents(bool enabled);

  // Represents the visual style of the slider.
  enum class RenderingStyle {
    kDefaultStyle,
    kMinimalStyle,
  };

  // Set rendering style and schedule paint since the colors for the slider
  // may change.
  void SetRenderingStyle(RenderingStyle style);

  RenderingStyle style() const { return style_; }

  // Sets discrete set of allowed slider values. Each value must be in [0,1].
  // Sets active value to the lower bound of the current value in allowed set.
  // nullptr will drop currently active set and allow full [0,1] interval.
  void SetAllowedValues(const base::flat_set<float>* allowed_values);

  const base::flat_set<float>& allowed_values() const {
    return allowed_values_;
  }

  // The radius of the thumb.
  static constexpr float kThumbRadius = 4.f;

 protected:
  // Returns the current position of the thumb on the slider.
  float GetAnimatingValue() const;

  // Shows or hides the highlight on the slider thumb. The default
  // implementation does nothing.
  void SetHighlighted(bool is_highlighted);

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

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
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  void OnFocus() override;
  void OnBlur() override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void AddedToWidget() override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  void set_listener(SliderListener* listener) { listener_ = listener; }

  void NotifyPendingAccessibilityValueChanged();

  virtual SkColor GetThumbColor() const;
  virtual SkColor GetTroughColor() const;
  int GetSliderExtraPadding() const;

  raw_ptr<SliderListener, AcrossTasksDanglingUntriaged> listener_;

  std::unique_ptr<gfx::SlideAnimation> move_animation_;

  // When |allowed_values_| is not empty, slider will allow moving only between
  // these values. I.e. it will become discrete slider.
  base::flat_set<float> allowed_values_;  // Allowed values.
  float value_ = 0.f;
  float keyboard_increment_ = 0.1f;
  float initial_animating_value_ = 0.f;
  bool value_is_valid_ = false;
  bool accessibility_events_enabled_ = true;

  // Relative position of the mouse cursor (or the touch point) on the slider's
  // button.
  int initial_button_offset_ = 0;

  // The radius of the value indicator.
  float value_indicator_radius_ = kThumbRadius;

  RenderingStyle style_ = RenderingStyle::kDefaultStyle;

  // Animating value of the current radius of the thumb's highlight.
  float thumb_highlight_radius_ = 0.f;

  gfx::SlideAnimation highlight_animation_{this};

  bool pending_accessibility_value_change_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SLIDER_H_
