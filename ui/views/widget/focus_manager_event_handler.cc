// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/focus_manager_event_handler.h"

#include <string_view>

#include "ui/aura/window.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace views {

FocusManagerEventHandler::FocusManagerEventHandler(Widget* widget,
                                                   aura::Window* window)
    : widget_(widget->GetWeakPtr()), window_(window) {
  DCHECK(window_);
  window_->AddPreTargetHandler(this);
}

FocusManagerEventHandler::~FocusManagerEventHandler() {
  window_->RemovePreTargetHandler(this);
}

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
