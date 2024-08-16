// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/window_modality_controller.h"

#include <stddef.h>

#include <queue>
#include <string_view>

#include "base/ranges/algorithm.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/class_property.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace wm {
namespace {

bool HasAncestor(const aura::Window* window, const aura::Window* ancestor) {
  return ancestor && ancestor->Contains(window);
}

bool TransientChildIsWindowModal(const aura::Window* window) {
  return window->GetProperty(aura::client::kModalKey) ==
         ui::mojom::ModalType::kWindow;
}

bool TransientChildIsSystemModal(const aura::Window* window) {
  return window->GetProperty(aura::client::kModalKey) ==
         ui::mojom::ModalType::kSystem;
}

bool TransientChildIsChildModal(const aura::Window* window) {
  return window->GetProperty(aura::client::kModalKey) ==
         ui::mojom::ModalType::kChild;
}

aura::Window* GetModalParent(const aura::Window* window) {
  return window->GetProperty(aura::client::kChildModalParentKey);
}

bool IsModalTransientChild(const aura::Window* transient,
                           const aura::Window* original) {
  return transient->IsVisible() &&
         (TransientChildIsWindowModal(transient) ||
          TransientChildIsSystemModal(transient) ||
          (TransientChildIsChildModal(transient) &&
           HasAncestor(original, GetModalParent(transient))));
}

const aura::Window* GetModalTransientChild(const aura::Window* activatable,
                                           const aura::Window* original) {
  for (const aura::Window* transient : GetTransientChildren(activatable)) {
    if (IsModalTransientChild(transient, original)) {
      if (GetTransientChildren(transient).empty())
        return transient;

      const aura::Window* modal_child =
          GetModalTransientChild(transient, original);
      return modal_child ? modal_child : transient;
    }
  }
  return nullptr;
}

}  // namespace

void SetModalParent(aura::Window* child, aura::Window* parent) {
  child->SetProperty(aura::client::kChildModalParentKey, parent);
}

aura::Window* GetModalTransient(aura::Window* window) {
  return const_cast<aura::Window*>(
      GetModalTransient(const_cast<const aura::Window*>(window)));
}

const aura::Window* GetModalTransient(const aura::Window* window) {
  if (!window)
    return nullptr;

  // We always want to check for the transient child of the toplevel window.
  const aura::Window* toplevel = GetToplevelWindow(window);
  if (!toplevel)
    return nullptr;

  return GetModalTransientChild(toplevel, window);
}

////////////////////////////////////////////////////////////////////////////////
// WindowModalityController, public:

WindowModalityController::WindowModalityController(
    ui::EventTarget* event_target,
    aura::Env* env)
    : env_(env ? env : aura::Env::GetInstance()), event_target_(event_target) {
  env_->AddObserver(this);
  DCHECK(event_target->IsPreTargetListEmpty());
  event_target_->AddPreTargetHandler(this);
}

WindowModalityController::~WindowModalityController() {
  event_target_->RemovePreTargetHandler(this);
  env_->RemoveObserver(this);
  for (size_t i = 0; i < windows_.size(); ++i)
    windows_[i]->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// WindowModalityController, aura::EventFilter implementation:

void WindowModalityController::OnKeyEvent(ui::KeyEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (GetModalTransient(target))
    event->SetHandled();
}

void WindowModalityController::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (ProcessLocatedEvent(target, event))
    event->SetHandled();
}

void WindowModalityController::OnTouchEvent(ui::TouchEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (ProcessLocatedEvent(target, event))
    event->SetHandled();
}

std::string_view WindowModalityController::GetLogContext() const {
  return "WindowModalityController";
}

////////////////////////////////////////////////////////////////////////////////
// WindowModalityController, aura::EnvObserver implementation:

void WindowModalityController::OnWindowInitialized(aura::Window* window) {
  windows_.push_back(window);
  window->AddObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// WindowModalityController, aura::WindowObserver implementation:

void WindowModalityController::OnWindowPropertyChanged(aura::Window* window,
                                                       const void* key,
                                                       intptr_t old) {
  // In tests, we sometimes create the modality relationship after a window is
  // visible.
  if (key == aura::client::kModalKey &&
      window->GetProperty(aura::client::kModalKey) !=
          ui::mojom::ModalType::kNone &&
      window->IsVisible()) {
    ActivateWindow(window);
    CancelTouchesOnTransientWindowTree(window);
  }
}

void WindowModalityController::OnWindowVisibilityChanged(aura::Window* window,
                                                         bool visible) {
  if (visible && window->GetProperty(aura::client::kModalKey) !=
                     ui::mojom::ModalType::kNone) {
    CancelTouchesOnTransientWindowTree(window);

    // Make sure no other window has capture, otherwise |window| won't get mouse
    // events.
    aura::Window* capture_window = aura::client::GetCaptureWindow(window);
    if (capture_window) {
      bool should_release_capture = true;
      if (window->GetProperty(aura::client::kModalKey) ==
              ui::mojom::ModalType::kChild &&
          !HasAncestor(capture_window, GetModalParent(window))) {
        // For child modal windows we only need ensure capture is not on a
        // descendant of the modal parent. This way we block events to the
        // parents subtree appropriately.
        should_release_capture = false;
      }

      if (should_release_capture)
        capture_window->ReleaseCapture();
    }
  }
}

void WindowModalityController::OnWindowDestroyed(aura::Window* window) {
  windows_.erase(base::ranges::find(windows_, window));
  window->RemoveObserver(this);
}

bool WindowModalityController::ProcessLocatedEvent(aura::Window* target,
                                                   ui::LocatedEvent* event) {
  if (event->handled())
    return false;
  aura::Window* modal_transient_child = GetModalTransient(target);
  if (modal_transient_child &&
      (event->type() == ui::EventType::kMousePressed ||
       event->type() == ui::EventType::kTouchPressed)) {
    // Activate top window if transient child window is window modal.
    if (TransientChildIsWindowModal(modal_transient_child)) {
      aura::Window* toplevel = GetToplevelWindow(target);
      DCHECK(toplevel);
      ActivateWindow(toplevel);
    }

    AnimateWindow(modal_transient_child, WINDOW_ANIMATION_TYPE_BOUNCE);
  }
  if (event->type() == ui::EventType::kTouchCancelled) {
    return false;
  }
  return !!modal_transient_child;
}

void WindowModalityController::CancelTouchesOnTransientWindowTree(
    aura::Window* window) {
  // Find the top level transient window.
  aura::Window* top_level_window = window;
  while (wm::GetTransientParent(top_level_window))
    top_level_window = wm::GetTransientParent(top_level_window);

  // BFS to get all transient windows in the tree rooted to the top level
  // transient window.
  std::vector<ui::GestureConsumer*> blocked_consumers;
  std::queue<aura::Window*> que;
  que.emplace(top_level_window);
  while (!que.empty()) {
    aura::Window* parent = que.front();
    que.pop();
    blocked_consumers.emplace_back(parent);
    for (aura::Window* w : wm::GetTransientChildren(parent)) {
      que.emplace(w);
    }
  }

  // Cancel touches on all the transient windows.
  env_->gesture_recognizer()->CancelActiveTouchesOn(blocked_consumers);
}

}  // namespace wm
