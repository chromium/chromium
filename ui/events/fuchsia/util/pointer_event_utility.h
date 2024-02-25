// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_FUCHSIA_UTIL_POINTER_EVENT_UTILITY_H_
#define UI_EVENTS_FUCHSIA_UTIL_POINTER_EVENT_UTILITY_H_

#include <fidl/fuchsia.ui.pointer/cpp/fidl.h>
#include <ui/gfx/geometry/point_f.h>
#include <ui/gfx/geometry/rect_f.h>
#include <zircon/types.h>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace {
constexpr gfx::RectF kRect(0, 0, 20, 20);
constexpr std::array<float, 9> kIdentity = {1, 0, 0, 0, 1, 0, 0, 0, 1};
const fuchsia_ui_pointer::TouchInteractionId kIxnOne = {
    {.device_id = 1u, .pointer_id = 1u, .interaction_id = 2u}};
constexpr uint32_t kMouseDeviceId = 123;
}  // namespace

namespace ui {

// A helper class for crafting a fuchsia.ui.pointer.TouchEvent table.
class TouchEventBuilder {
 public:
  TouchEventBuilder();
  ~TouchEventBuilder();

  TouchEventBuilder& SetTime(zx::time time);
  TouchEventBuilder& IncrementTime();
  TouchEventBuilder& SetId(fuchsia_ui_pointer::TouchInteractionId id);
  TouchEventBuilder& SetPhase(fuchsia_ui_pointer::EventPhase phase);
  TouchEventBuilder& SetPosition(gfx::PointF position);
  TouchEventBuilder& SetView(gfx::RectF view);
  TouchEventBuilder& SetViewport(gfx::RectF viewport);
  TouchEventBuilder& SetTransform(std::array<float, 9> transform);
  TouchEventBuilder& SetTouchInteractionStatus(
      fuchsia_ui_pointer::TouchInteractionStatus touch_interaction_status);

  TouchEventBuilder& WithoutSample();

  fuchsia_ui_pointer::TouchEvent Build() const;

 private:
  fuchsia_ui_pointer::TouchPointerSample BuildSample() const;
  fuchsia_ui_pointer::TouchInteractionResult BuildResult() const;

  zx::time time_{1u};
  fuchsia_ui_pointer::TouchInteractionId id_ = kIxnOne;
  fuchsia_ui_pointer::EventPhase phase_ = fuchsia_ui_pointer::EventPhase::kAdd;
  gfx::PointF position_ = {10.f, 10.f};
  gfx::RectF view_ = kRect;
  gfx::RectF viewport_ = kRect;
  std::array<float, 9> transform_ = kIdentity;
  std::optional<fuchsia_ui_pointer::TouchInteractionStatus>
      touch_interaction_status_;

  bool include_sample_ = true;
};

struct Scroll {
  int64_t horizontal;
  int64_t vertical;
};

// A helper class for crafting a fuchsia.ui.pointer.MouseEvent table.
class MouseEventBuilder {
 public:
  MouseEventBuilder();
  ~MouseEventBuilder();

  MouseEventBuilder& SetTime(zx::time time);
  MouseEventBuilder& IncrementTime();
  MouseEventBuilder& SetDeviceId(uint32_t device_id);
  MouseEventBuilder& SetView(gfx::RectF view);
  MouseEventBuilder& SetViewport(gfx::RectF viewport);
  MouseEventBuilder& SetTransform(std::array<float, 9> transform);
  MouseEventBuilder& SetButtons(std::vector<uint8_t> buttons);
  MouseEventBuilder& SetPosition(gfx::PointF position);
  MouseEventBuilder& SetPressedButtons(std::vector<uint8_t> pressed_buttons);
  MouseEventBuilder& SetScroll(Scroll scroll);
  MouseEventBuilder& SetScrollInPhysicalPixel(Scroll scroll_in_physical_pixel);
  MouseEventBuilder& SetIsPrecisionScroll(bool is_precision_scroll);

  MouseEventBuilder& WithoutDeviceInfo();
  MouseEventBuilder& WithoutViewParameters();

  fuchsia_ui_pointer::MouseEvent Build() const;

 private:
  fuchsia_ui_pointer::MousePointerSample BuildSample() const;
  fuchsia_ui_pointer::ViewParameters BuildViewParameters() const;
  fuchsia_ui_pointer::MouseDeviceInfo BuildDeviceInfo() const;

  zx::time time_{1u};
  uint32_t device_id_ = kMouseDeviceId;
  gfx::RectF view_ = kRect;
  gfx::RectF viewport_ = kRect;
  std::array<float, 9> transform_ = kIdentity;
  std::vector<uint8_t> buttons_ = {0, 1, 2};
  gfx::PointF position_ = {0, 0};
  std::vector<uint8_t> pressed_buttons_ = {};
  Scroll scroll_ = {.horizontal = 0, .vertical = 0};
  Scroll scroll_in_physical_pixel_ = {.horizontal = 0, .vertical = 0};
  bool is_precision_scroll_ = false;

  bool include_device_info_ = true;
  bool include_view_parameters_ = true;
};

}  // namespace ui

#endif  // UI_EVENTS_FUCHSIA_UTIL_POINTER_EVENT_UTILITY_H_
