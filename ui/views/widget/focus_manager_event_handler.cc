// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/focus_manager_event_handler.h"

#include <string_view>

#include "ui/aura/window.h"
#include "ui/events/event_target.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace views {

FocusManagerEventHandler::FocusManagerEventHandler(Widget* widget,
                                                   aura::Window* window)
    : widget_(widget->GetWeakPtr()) {
  DCHECK(window);
  window_observation_.Observe(window);
}

FocusManagerEventHandler::~FocusManagerEventHandler() = default;

void FocusManagerEventHandler::OnKeyEvent(ui::KeyEvent* event) {
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
