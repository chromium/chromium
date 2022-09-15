// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/fuchsia/fakes/pointer_event_utility.h"
#include <utility>

namespace ui {

namespace fup = fuchsia::ui::pointer;

namespace {

fup::ViewParameters CreateViewParameters(
    std::array<std::array<float, 2>, 2> view,
    std::array<std::array<float, 2>, 2> viewport,
    std::array<float, 9> transform) {
  fup::ViewParameters params;
  fuchsia::ui::pointer::Rectangle view_rect;
  view_rect.min = view[0];
  view_rect.max = view[1];
  params.view = view_rect;
  fuchsia::ui::pointer::Rectangle viewport_rect;
  viewport_rect.min = viewport[0];
  viewport_rect.max = viewport[1];
  params.viewport = viewport_rect;
  params.viewport_to_view_transform = transform;
  return params;
}

}  // namespace
TouchEventBuilder::TouchEventBuilder() = default;

TouchEventBuilder::~TouchEventBuilder() = default;

TouchEventBuilder& TouchEventBuilder::AddTime(zx_time_t time) {
  time_ = time;
  return *this;
}

TouchEventBuilder& TouchEventBuilder::AddSample(fup::TouchInteractionId id,
                                                fup::EventPhase phase,
                                                std::array<float, 2> position) {
  sample_ = absl::make_optional<fup::TouchPointerSample>();
  sample_->set_interaction(id);
  sample_->set_phase(phase);
  sample_->set_position_in_viewport(position);
  return *this;
}

TouchEventBuilder& TouchEventBuilder::AddViewParameters(
    std::array<std::array<float, 2>, 2> view,
    std::array<std::array<float, 2>, 2> viewport,
    std::array<float, 9> transform) {
  params_ = CreateViewParameters(std::move(view), std::move(viewport),
                                 std::move(transform));
  return *this;
}

TouchEventBuilder& TouchEventBuilder::AddResult(
    fup::TouchInteractionResult result) {
  result_ = result;
  return *this;
}

fup::TouchEvent TouchEventBuilder::Build() {
  fup::TouchEvent event;
  if (time_) {
    event.set_timestamp(time_.value());
  }
  if (params_) {
    event.set_view_parameters(std::move(params_.value()));
  }
  if (sample_) {
    event.set_pointer_sample(std::move(sample_.value()));
  }
  if (result_) {
    event.set_interaction_result(std::move(result_.value()));
  }
  event.set_trace_flow_id(123);
  return event;
}

std::vector<fup::TouchEvent> TouchEventBuilder::BuildAsVector() {
  std::vector<fup::TouchEvent> events;
  events.emplace_back(Build());
  return events;
}

MouseEventBuilder::MouseEventBuilder() = default;

MouseEventBuilder::~MouseEventBuilder() = default;

MouseEventBuilder& MouseEventBuilder::AddTime(zx_time_t time) {
  time_ = time;
  return *this;
}

MouseEventBuilder& MouseEventBuilder::AddSample(
    uint32_t id,
    std::array<float, 2> position,
    std::vector<uint8_t> pressed_buttons,
    std::array<int64_t, 2> scroll,
    std::array<int64_t, 2> scroll_in_physical_pixel,
    bool is_precision_scroll) {
  sample_ = absl::make_optional<fup::MousePointerSample>();
  sample_->set_device_id(id);
  if (!pressed_buttons.empty()) {
    sample_->set_pressed_buttons(pressed_buttons);
  }
  sample_->set_position_in_viewport(position);
  if (scroll[0] != 0) {
    sample_->set_scroll_h(scroll[0]);
  }
  if (scroll[1] != 0) {
    sample_->set_scroll_v(scroll[1]);
  }
  if (scroll_in_physical_pixel[0] != 0) {
    sample_->set_scroll_h_physical_pixel(scroll_in_physical_pixel[0]);
  }
  if (scroll_in_physical_pixel[1] != 0) {
    sample_->set_scroll_v_physical_pixel(scroll_in_physical_pixel[1]);
  }
  sample_->set_is_precision_scroll(is_precision_scroll);
  return *this;
}

MouseEventBuilder& MouseEventBuilder::AddViewParameters(
    std::array<std::array<float, 2>, 2> view,
    std::array<std::array<float, 2>, 2> viewport,
    std::array<float, 9> transform) {
  params_ = CreateViewParameters(std::move(view), std::move(viewport),
                                 std::move(transform));
  return *this;
}

MouseEventBuilder& MouseEventBuilder::AddMouseDeviceInfo(
    uint32_t id,
    std::vector<uint8_t> buttons) {
  device_info_ = absl::make_optional<fup::MouseDeviceInfo>();
  device_info_->set_id(id);
  device_info_->set_buttons(buttons);
  return *this;
}

fup::MouseEvent MouseEventBuilder::Build() {
  fup::MouseEvent event;
  if (time_) {
    event.set_timestamp(time_.value());
  }
  if (params_) {
    event.set_view_parameters(std::move(params_.value()));
  }
  if (sample_) {
    event.set_pointer_sample(std::move(sample_.value()));
  }
  if (device_info_) {
    event.set_device_info(std::move(device_info_.value()));
  }
  event.set_trace_flow_id(123);
  return event;
}

std::vector<fup::MouseEvent> MouseEventBuilder::BuildAsVector() {
  std::vector<fup::MouseEvent> events;
  events.emplace_back(Build());
  return events;
}

}  // namespace ui
