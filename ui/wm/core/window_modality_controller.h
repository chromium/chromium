// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_WINDOW_MODALITY_CONTROLLER_H_
#define UI_WM_CORE_WINDOW_MODALITY_CONTROLLER_H_

#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"

namespace aura {
class Env;
}

namespace ui {
class EventTarget;
class LocatedEvent;
}  // namespace ui

namespace wm {

// Sets the modal parent for the child.
COMPONENT_EXPORT(UI_WM)
void SetModalParent(aura::Window* child, aura::Window* parent);

// Returns the modal transient child of |window|, or NULL if |window| does not
// have any modal transient children.
COMPONENT_EXPORT(UI_WM) aura::Window* GetModalTransient(aura::Window* window);
COMPONENT_EXPORT(UI_WM)
const aura::Window* GetModalTransient(const aura::Window* window);

// WindowModalityController is an event filter that consumes events sent to
// windows that are the transient parents of window-modal windows. This filter
// must be added to the CompoundEventFilter so that activation works properly.
class COMPONENT_EXPORT(UI_WM) WindowModalityController
    : public ui::EventHandler,
      public aura::EnvObserver,
      public aura::WindowObserver {
 public:
  explicit WindowModalityController(ui::EventTarget* event_target,
                                    aura::Env* env = nullptr);

  WindowModalityController(const WindowModalityController&) = delete;
  WindowModalityController& operator=(const WindowModalityController&) = delete;

  ~WindowModalityController() override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  std::string_view GetLogContext() const override;

  // Overridden from aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // Overridden from aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroyed(aura::Window* window) override;

 private:
  // Processes a mouse/touch event, and returns true if the event should be
  // consumed.
  bool ProcessLocatedEvent(aura::Window* target, ui::LocatedEvent* event);

  // Cancel touches on the transient window tree rooted to the top level
  // transient window of the |window|.
  void CancelTouchesOnTransientWindowTree(aura::Window* window);

  raw_ptr<aura::Env> env_;

  std::vector<raw_ptr<aura::Window, VectorExperimental>> windows_;

  raw_ptr<ui::EventTarget> event_target_;
};

}  // namespace wm

#endif  // UI_WM_CORE_WINDOW_MODALITY_CONTROLLER_H_
