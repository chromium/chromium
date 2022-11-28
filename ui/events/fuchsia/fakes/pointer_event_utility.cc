// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/fuchsia/fakes/pointer_event_utility.h"
#include <zircon/types.h>
#include <utility>

namespace ui {

namespace fup = fuchsia::ui::pointer;

namespace {

fup::ViewParameters CreateViewParameters(gfx::RectF view,
                                         gfx::RectF viewport,
                                         std::array<float, 9> transform) {
  fup::ViewParameters params;
  fuchsia::ui::pointer::Rectangle view_rect;
  view_rect.min = {view.x(), view.y()};
  view_rect.max = {view.bottom_right().x(), view.bottom_right().y()};
  params.view = view_rect;
  fuchsia::ui::pointer::Rectangle viewport_rect;
  viewport_rect.min = {viewport.x(), viewport.y()};
  viewport_rect.max = {viewport.bottom_right().x(),
                       viewport.bottom_right().y()};
  params.viewport = viewport_rect;
  params.viewport_to_view_transform = transform;
  return params;
}

}  // namespace
TouchEventBuilder::TouchEventBuilder() = default;

TouchEventBuilder::~TouchEventBuilder() = default;

TouchEventBuilder& TouchEventBuilder::SetTime(zx::time time) {
  time_ = time;
  return *this;
}

TouchEventBuilder& TouchEventBuilder::IncrementTime() {
  static zx::time incrementing_time(0);
  incrementing_time += zx::nsec(1111789u);
  time_ = incrementing_time;
  return *this;
}

TouchEventBuilder& TouchEventBuilder::SetId(
    fuchsia::ui::pointer::TouchInteractionId id) {
  id_ = id;
  return *this;
}

TouchEventBuilder& TouchEventBuilder::SetPhase(
    fuchsia::ui::pointer::EventPhase phase) {
  phase_ = phase;
  return *this;
}

TouchEventBuilder& TouchEventBuilder::SetPosition(gfx::PointF position) {
  position_ = position;
  return *this;
}

TouchEventBuilder& TouchEventBuilder::SetView(gfx::RectF view) {
  view_ = view;
  return *this;
}

TouchEventBuilder& TouchEventBuilder::SetViewport(gfx::RectF viewport) {
  viewport_ = viewport;
  return *this;
}

TouchEventBuilder& TouchEventBuilder::SetTransform(
    std::array<float, 9> transform) {
  transform_ = transform;
  return *this;
}

TouchEventBuilder& TouchEventBuilder::SetTouchInteractionStatus(
    fup::TouchInteractionStatus touch_interaction_status) {
  touch_interaction_status_ = touch_interaction_status;
  return *this;
}

TouchEventBuilder& TouchEventBuilder::WithoutSample() {
  include_sample_ = false;
  return *this;
}

fup::TouchPointerSample TouchEventBuilder::BuildSample() const {
  fup::TouchPointerSample sample;
  sample.set_interaction(id_);
  sample.set_phase(phase_);
  sample.set_position_in_viewport({position_.x(), position_.y()});
  return sample;
}

fup::TouchInteractionResult TouchEventBuilder::BuildResult() const {
  return {id_, touch_interaction_status_.value()};
}

fup::TouchEvent TouchEventBuilder::Build() const {
  fup::TouchEvent event;
  event.set_timestamp(time_.get());
  event.set_view_parameters(CreateViewParameters(view_, viewport_, transform_));
  if (include_sample_) {
    event.set_pointer_sample(BuildSample());
  }
  if (touch_interaction_status_) {
    event.set_interaction_result(BuildResult());
  }
  event.set_trace_flow_id(123);
  return event;
}

MouseEventBuilder::MouseEventBuilder() = default;

MouseEventBuilder::~MouseEventBuilder() = default;

MouseEventBuilder& MouseEventBuilder::SetTime(zx::time time) {
  time_ = time;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::IncrementTime() {
  static zx::time incrementing_time(0u);
  incrementing_time += zx::nsec(1111789u);
  time_ = incrementing_time;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::SetDeviceId(uint32_t device_id) {
  device_id_ = device_id;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::SetView(gfx::RectF view) {
  view_ = view;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::SetViewport(gfx::RectF viewport) {
  viewport_ = viewport;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::SetTransform(
    std::array<float, 9> transform) {
  transform_ = transform;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::SetButtons(std::vector<uint8_t> buttons) {
  buttons_ = buttons;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::SetPosition(gfx::PointF position) {
  position_ = position;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::SetPressedButtons(
    std::vector<uint8_t> pressed_buttons) {
  pressed_buttons_ = pressed_buttons;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::SetScroll(Scroll scroll) {
  scroll_ = scroll;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::SetScrollInPhysicalPixel(
    Scroll scroll_in_physical_pixel) {
  scroll_in_physical_pixel_ = scroll_in_physical_pixel;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::SetIsPrecisionScroll(
    bool is_precision_scroll) {
  is_precision_scroll_ = is_precision_scroll;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::WithoutDeviceInfo() {
  include_device_info_ = false;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::WithoutViewParameters() {
  include_view_parameters_ = false;
  return *this;
}

fup::MousePointerSample MouseEventBuilder::MouseEventBuilder::BuildSample()
    const {
  fup::MousePointerSample sample;
  sample.set_device_id(device_id_);
  if (!pressed_buttons_.empty()) {
    sample.set_pressed_buttons(pressed_buttons_);
  }
  sample.set_position_in_viewport({position_.x(), position_.y()});
  if (scroll_.horizontal != 0) {
    sample.set_scroll_h(scroll_.horizontal);
  }
  if (scroll_.vertical != 0) {
    sample.set_scroll_v(scroll_.vertical);
  }
  if (scroll_in_physical_pixel_.horizontal != 0) {
    sample.set_scroll_h_physical_pixel(scroll_in_physical_pixel_.horizontal);
  }
  if (scroll_in_physical_pixel_.vertical != 0) {
    sample.set_scroll_v_physical_pixel(scroll_in_physical_pixel_.vertical);
  }
  sample.set_is_precision_scroll(is_precision_scroll_);
  return sample;
}

fup::MouseDeviceInfo MouseEventBuilder::BuildDeviceInfo() const {
  fup::MouseDeviceInfo device_info;
  device_info.set_id(device_id_);
  device_info.set_buttons(buttons_);
  return device_info;
}

fup::MouseEvent MouseEventBuilder::Build() const {
  fup::MouseEvent event;
  event.set_timestamp(time_.get());
  if (include_view_parameters_) {
    event.set_view_parameters(
        CreateViewParameters(view_, viewport_, transform_));
  }
  event.set_pointer_sample(BuildSample());
  if (include_device_info_) {
    event.set_device_info(BuildDeviceInfo());
  }
  event.set_trace_flow_id(123);
  return event;
}

}  // namespace ui
