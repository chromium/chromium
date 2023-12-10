// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/window_event_manager.h"

#include <stddef.h>

#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"

namespace x11 {

namespace {

// Asks the X server to set |window|'s event mask to |new_mask|.
void SetEventMask(Connection* connection, Window window, EventMask new_mask) {
  // Window |window| may already be destroyed at this point, so the
  // change_attributes request may give a BadWindow error.  In this case, just
  // ignore the error.
  connection
      ->ChangeWindowAttributes(ChangeWindowAttributesRequest{
          .window = window, .event_mask = static_cast<EventMask>(new_mask)})
      .IgnoreError();
}

}  // anonymous namespace

ScopedEventSelector::ScopedEventSelector() = default;

ScopedEventSelector::ScopedEventSelector(Connection* connection,
                                         Window window,
                                         EventMask event_mask)
    : window_(window),
      event_mask_(event_mask),
      event_manager_(&connection->window_event_manager()) {
  event_manager_->SelectEvents(window_, event_mask_);
}

ScopedEventSelector::ScopedEventSelector(ScopedEventSelector&& other) {
  Swap(other);
}

ScopedEventSelector& ScopedEventSelector::operator=(
    ScopedEventSelector&& other) {
  Reset();
  Swap(other);
  return *this;
}

ScopedEventSelector::~ScopedEventSelector() {
  Reset();
}

void ScopedEventSelector::Swap(ScopedEventSelector& other) {
  std::swap(window_, other.window_);
  std::swap(event_mask_, other.event_mask_);
  std::swap(event_manager_, other.event_manager_);
}

void ScopedEventSelector::Reset() {
  if (event_manager_) {
    event_manager_->DeselectEvents(window_, event_mask_);
  }
  window_ = Window::None;
  event_mask_ = EventMask::NoEvent;
  event_manager_ = nullptr;
}

class WindowEventManager::MultiMask {
 public:
  MultiMask() { memset(mask_bits_, 0, sizeof(mask_bits_)); }

  MultiMask(const MultiMask&) = delete;
  MultiMask& operator=(const MultiMask&) = delete;

  ~MultiMask() = default;

  void AddMask(EventMask mask) {
    for (int i = 0; i < kMaskSize; i++) {
      if (static_cast<uint32_t>(mask) & (1 << i)) {
        mask_bits_[i]++;
      }
    }
  }

  void RemoveMask(EventMask mask) {
    for (int i = 0; i < kMaskSize; i++) {
      if (static_cast<uint32_t>(mask) & (1 << i)) {
        DUMP_WILL_BE_CHECK(mask_bits_[i]);
        mask_bits_[i]--;
      }
    }
  }

  EventMask ToMask() const {
    EventMask mask = EventMask::NoEvent;
    for (int i = 0; i < kMaskSize; i++) {
      if (mask_bits_[i]) {
        mask = mask | static_cast<EventMask>(1 << i);
      }
    }
    return mask;
  }

 private:
  static constexpr auto kMaskSize = 25;

  int mask_bits_[kMaskSize];
};

WindowEventManager::WindowEventManager(Connection* connection)
    : connection_(connection) {}

WindowEventManager::~WindowEventManager() {
  // Clear events still requested by not-yet-deleted ScopedEventSelectors.
  for (const auto& mask_pair : mask_map_) {
    SetEventMask(connection_, mask_pair.first, EventMask::NoEvent);
  }
}

void WindowEventManager::SelectEvents(Window window, EventMask event_mask) {
  std::unique_ptr<MultiMask>& mask = mask_map_[window];
  if (!mask) {
    mask = std::make_unique<MultiMask>();
  }
  EventMask old_mask = mask_map_[window]->ToMask();
  mask->AddMask(event_mask);
  AfterMaskChanged(window, old_mask);
}

void WindowEventManager::DeselectEvents(Window window, EventMask event_mask) {
  DUMP_WILL_BE_CHECK(mask_map_.find(window) != mask_map_.end());
  std::unique_ptr<MultiMask>& mask = mask_map_[window];
  EventMask old_mask = mask->ToMask();
  mask->RemoveMask(event_mask);
  AfterMaskChanged(window, old_mask);
}

void WindowEventManager::AfterMaskChanged(Window window, EventMask old_mask) {
  EventMask new_mask = mask_map_[window]->ToMask();
  if (new_mask == old_mask) {
    return;
  }

  SetEventMask(connection_, window, new_mask);

  if (new_mask == EventMask::NoEvent) {
    mask_map_.erase(window);
  }
}

}  // namespace x11
