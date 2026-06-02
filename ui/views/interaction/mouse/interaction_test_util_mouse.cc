// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/mouse/interaction_test_util_mouse.h"

#include <memory>
#include <utility>
#include <variant>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/native_ui_types.h"

#if defined(USE_AURA)
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#endif  // defined(USE_AURA)

// Currently, touch is only supported on ChromeOS Ash.
#if BUILDFLAG(IS_CHROMEOS)
#define TOUCH_INPUT_SUPPORTED 1
#else
#define TOUCH_INPUT_SUPPORTED 0
#endif

namespace views::test {

namespace {
InteractionTestUtilMouse* g_current_mouse_util = nullptr;

#if defined(USE_AURA)
void PostTask(base::OnceClosure task) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              std::move(task));
}
#endif  // defined(USE_AURA)

#if TOUCH_INPUT_SUPPORTED

int GetTouchAction(int state) {
  int action = 0;
  if (state & ui_controls::DOWN) {
    action |= ui_controls::kTouchPress;
  }
  if (state & ui_controls::UP) {
    action |= ui_controls::kTouchRelease;
  }
  return action;
}

int GetTouchCount(ui_controls::MouseButton button) {
  switch (button) {
    case ui_controls::LEFT:
      return 1;
    case ui_controls::MIDDLE:
      return 3;
    case ui_controls::RIGHT:
      return 2;
  }
}

#endif  // TOUCH_INPUT_SUPPORTED

}  // namespace

InteractionTestUtilMouse::GestureParams::GestureParams() = default;
InteractionTestUtilMouse::GestureParams::GestureParams(
    gfx::NativeWindow window_hint_)
    : window_hint(window_hint_) {}
InteractionTestUtilMouse::GestureParams::GestureParams(const GestureParams&) =
    default;
InteractionTestUtilMouse::GestureParams&
InteractionTestUtilMouse::GestureParams::operator=(const GestureParams&) =
    default;
InteractionTestUtilMouse::GestureParams::GestureParams(
    GestureParams&&) noexcept = default;
InteractionTestUtilMouse::GestureParams&
InteractionTestUtilMouse::GestureParams::operator=(GestureParams&&) noexcept =
    default;
InteractionTestUtilMouse::GestureParams::~GestureParams() = default;

#if defined(USE_AURA)

// Ends any drag currently in progress or that starts during this object's
// lifetime. This is needed because the drag controller can get out of sync with
// mouse event handling - especially when running ChromeOS-on-Linux. This can
// result in weird test hangs/timeouts during mouse-up after a drag, or (more
// insidiously) during test shutdown.
//
// Once started, the DragEnder will kill any drags that start until:
//  - Stop() is called.
//  - The Aura window it is watching goes away.
//  - The DragEnder is destroyed (which should happen no earlier than the end of
//    ShutDownOnMainThread()).
class InteractionTestUtilMouse::DragEnder
    : public aura::client::DragDropClientObserver,
      public aura::WindowObserver {
 public:
  explicit DragEnder(aura::Window* window) : window_(window) {
    window_observation_.Observe(window);
  }

  ~DragEnder() override = default;

  // Either cancels a current drag, or starts observing for a future drag start
  // event (at which point the drag will be canceled).
  void Start() {
    if (CancelDragNow() || drag_client_observation_.IsObserving()) {
      return;
    }
    // Only Ash actually supports observing the drag-drop client. Therefore, on
    // other platforms, only direct cancel is possible.
#if BUILDFLAG(IS_CHROMEOS)
    if (auto* const client = GetClient()) {
      drag_client_observation_.Observe(client);
    }
#endif
  }

  // Stops any ongoing observation of drag start events.
  void Stop() { drag_client_observation_.Reset(); }

  // Cancels any drag that is currently happening, but does not watch for future
  // drag start events.
  bool CancelDragNow() {
    if (auto* const client = GetClient()) {
      if (client->IsDragDropInProgress()) {
        LOG(WARNING)
            << "InteractionTestUtilMouse: Force-canceling spurious drag "
               "operation.\n"
            << "This can happen when the drag controller gets out of sync with "
               "mouse events being sent by the test, and is especially likely "
               "on  ChromeOS-on-Linux.\n"
            << "This is not necessarily a serious error if the test functions "
               "normally; however, if you see this too often or your test "
               "flakes as a result of the cancel you may need to restructure "
               "the test so that you can be sure the drag has started before "
               "attempting to invoke ReleaseMouse().";
        client->DragCancel();
        return true;
      }
    }
    return false;
  }

  base::WeakPtr<DragEnder> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // aura::client::DragDropClientObserver:
  void OnDragStarted() override {
    drag_client_observation_.Reset();
    PostTask(base::BindOnce(base::IgnoreResult(&DragEnder::CancelDragNow),
                            weak_ptr_factory_.GetWeakPtr()));
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window_, window);
    drag_client_observation_.Reset();
    window_observation_.Reset();
    window_ = nullptr;
  }

  aura::client::DragDropClient* GetClient() {
    return window_ ? aura::client::GetDragDropClient(window_->GetRootWindow())
                   : nullptr;
  }

  // Since there is no "DragDropClientDestroying" event, use the aura::Window as
  // a proxy for the existence of the DragDropClient, and unregister listeners
  // when the window goes away. If this is not done, UAFs may happen when the
  // scoped observation of the drag client goes away.
  raw_ptr<aura::Window> window_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
  base::ScopedObservation<aura::client::DragDropClient,
                          aura::client::DragDropClientObserver>
      drag_client_observation_{this};
  base::WeakPtrFactory<DragEnder> weak_ptr_factory_{this};
};

