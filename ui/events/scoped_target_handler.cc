// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/scoped_target_handler.h"

#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_target.h"

namespace ui {

ScopedTargetHandler::ScopedTargetHandler(EventTarget* target,
                                         EventHandler* handler)
    : destroyed_flag_(nullptr), target_(target), new_handler_(handler) {
  original_handler_ = target_->SetTargetHandler(this);
}

ScopedTargetHandler::~ScopedTargetHandler() {
  EventHandler* handler = target_->SetTargetHandler(original_handler_);
  DCHECK_EQ(this, handler);
  if (destroyed_flag_)
    *destroyed_flag_ = true;
}

void ScopedTargetHandler::OnEvent(Event* event) {
  if (original_handler_) {
    bool destroyed = false;
    bool* old_destroyed_flag = destroyed_flag_;
    destroyed_flag_ = &destroyed;

    original_handler_->OnEvent(event);

    if (destroyed) {
      if (old_destroyed_flag)
        *old_destroyed_flag = true;
      return;
    }
    destroyed_flag_ = old_destroyed_flag;
  }

  // This check is needed due to nested event loops when starting DragDrop.
  if (event->stopped_propagation())
    return;

  new_handler_->OnEvent(event);
}

base::StringPiece ScopedTargetHandler::GetLogContext() const {
  return "ScopedTargetHandler";
}

}  // namespace ui
