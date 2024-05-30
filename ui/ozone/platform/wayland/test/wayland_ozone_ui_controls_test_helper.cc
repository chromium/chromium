// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/wayland_ozone_ui_controls_test_helper.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace ui {

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperWayland() {
  return new wl::WaylandOzoneUIControlsTestHelper();
}

}  // namespace ui

namespace wl {

namespace {

uint32_t GetNextRequestId() {
  static uint32_t id = 0;
  return id++;
}

}  // namespace

WaylandOzoneUIControlsTestHelper::WaylandOzoneUIControlsTestHelper() {
  auto request_processed_callback =
      base::BindRepeating(&WaylandOzoneUIControlsTestHelper::RequestProcessed,
                          weak_factory_.GetWeakPtr());
  input_emulate_ = std::make_unique<WaylandInputEmulate>(
      std::move(request_processed_callback));
}

WaylandOzoneUIControlsTestHelper::~WaylandOzoneUIControlsTestHelper() = default;

void WaylandOzoneUIControlsTestHelper::Reset() {
  // There's nothing to do here, as the both Exo and Weston automatically reset
  // the state when we close the connection.
  // TODO(crbug.com/40235082): do we still need this method after the switch to
  // ui-controls instead of weston-test is complete?
}

bool WaylandOzoneUIControlsTestHelper::SupportsScreenCoordinates() const {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return true;
#else
  return false;
#endif
}

unsigned WaylandOzoneUIControlsTestHelper::ButtonDownMask() const {
  // This code only runs on Lacros and desktop Linux Wayland, where we always
  // use SendMouseMotionNotifyEvent instead of calling MoveCursorTo via
  // aura::Window, regardless of what the button down mask is.

  NOTREACHED_IN_MIGRATION();
  return 0;
}

void WaylandOzoneUIControlsTestHelper::SendKeyEvents(
    gfx::AcceleratedWidget widget,
    ui::KeyboardCode key,
    int key_event_types,
    int accelerator_state,
    base::OnceClosure closure) {
  CHECK(!(accelerator_state & ui_controls::kCommand))
      << "No Command key on Wayland";

  uint32_t request_id = GetNextRequestId();
  pending_closures_.insert_or_assign(request_id, std::move(closure));
  input_emulate_->EmulateKeyboardKey(UsLayoutKeyboardCodeToDomCode(key),
                                     key_event_types, accelerator_state,
                                     request_id);
}

void WaylandOzoneUIControlsTestHelper::SendMouseMotionNotifyEvent(
    gfx::AcceleratedWidget widget,
    const gfx::Point& mouse_loc,
    const gfx::Point& mouse_loc_in_screen,
    base::OnceClosure closure) {
  uint32_t request_id = GetNextRequestId();

  pending_closures_.insert_or_assign(request_id, std::move(closure));
  input_emulate_->EmulatePointerMotion(widget, mouse_loc, mouse_loc_in_screen,
                                       request_id);
}

void WaylandOzoneUIControlsTestHelper::SendMouseEvent(
    gfx::AcceleratedWidget widget,
    ui_controls::MouseButton type,
    int button_state,
    int accelerator_state,
    const gfx::Point& mouse_loc,
    const gfx::Point& mouse_loc_in_screen,
    base::OnceClosure closure) {
  uint32_t request_id = GetNextRequestId();

  pending_closures_.insert_or_assign(request_id, std::move(closure));
  input_emulate_->EmulatePointerButton(type, button_state, accelerator_state,
                                       request_id);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void WaylandOzoneUIControlsTestHelper::SendTouchEvent(
    gfx::AcceleratedWidget widget,
    int action,
    int id,
    const gfx::Point& touch_loc,
    base::OnceClosure closure) {
  uint32_t request_id = GetNextRequestId();

  pending_closures_.insert_or_assign(request_id, std::move(closure));
  input_emulate_->EmulateTouch(action, touch_loc, id, request_id);
}

void WaylandOzoneUIControlsTestHelper::UpdateDisplay(
    const std::string& display_specs,
    base::OnceClosure closure) {
  uint32_t request_id = GetNextRequestId();
  pending_closures_.insert_or_assign(request_id, std::move(closure));
  input_emulate_->EmulateUpdateDisplay(display_specs, request_id);
}
#endif

void WaylandOzoneUIControlsTestHelper::RunClosureAfterAllPendingUIEvents(
    base::OnceClosure closure) {
  NOTREACHED_IN_MIGRATION();
}

bool WaylandOzoneUIControlsTestHelper::MustUseUiControlsForMoveCursorTo() {
  return true;
}

#if BUILDFLAG(IS_LINUX)
void WaylandOzoneUIControlsTestHelper::ForceUseScreenCoordinatesOnce() {
  input_emulate_->ForceUseScreenCoordinatesOnce();
}
#endif

void WaylandOzoneUIControlsTestHelper::RequestProcessed(uint32_t request_id) {
  // The Wayland base protocol does not map cleanly onto ui_controls semantics.
  // We need to wait for a Wayland round-trip to ensure that all side-effects
  // have been processed. See https://crbug.com/1336706#c11 and
  // https://crbug.com/1443374#c3 for details.
  wl::WaylandProxy::GetInstance()->RoundTripQueue();

  if (base::Contains(pending_closures_, request_id)) {
    if (!pending_closures_[request_id].is_null()) {
      // PostTask to avoid re-entrancy.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(pending_closures_[request_id]));
    }
    pending_closures_.erase(request_id);
  }
}

}  // namespace wl