#endif  // defined(USE_AURA)

InteractionTestUtilMouse::~InteractionTestUtilMouse() {
  CHECK(!performing_gestures_)
      << "InteractionTestUtilMouse destroyed with pending actions.";
  LOG_IF(ERROR, g_current_mouse_util != this)
      << "Expected |this| to be current InteractionTestUtilMouse.";
  g_current_mouse_util = nullptr;
}

// static
InteractionTestUtilMouse::MouseGesture InteractionTestUtilMouse::MoveTo(
    gfx::Point point) {
  return MouseGesture(point);
}

// static
InteractionTestUtilMouse::MouseGesture InteractionTestUtilMouse::MouseDown(
    ui_controls::MouseButton button,
    int modifier_keys) {
  return MouseButtonGesture(button, ui_controls::DOWN, modifier_keys);
}

// static
InteractionTestUtilMouse::MouseGesture InteractionTestUtilMouse::MouseUp(
    ui_controls::MouseButton button,
    int modifier_keys) {
  return MouseButtonGesture(button, ui_controls::UP, modifier_keys);
}

// static
InteractionTestUtilMouse::MouseGesture InteractionTestUtilMouse::Click(
    ui_controls::MouseButton button,
    int modifier_keys) {
  return MouseButtonGesture(button, ui_controls::UP | ui_controls::DOWN,
                            modifier_keys);
}

// static
InteractionTestUtilMouse::MouseGestures InteractionTestUtilMouse::DragAndHold(
    gfx::Point destination) {
  return MouseGestures{MouseDown(ui_controls::LEFT), MoveTo(destination)};
}

// static
InteractionTestUtilMouse::MouseGestures
InteractionTestUtilMouse::DragAndRelease(gfx::Point destination) {
  return MouseGestures{MouseDown(ui_controls::LEFT), MoveTo(destination),
                       MouseUp(ui_controls::LEFT)};
}

bool InteractionTestUtilMouse::ShouldCancelDrag() const {
#if defined(USE_AURA)
  return dragging_;
#else
  return false;
#endif
}

void InteractionTestUtilMouse::CancelFutureDrag() {
#if defined(USE_AURA)
  // Allow the system to finish processing any mouse input before force-
  // canceling any ongoing drag. It's possible that a drag that was queued to
  // complete simply hasn't yet.
  PostTask(base::BindOnce(&DragEnder::Start, drag_ender_->GetWeakPtr()));
#endif
}

