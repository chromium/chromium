// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/x11_window_event_manager.h"

#include <stddef.h>

#include "base/memory/singleton.h"
#include "ui/gfx/x/x11.h"

namespace ui {

namespace {

// Asks the X server to set |window|'s event mask to |new_mask|.
void SetEventMask(x11::Window window, x11::EventMask new_mask) {
  auto* connection = x11::Connection::Get();
  // Window |window| may already be destroyed at this point, so the
  // change_attributes request may give a BadWindow error.  In this case, just
  // ignore the error.
  connection
      ->ChangeWindowAttributes(
          {.window = window,
           .event_mask = static_cast<x11::EventMask>(new_mask)})
      .IgnoreError();
}

}  // anonymous namespace

XScopedEventSelector::XScopedEventSelector(x11::Window window,
                                           x11::EventMask event_mask)
    : window_(window),
      event_mask_(event_mask),
      event_manager_(
          XWindowEventManager::GetInstance()->weak_ptr_factory_.GetWeakPtr()) {
  event_manager_->SelectEvents(window_, event_mask_);
}

XScopedEventSelector::~XScopedEventSelector() {
  if (event_manager_)
    event_manager_->DeselectEvents(window_, event_mask_);
}

// static
XWindowEventManager* XWindowEventManager::GetInstance() {
  return base::Singleton<XWindowEventManager>::get();
}

class XWindowEventManager::MultiMask {
 public:
  MultiMask() { memset(mask_bits_, 0, sizeof(mask_bits_)); }

  ~MultiMask() = default;

  void AddMask(x11::EventMask mask) {
    for (int i = 0; i < kMaskSize; i++) {
      if (static_cast<uint32_t>(mask) & (1 << i))
        mask_bits_[i]++;
    }
  }

  void RemoveMask(x11::EventMask mask) {
    for (int i = 0; i < kMaskSize; i++) {
      if (static_cast<uint32_t>(mask) & (1 << i)) {
        DCHECK(mask_bits_[i]);
        mask_bits_[i]--;
      }
    }
  }

  x11::EventMask ToMask() const {
    x11::EventMask mask = x11::EventMask::NoEvent;
    for (int i = 0; i < kMaskSize; i++) {
      if (mask_bits_[i])
        mask = mask | static_cast<x11::EventMask>(1 << i);
    }
    return mask;
  }

 private:
  static constexpr auto kMaskSize = 25;

  int mask_bits_[kMaskSize];

  DISALLOW_COPY_AND_ASSIGN(MultiMask);
};

XWindowEventManager::XWindowEventManager() = default;

XWindowEventManager::~XWindowEventManager() {
  // Clear events still requested by not-yet-deleted XScopedEventSelectors.
  for (const auto& mask_pair : mask_map_)
    SetEventMask(mask_pair.first, x11::EventMask::NoEvent);
}

void XWindowEventManager::SelectEvents(x11::Window window,
                                       x11::EventMask event_mask) {
  std::unique_ptr<MultiMask>& mask = mask_map_[window];
  if (!mask)
    mask = std::make_unique<MultiMask>();
  x11::EventMask old_mask = mask_map_[window]->ToMask();
  mask->AddMask(event_mask);
  AfterMaskChanged(window, old_mask);
}

void XWindowEventManager::DeselectEvents(x11::Window window,
                                         x11::EventMask event_mask) {
  DCHECK(mask_map_.find(window) != mask_map_.end());
  std::unique_ptr<MultiMask>& mask = mask_map_[window];
  x11::EventMask old_mask = mask->ToMask();
  mask->RemoveMask(event_mask);
  AfterMaskChanged(window, old_mask);
}

void XWindowEventManager::AfterMaskChanged(x11::Window window,
                                           x11::EventMask old_mask) {
  x11::EventMask new_mask = mask_map_[window]->ToMask();
  if (new_mask == old_mask)
    return;

  SetEventMask(window, new_mask);

  if (new_mask == x11::EventMask::NoEvent)
    mask_map_.erase(window);
}

}  // namespace ui
