// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/focus_manager_event_handler.h"

#include <string_view>

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace views {

FocusManagerEventHandler::FocusManagerEventHandler(Widget* widget,
                                                   aura::Window* window)
    : widget_(widget->GetWeakPtr()) {
  CHECK(window, base::NotFatalUntil::M163);
  window_observation_.Observe(window);
}

FocusManagerEventHandler::~FocusManagerEventHandler() = default;

void FocusManagerEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  aura::Window* window = widget_ ? widget_->GetNativeWindow() : nullptr;
  aura::WindowTreeHost* host = window ? window->GetHost() : nullptr;
  if (host && event->code() != ui::DomCode::ESCAPE &&
      host->IsKeyLocked(event->code())) {
    return;
  }
  if (widget_ && widget_->GetFocusManager() &&
      widget_->GetFocusManager()->GetFocusedView() &&
      !widget_->GetFocusManager()->OnKeyEvent(*event)) {
    event->StopPropagation();
  }
}

std::string_view FocusManagerEventHandler::GetLogContext() const {
  return "FocusManagerEventHandler";
}

}  // namespace views