void InteractionTestUtilMouse::CancelDragNow() {
#if defined(USE_AURA)
  CHECK(!dragging_);
  drag_ender_->Stop();
  drag_ender_->CancelDragNow();
#endif
}

bool InteractionTestUtilMouse::SendButtonPress(
    const MouseButtonGesture& gesture,
    const GestureParams& params,
    base::OnceClosure on_complete) {
#if TOUCH_INPUT_SUPPORTED
  if (touch_mode_) {
    return ui_controls::SendTouchEventsNotifyWhenDone(
        GetTouchAction(gesture.button_state), GetTouchCount(gesture.button),
        touch_hover_point_.x(), touch_hover_point_.y(), std::move(on_complete));
  }
#endif  // TOUCH_INPUT_SUPPORTED
  return ui_controls::SendMouseEventsNotifyWhenDone(
      gesture.button, gesture.button_state, std::move(on_complete),
      gesture.modifier_keys, params.window_hint);
}

bool InteractionTestUtilMouse::SendMove(const MouseMoveGesture& gesture,
                                        const GestureParams& params,
                                        base::OnceClosure on_complete) {
#if TOUCH_INPUT_SUPPORTED
  if (touch_mode_) {
    // Need to remember where our finger is.
    touch_hover_point_ = gesture;
    // If no fingers are down, there's nothing to do.
    if (buttons_down_.empty()) {
      std::move(on_complete).Run();
      return true;
    }
    // Should never have two different sets of fingers down at once.
    CHECK_EQ(1U, buttons_down_.size());
  }
#endif  // TOUCH_INPUT_SUPPORTED
#if TOUCH_INPUT_SUPPORTED
  if (touch_mode_) {
    return ui_controls::SendTouchEventsNotifyWhenDone(
        ui_controls::kTouchMove, GetTouchCount(*buttons_down_.begin()),
        gesture.x(), gesture.y(), std::move(on_complete));
  }
#endif  // TOUCH_INPUT_SUPPORTED
  return ui_controls::SendMouseMoveNotifyWhenDone(
      gesture.x(), gesture.y(), std::move(on_complete), params.window_hint);
}

bool InteractionTestUtilMouse::SetTouchMode(bool touch_mode) {
  if (touch_mode == touch_mode_) {
    return true;
  }
  CHECK(buttons_down_.empty())
      << "Cannot toggle touch mode when buttons or fingers are down.";
#if TOUCH_INPUT_SUPPORTED
  touch_mode_ = touch_mode;
  return true;
#else
  LOG(WARNING) << "Touch mode not supported on this platform.";
  return false;
#endif
}

bool InteractionTestUtilMouse::GetTouchMode() const {
  return touch_mode_;
}

void InteractionTestUtilMouse::PerformGesturesImpl(
    base::OnceCallback<void(bool)> on_complete,
    const GestureParams& params,
    MouseGestures gestures) {
  CHECK(!gestures.empty());
  CHECK(!performing_gestures_);
  performing_gestures_ = true;
  on_complete_ = std::move(on_complete);
  canceled_ = false;

  PerformNextGesture(params, std::move(gestures));
}

bool InteractionTestUtilMouse::PerformGesturesImpl(const GestureParams& params,
                                                   MouseGestures gestures) {
  base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
  PerformGesturesImpl(base::IgnoreArgs<bool>(run_loop.QuitClosure()), params,
                      std::move(gestures));
  run_loop.Run();
  return !canceled_;
}

