// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/accelerator_filter.h"

#include <utility>

#include "build/build_config.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/wm/core/accelerator_delegate.h"

namespace wm {

////////////////////////////////////////////////////////////////////////////////
// AcceleratorFilter, public:

AcceleratorFilter::AcceleratorFilter(
    std::unique_ptr<AcceleratorDelegate> delegate)
    : delegate_(std::move(delegate)) {}

AcceleratorFilter::~AcceleratorFilter() {
}

bool AcceleratorFilter::ShouldFilter(ui::KeyEvent* event) {
  const ui::EventType type = event->type();
  if (!event->target() ||
      (type != ui::EventType::kKeyPressed &&
       type != ui::EventType::kKeyReleased) ||
      event->is_char() || !event->target() ||
      // Key events with key code of VKEY_PROCESSKEY, usually created by virtual
      // keyboard (like handwriting input), have no effect on accelerator and
      // they may disturb the accelerator history. So filter them out. (see
      // https://crbug.com/918317)
      event->key_code() == ui::VKEY_PROCESSKEY) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorFilter, EventFilter implementation:

void AcceleratorFilter::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(event->target());
  if (ShouldFilter(event))
    return;

  ui::Accelerator accelerator(*event);
  if (delegate_->ProcessAccelerator(*event, accelerator))
    event->StopPropagation();
}

}  // namespace wm
