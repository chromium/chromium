// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/scoped_target_handler.h"

#include <string_view>

#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_target.h"

namespace ui {

ScopedTargetHandler::ScopedTargetHandler(EventTarget* target,
                                         EventHandler* handler)
    : target_(target), new_handler_(handler) {
  original_handler_ = target_->SetTargetHandler(this);
}

ScopedTargetHandler::~ScopedTargetHandler() {
  EventHandler* handler = target_->SetTargetHandler(original_handler_);
  DCHECK_EQ(this, handler);
}

void ScopedTargetHandler::OnEvent(Event* event) {
  if (original_handler_) {
    auto weak_this = weak_factory_.GetWeakPtr();

    original_handler_->OnEvent(event);

    if (!weak_this)
      return;
  }

  // This check is needed due to nested event loops when starting DragDrop.
  if (event->stopped_propagation())
    return;

  new_handler_->OnEvent(event);
}

std::string_view ScopedTargetHandler::GetLogContext() const {
  return "ScopedTargetHandler";
}

}  // namespace ui