void InteractionTestUtilMouse::PerformNextGesture(const GestureParams& params,
                                                  MouseGestures gestures) {
  if (canceled_ || gestures.empty()) {
    performing_gestures_ = false;
    if (on_complete_) {
      std::move(on_complete_).Run(!canceled_);
    }
    return;
  }

  MouseGesture gesture = std::move(gestures.front());
  gestures.pop_front();

  base::OnceClosure step_complete = base::BindOnce(
      &InteractionTestUtilMouse::PerformNextGesture,
      weak_ptr_factory_.GetWeakPtr(), params, std::move(gestures));

  if (MouseButtonGesture* const button =
          std::get_if<MouseButtonGesture>(&gesture)) {
    if (button->button_state & ui_controls::DOWN) {
#if TOUCH_INPUT_SUPPORTED
      CHECK(!touch_mode_ || buttons_down_.empty())
          << "In touch mode, only one set of fingers may be down at any "
             "given time.";
#endif
      // Only update the button state if this isn't a click gesture (i.e. no
      // "up").
      if (!(button->button_state & ui_controls::UP)) {
        CHECK(buttons_down_.insert(button->button).second);
      }
      CancelDragNow();
      if (!SendButtonPress(*button, params, std::move(step_complete))) {
        LOG(ERROR) << "Mouse button " << button->button << " down failed.";
        CancelAllGestures();
        return;
      }
    } else {
      CHECK(button->button_state == ui_controls::UP);
      CHECK(buttons_down_.erase(button->button));
      if (ShouldCancelDrag()) {
        // This will bail out of any nested drag-drop run loop, allowing
        // the code to proceed even if the drag somehow starts while the
        // mouse-up is being processed.
        step_complete = std::move(step_complete)
                            .Then(base::BindOnce(
                                &InteractionTestUtilMouse::CancelFutureDrag,
                                weak_ptr_factory_.GetWeakPtr()));
      }
#if defined(USE_AURA)
      dragging_ = false;
#endif
      if (!SendButtonPress(*button, params, std::move(step_complete))) {
        LOG(ERROR) << "Mouse button " << button->button << " up failed.";
        CancelAllGestures();
        return;
      }
    }
  } else {
    const auto& move = std::get<MouseMoveGesture>(gesture);
#if defined(USE_AURA)
    if (!buttons_down_.empty()) {
      CHECK(buttons_down_.contains(ui_controls::LEFT));
      dragging_ = true;
    }
#endif
    if (!SendMove(move, params, std::move(step_complete))) {
      LOG(ERROR) << "Mouse move to " << move.ToString() << " failed.";
      CancelAllGestures();
      return;
    }
  }
}

void InteractionTestUtilMouse::CancelAllGestures() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  canceled_ = true;
  if (performing_gestures_) {
    performing_gestures_ = false;
    if (on_complete_) {
      std::move(on_complete_).Run(false);
    }
  }

#if TOUCH_INPUT_SUPPORTED
  if (touch_mode_ && !buttons_down_.empty()) {
    // Should never get in a state where multiple finger combinations are down
    // at the same time.
    CHECK_EQ(1U, buttons_down_.size());
    const auto& button = *buttons_down_.begin();
    if (!ui_controls::SendTouchEvents(
            ui_controls::kTouchRelease, GetTouchCount(button),
            touch_hover_point_.x(), touch_hover_point_.y())) {
      LOG(WARNING) << "Unable to send touch up.";
    }
    buttons_down_.clear();
  }
#endif  // TOUCH_INPUT_SUPPORTED

  // Now that no additional actions will happen, release all mouse buttons.
  for (ui_controls::MouseButton button : buttons_down_) {
    if (!ui_controls::SendMouseEvents(button, ui_controls::UP)) {
      LOG(WARNING) << "Unable to release mouse button " << button;
    }
  }
  buttons_down_.clear();

  // Maybe handle dragging stopped.
  if (ShouldCancelDrag()) {
    CancelFutureDrag();
  }
}

InteractionTestUtilMouse::InteractionTestUtilMouse(gfx::NativeWindow window)
#if defined(USE_AURA)
    : drag_ender_(std::make_unique<DragEnder>(window))
#endif
{
  CHECK(window);
  CHECK(!g_current_mouse_util)
      << "Cannot have multiple overlapping InteractionTestUtilMouse instances";
  g_current_mouse_util = this;
}

// static
void InteractionTestUtilMouse::AddGestures(MouseGestures& gestures,
                                           MouseGesture to_add) {
  gestures.emplace_back(std::move(to_add));
}

// static
void InteractionTestUtilMouse::AddGestures(MouseGestures& gestures,
                                           MouseGestures to_add) {
  for (auto& gesture : to_add) {
    gestures.emplace_back(std::move(gesture));
  }
}

}  // namespace views::test
