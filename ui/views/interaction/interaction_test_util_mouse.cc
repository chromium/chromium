// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interaction_test_util_mouse.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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
  CHECK(!pending_callback_ && pending_gestures_.empty())
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

void InteractionTestUtilMouse::PerformGesturesImpl(
    MouseGestures gestures,
    GestureCallback result_callback) {
  CHECK(pending_gestures_.empty());
  CHECK(!pending_callback_);
  CHECK(!gestures.empty());
  CHECK(result_callback);

  pending_gestures_ = std::move(gestures);
  pending_callback_ = std::move(result_callback);

  QueueNextGesture();
}

void InteractionTestUtilMouse::CancelAllGestures() {
  // Clear and cancel all pending actions.
  weak_ptr_factory_.InvalidateWeakPtrs();
  pending_gestures_.clear();

  // Now that no additional actions will happen, release all mouse buttons.
  for (ui_controls::MouseButton button : buttons_down_) {
    if (!ui_controls::SendMouseEvents(button, ui_controls::UP))
      LOG(WARNING) << "Unable to release mouse button " << button;
  }
  buttons_down_.clear();

  // Maybe handle dragging stopped.
#if defined(USE_AURA)
  if (dragged_) {
    if (auto* const window = static_cast<aura::Window*>(*native_window_))
      drag_ender_ = std::make_unique<DragEnder>(window);
  }
#endif
  dragged_ = false;
  dragging_ = false;

  // Call the gesture failed callback if one is present. This needs to be the
  // last thing here because theoretically it could cause the |this| pointer to
  // be deleted.
  if (pending_callback_)
    std::move(pending_callback_).Run(false);
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

void InteractionTestUtilMouse::QueueNextGesture() {
  base::OnceClosure to_post;
  // We are often in the middle of an event callback. Therefore, don't post the
  // completed event quite yet - put it at the end of the current event queue
  // instead.
  if (pending_gestures_.empty()) {
    to_post = base::BindOnce(&InteractionTestUtilMouse::OnSequenceComplete,
                             weak_ptr_factory_.GetWeakPtr());
  } else {
    to_post = base::BindOnce(&InteractionTestUtilMouse::PerformNextGesture,
                             weak_ptr_factory_.GetWeakPtr());
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(to_post));
}

void InteractionTestUtilMouse::PerformNextGesture() {
  const MouseGesture next = std::move(pending_gestures_.front());
  pending_gestures_.pop_front();

  auto followup = base::BindOnce(&InteractionTestUtilMouse::QueueNextGesture,
                                 weak_ptr_factory_.GetWeakPtr());

  bool result;
  if (absl::holds_alternative<MouseButtonGesture>(next)) {
    const auto& gesture = absl::get<MouseButtonGesture>(next);
    OnMouseButton(gesture);
    result = ui_controls::SendMouseEventsNotifyWhenDone(
        gesture.first, gesture.second, std::move(followup));
  } else {
    const auto& gesture = absl::get<MouseMoveGesture>(next);
    OnMouseMove();
    result = ui_controls::SendMouseMoveNotifyWhenDone(gesture.x(), gesture.y(),
                                                      std::move(followup));
  }

  if (!result && pending_callback_)
    std::move(pending_callback_).Run(false);
}

void InteractionTestUtilMouse::OnMouseButton(MouseButtonGesture gesture) {
#if defined(USE_AURA)
  drag_ender_.reset();
#endif
  switch (gesture.second) {
    case ui_controls::DOWN:
#if defined(USE_AURA)
      if (aura::Window* const window =
              static_cast<aura::Window*>(*native_window_)) {
        DragEnder::EndDrag(window);
      }
#endif
      CHECK(buttons_down_.insert(gesture.first).second);
      CHECK(!dragging_);
      dragging_ = false;
      dragged_ = false;
      break;
    case ui_controls::UP:
      CHECK(buttons_down_.erase(gesture.first));
      if (dragging_)
        OnDragEnd();
      break;
  }
}

void InteractionTestUtilMouse::OnMouseMove() {
#if defined(USE_AURA)
  drag_ender_.reset();
#endif
  switch (buttons_down_.size()) {
    case 0U:
      return;
    case 1U:
      CHECK_EQ(ui_controls::LEFT, *buttons_down_.begin());
      if (!dragging_)
        OnDragStart();
      break;
    default:
      NOTREACHED() << "Cannot drag with multiple buttons down.";
      break;
  }
}

void InteractionTestUtilMouse::OnDragStart() {
  dragging_ = true;
}

void InteractionTestUtilMouse::OnDragEnd() {
  dragged_ |= dragging_;
  dragging_ = false;
}

void InteractionTestUtilMouse::OnSequenceComplete() {
  if (pending_callback_)
    std::move(pending_callback_).Run(true);
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
