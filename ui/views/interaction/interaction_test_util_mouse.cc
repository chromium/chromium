// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interaction_test_util_mouse.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#endif  // defined(USE_AURA)

namespace views::test {

namespace {
base::raw_ptr<InteractionTestUtilMouse> g_current_mouse_util = nullptr;
}

#if defined(USE_AURA)

// Ends any drag currently in progress or that starts during this object's
// lifetime.
class InteractionTestUtilMouse::DragEnder
    : public aura::client::DragDropClientObserver {
 public:
  explicit DragEnder(aura::Window* window)
      : client_(aura::client::GetDragDropClient(window->GetRootWindow())) {
    if (!EndDrag(window))
      scoped_observation_.Observe(client_);
  }
  ~DragEnder() override = default;

  static bool EndDrag(aura::Window* window) {
    auto* const client =
        aura::client::GetDragDropClient(window->GetRootWindow());
    if (client->IsDragDropInProgress()) {
      client->DragCancel();
      return true;
    }
    return false;
  }

 private:
  // aura::client::DragDropClientObserver:
  void OnDragStarted() override {
    scoped_observation_.Reset();
    PostCancel();
  }

  void PostCancel() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DragEnder::CancelDrag, weak_ptr_factory_.GetWeakPtr()));
  }

  void CancelDrag() { client_->DragCancel(); }

  const base::raw_ptr<aura::client::DragDropClient> client_;
  base::ScopedObservation<aura::client::DragDropClient,
                          aura::client::DragDropClientObserver>
      scoped_observation_{this};
  base::WeakPtrFactory<DragEnder> weak_ptr_factory_{this};
};

// Acts more or less like an aura::Window* except that it becomes falsy/null
// when the window goes away.
class InteractionTestUtilMouse::NativeWindowRef : public aura::WindowObserver {
 public:
  explicit NativeWindowRef(aura::Window* window) : window_(window) {
    if (window)
      scoped_observation_.Observe(window);
  }

  ~NativeWindowRef() override = default;
  NativeWindowRef(const NativeWindowRef&) = delete;
  void operator=(const NativeWindowRef&) = delete;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window_, window);
    window_ = nullptr;
  }

  explicit operator aura::Window*() const { return window_; }
  explicit operator bool() const { return window_; }
  bool operator!() const { return !window_; }

 private:
  base::raw_ptr<aura::Window> window_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      scoped_observation_{this};
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
    ui_controls::MouseButton button) {
  return MouseGesture(std::make_pair(button, ui_controls::DOWN));
}

// static
InteractionTestUtilMouse::MouseGesture InteractionTestUtilMouse::MouseUp(
    ui_controls::MouseButton button) {
  return MouseGesture(std::make_pair(button, ui_controls::UP));
}

// static
InteractionTestUtilMouse::MouseGestures InteractionTestUtilMouse::Click(
    ui_controls::MouseButton button) {
  return MouseGestures{MouseDown(button), MouseUp(button)};
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

void InteractionTestUtilMouse::MaybeCancelDrag(bool in_future) {
#if defined(USE_AURA)
  if (in_future) {
    if (dragging_ && !drag_ender_) {
      if (auto* const window = static_cast<aura::Window*>(*native_window_)) {
        drag_ender_ = std::make_unique<DragEnder>(window);
      }
    }
    dragging_ = false;
  } else {
    CHECK(!dragging_);
    drag_ender_.reset();
    if (aura::Window* const window =
            static_cast<aura::Window*>(*native_window_)) {
      DragEnder::EndDrag(window);
    }
  }
#endif
}

bool InteractionTestUtilMouse::SendButtonPress(
    const MouseButtonGesture& gesture,
    gfx::NativeWindow window_hint,
    base::OnceClosure sync_operation_complete) {
  if (sync_operation_complete) {
    return ui_controls::SendMouseEventsNotifyWhenDone(
        gesture.first, gesture.second, std::move(sync_operation_complete),
        ui_controls::kNoAccelerator, window_hint);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<InteractionTestUtilMouse> util,
             MouseButtonGesture gesture, gfx::NativeWindow window_hint) {
            if (!util) {
              return;
            }
            CHECK(ui_controls::SendMouseEvents(gesture.first, gesture.second,
                                               ui_controls::kNoAccelerator,
                                               window_hint));
          },
          weak_ptr_factory_.GetWeakPtr(), gesture, window_hint));

  return true;
}

