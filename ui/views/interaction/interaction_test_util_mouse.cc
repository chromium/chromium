// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interaction_test_util_mouse.h"

#include <memory>
#include <utility>
#include <variant>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/contains.h"
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
#include "ui/views/widget/widget.h"

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
raw_ptr<InteractionTestUtilMouse> g_current_mouse_util = nullptr;

void PostTask(base::OnceClosure task) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              std::move(task));
}

#if TOUCH_INPUT_SUPPORTED

ui_controls::TouchType GetTouchAction(ui_controls::MouseButtonState state) {
  switch (state) {
    case ui_controls::DOWN:
      return ui_controls::kTouchPress;
    case ui_controls::UP:
      return ui_controls::kTouchRelease;
  }
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
    gfx::NativeWindow window_hint_,
    bool force_async_)
    : window_hint(window_hint_), force_async(force_async_) {}
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

InteractionTestUtilMouse::InteractionTestUtilMouse(views::Widget* widget)
    : InteractionTestUtilMouse(widget->GetNativeWindow()) {}

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
InteractionTestUtilMouse::MouseGestures InteractionTestUtilMouse::Click(
    ui_controls::MouseButton button,
    int modifier_keys) {
  return MouseGestures{MouseDown(button, modifier_keys),
                       MouseUp(button, modifier_keys)};
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
  if (!params.force_async) {
#if TOUCH_INPUT_SUPPORTED
    if (touch_mode_) {
      return ui_controls::SendTouchEventsNotifyWhenDone(
          GetTouchAction(gesture.button_state), GetTouchCount(gesture.button),
          touch_hover_point_.x(), touch_hover_point_.y(),
          std::move(on_complete));
    }
#endif  // TOUCH_INPUT_SUPPORTED
    return ui_controls::SendMouseEventsNotifyWhenDone(
        gesture.button, gesture.button_state, std::move(on_complete),
        gesture.modifier_keys, params.window_hint);
  }

#if TOUCH_INPUT_SUPPORTED
  if (touch_mode_) {
    PostTask(base::BindOnce(
        [](base::WeakPtr<InteractionTestUtilMouse> util,
           base::OnceClosure on_complete, MouseButtonGesture gesture,
           gfx::Point target) {
          if (!util) {
            return;
          }
          CHECK(ui_controls::SendTouchEventsNotifyWhenDone(
              GetTouchAction(gesture.button_state),
              GetTouchCount(gesture.button), target.x(), target.y(),
              std::move(on_complete)));
        },
        weak_ptr_factory_.GetWeakPtr(), std::move(on_complete), gesture,
        touch_hover_point_));
    return true;
  }
#endif  // TOUCH_INPUT_SUPPORTED

  PostTask(base::BindOnce(
      [](base::WeakPtr<InteractionTestUtilMouse> util,
         base::OnceClosure on_complete, MouseButtonGesture gesture,
         gfx::NativeWindow window_hint) {
        if (!util) {
          return;
        }
        CHECK(ui_controls::SendMouseEventsNotifyWhenDone(
            gesture.button, gesture.button_state, std::move(on_complete),
            gesture.modifier_keys, window_hint));
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(on_complete), gesture,
      params.window_hint));

  return true;
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
  if (!params.force_async) {
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

#if TOUCH_INPUT_SUPPORTED
  if (touch_mode_) {
    PostTask(base::BindOnce(
        [](base::WeakPtr<InteractionTestUtilMouse> util,
           base::OnceClosure on_complete, MouseMoveGesture gesture) {
          if (!util) {
            return;
          }
          const int touch_count = GetTouchCount(*util->buttons_down_.begin());
          CHECK(ui_controls::SendTouchEventsNotifyWhenDone(
              ui_controls::kTouchMove, touch_count, gesture.x(), gesture.y(),
              std::move(on_complete)));
        },
        weak_ptr_factory_.GetWeakPtr(), std::move(on_complete), gesture));
    return true;
  }
#endif  // TOUCH_INPUT_SUPPORTED

  PostTask(base::BindOnce(
      [](base::WeakPtr<InteractionTestUtilMouse> util,
         base::OnceClosure on_complete, MouseMoveGesture gesture,
         gfx::NativeWindow window_hint) {
        if (!util) {
          return;
        }
        CHECK(ui_controls::SendMouseMoveNotifyWhenDone(
            gesture.x(), gesture.y(), std::move(on_complete), window_hint));
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(on_complete), gesture,
      params.window_hint));

  return true;
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

bool InteractionTestUtilMouse::PerformGesturesImpl(const GestureParams& params,
                                                   MouseGestures gestures) {
  CHECK(!gestures.empty());
  CHECK(!performing_gestures_);
  base::AutoReset<bool> performing_gestures(&performing_gestures_, true);
  canceled_ = false;
  for (auto& gesture : gestures) {
    if (canceled_) {
      break;
    }

    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    if (MouseButtonGesture* const button =
            std::get_if<MouseButtonGesture>(&gesture)) {
      switch (button->button_state) {
        case ui_controls::UP: {
          CHECK(buttons_down_.erase(button->button));
          base::OnceClosure on_complete =
              params.force_async ? base::DoNothing() : run_loop.QuitClosure();
          if (ShouldCancelDrag()) {
            // This will bail out of any nested drag-drop run loop, allowing
            // the code to proceed even if the drag somehow starts while the
            // mouse-up is being processed.
            on_complete = std::move(on_complete)
                              .Then(base::BindOnce(
                                  &InteractionTestUtilMouse::CancelFutureDrag,
                                  weak_ptr_factory_.GetWeakPtr()));
          }
#if defined(USE_AURA)
          dragging_ = false;
#endif
          if (!SendButtonPress(*button, params, std::move(on_complete))) {
            LOG(ERROR) << "Mouse button " << button->button << " up failed.";
            return false;
          }
          if (!params.force_async) {
            run_loop.Run();
          }
          break;
        }
        case ui_controls::DOWN:
#if TOUCH_INPUT_SUPPORTED
          CHECK(!touch_mode_ || buttons_down_.empty())
              << "In touch mode, only one set of fingers may be down at any "
                 "given time.";
#endif
          CHECK(buttons_down_.insert(button->button).second);
#if BUILDFLAG(IS_MAC)
          if (!params.force_async && button->button == ui_controls::RIGHT) {
            LOG(WARNING)
                << "InteractionTestUtilMouse::PerformGestures() - "
                   "Important note:\n"
                << "Because right-clicking on Mac typically results in a "
                   "context menu, and because some (but not all) context menus "
                   "on Mac are native and take over the main message loop, "
                   "right-clicking could cause the test to hang.\n"
                << "If you notice your test hangs on Mac, use "
                   "MayInvolveNativeContextMenu() and minimize the number of "
                   "test "
                   "steps performed while the context menu is open.";
          }
#endif
          CancelDragNow();
          if (!SendButtonPress(*button, params,
                               params.force_async ? base::DoNothing()
                                                  : run_loop.QuitClosure())) {
            LOG(ERROR) << "Mouse button " << button->button << " down failed.";
            return false;
          }

          if (!params.force_async) {
            run_loop.Run();
          }
          break;
      }
    } else {
      const auto& move = std::get<MouseMoveGesture>(gesture);
#if defined(USE_AURA)
      if (!buttons_down_.empty()) {
        CHECK(base::Contains(buttons_down_, ui_controls::LEFT));
        dragging_ = true;
      }
#endif
      if (!SendMove(move, params,
                    params.force_async ? base::DoNothing()
                                       : run_loop.QuitClosure())) {
        LOG(ERROR) << "Mouse move to " << move.ToString() << " failed.";
        return false;
      }

      if (!params.force_async) {
        run_loop.Run();
      }
    }
  }

  return !canceled_;
}

void InteractionTestUtilMouse::CancelAllGestures() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  canceled_ = true;

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