bool InteractionTestUtilMouse::SendMove(
    const MouseMoveGesture& gesture,
    gfx::NativeWindow window_hint,
    base::OnceClosure sync_operation_complete) {
  if (sync_operation_complete) {
    return ui_controls::SendMouseMoveNotifyWhenDone(
        gesture.x(), gesture.y(), std::move(sync_operation_complete),
        window_hint);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<InteractionTestUtilMouse> util,
             MouseMoveGesture gesture, gfx::NativeWindow window_hint) {
            if (!util) {
              return;
            }
            CHECK(ui_controls::SendMouseMove(gesture.x(), gesture.y(),
                                             window_hint));
          },
          weak_ptr_factory_.GetWeakPtr(), gesture, window_hint));

  return true;
}

bool InteractionTestUtilMouse::PerformGesturesImpl(
    MouseGestures gestures,
    gfx::NativeWindow window_hint) {
  CHECK(!gestures.empty());
  CHECK(!performing_gestures_);
  base::AutoReset<bool> performing_gestures(&performing_gestures_, true);
  canceled_ = false;
  for (auto& gesture : gestures) {
    if (canceled_)
      break;

    bool force_async = false;
#if BUILDFLAG(IS_MAC)
    force_async = base::Contains(buttons_down_, ui_controls::RIGHT);
#endif

    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    if (MouseButtonGesture* const button =
            absl::get_if<MouseButtonGesture>(&gesture)) {
      switch (button->second) {
        case ui_controls::UP:
          CHECK(buttons_down_.erase(button->first));
          if (!SendButtonPress(*button, window_hint,
                               force_async ? base::NullCallback()
                                           : run_loop.QuitClosure())) {
            LOG(ERROR) << "Mouse button " << button->first << " up failed.";
            return false;
          }
          if (!force_async) {
            run_loop.Run();
          }
          MaybeCancelDrag(true);
          break;
        case ui_controls::DOWN:
          CHECK(buttons_down_.insert(button->first).second);
#if BUILDFLAG(IS_MAC)
          if (!force_async && button->first == ui_controls::RIGHT) {
            force_async = true;
            LOG(WARNING)
                << "InteractionTestUtilMouse::PerformGestures(): "
                   "Important note:\n"
                << "Because right-clicking on Mac typically results in a "
                   "context menu, and because context menus on Mac are native "
                   "and take over the main message loop, mouse events from "
                   "here until release of the right mouse button will be sent "
                   "asynchronously to avoid a hang.\n"
                << "Furthermore, your test will likely still hang unless you "
                   "explicitly find and close the context menu. There is (as "
                   "of the time this warning was written) no general way to do "
                   "this because it requires access to the menu runner, which "
                   "is not always publicly exposed.";
          }
#endif
          MaybeCancelDrag(false);
          if (!SendButtonPress(*button, window_hint,
                               force_async ? base::NullCallback()
                                           : run_loop.QuitClosure())) {
            LOG(ERROR) << "Mouse button " << button->first << " down failed.";
            return false;
          }

          if (!force_async) {
            run_loop.Run();
          }
          break;
      }
    } else {
      const auto& move = absl::get<MouseMoveGesture>(gesture);
#if defined(USE_AURA)
      if (!buttons_down_.empty()) {
        CHECK(base::Contains(buttons_down_, ui_controls::LEFT));
        dragging_ = true;
      }
#endif
      if (!SendMove(
              move, window_hint,
              force_async ? base::NullCallback() : run_loop.QuitClosure())) {
        LOG(ERROR) << "Mouse move to " << move.ToString() << " failed.";
        return false;
      }

      if (!force_async) {
        run_loop.Run();
      }
    }
  }

  return !canceled_;
}

void InteractionTestUtilMouse::CancelAllGestures() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  canceled_ = true;

  // Now that no additional actions will happen, release all mouse buttons.
  for (ui_controls::MouseButton button : buttons_down_) {
    if (!ui_controls::SendMouseEvents(button, ui_controls::UP))
      LOG(WARNING) << "Unable to release mouse button " << button;
  }
  buttons_down_.clear();

  // Maybe handle dragging stopped.
  MaybeCancelDrag(true);
}

InteractionTestUtilMouse::InteractionTestUtilMouse(gfx::NativeWindow window)
#if defined(USE_AURA)
    : native_window_(std::make_unique<NativeWindowRef>(window))
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
  for (auto& gesture : to_add)
    gestures.emplace_back(std::move(gesture));
}

}  // namespace views::test
