// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/x11_window_event_manager.h"

#include <stddef.h>
#include <xcb/xcb.h>

#include "base/memory/singleton.h"
#include "ui/gfx/x/x11.h"

namespace ui {

namespace {

// Asks the X server to set |xid|'s event mask to |new_mask|.
void SetEventMask(XID xid, uint32_t new_mask) {
  XDisplay* display = gfx::GetXDisplay();
  xcb_connection_t* connection = XGetXCBConnection(display);
  auto cookie = xcb_change_window_attributes(connection, xid, XCB_CW_EVENT_MASK,
                                             &new_mask);
  // Window |xid| may already be destroyed at this point, so the
  // change_attributes request may give a BadWindow error.  In this case, just
  // ignore the error.
  xcb_discard_reply(connection, cookie.sequence);
}

}  // anonymous namespace

XScopedEventSelector::XScopedEventSelector(XID xid, uint32_t event_mask)
    : xid_(xid),
      event_mask_(event_mask),
      event_manager_(
          XWindowEventManager::GetInstance()->weak_ptr_factory_.GetWeakPtr()) {
  event_manager_->SelectEvents(xid_, event_mask_);
}

XScopedEventSelector::~XScopedEventSelector() {
  if (event_manager_)
    event_manager_->DeselectEvents(xid_, event_mask_);
}

// static
XWindowEventManager* XWindowEventManager::GetInstance() {
  return base::Singleton<XWindowEventManager>::get();
}

class XWindowEventManager::MultiMask {
 public:
  MultiMask() {
    for (int i = 0; i < kMaskSize; i++)
      mask_bits_[i] = 0;
  }

  ~MultiMask() {}

  void AddMask(uint32_t mask) {
    for (int i = 0; i < kMaskSize; i++) {
      if (mask & (1 << i))
        mask_bits_[i]++;
    }
  }

  void RemoveMask(uint32_t mask) {
    for (int i = 0; i < kMaskSize; i++) {
      if (mask & (1 << i)) {
        DCHECK(mask_bits_[i]);
        mask_bits_[i]--;
      }
    }
  }

  uint32_t ToMask() const {
    uint32_t mask = NoEventMask;
    for (int i = 0; i < kMaskSize; i++) {
      if (mask_bits_[i])
        mask |= (1 << i);
    }
    return mask;
  }

 private:
  static constexpr auto kMaskSize = 25;

  int mask_bits_[kMaskSize];

  DISALLOW_COPY_AND_ASSIGN(MultiMask);
};

XWindowEventManager::XWindowEventManager() {}

XWindowEventManager::~XWindowEventManager() {
  // Clear events still requested by not-yet-deleted XScopedEventSelectors.
  for (const auto& mask_pair : mask_map_)
    SetEventMask(mask_pair.first, NoEventMask);
}

void XWindowEventManager::SelectEvents(XID xid, uint32_t event_mask) {
  std::unique_ptr<MultiMask>& mask = mask_map_[xid];
  if (!mask)
    mask = std::make_unique<MultiMask>();
  uint32_t old_mask = mask_map_[xid]->ToMask();
  mask->AddMask(event_mask);
  AfterMaskChanged(xid, old_mask);
}

void XWindowEventManager::DeselectEvents(XID xid, uint32_t event_mask) {
  DCHECK(mask_map_.find(xid) != mask_map_.end());
  std::unique_ptr<MultiMask>& mask = mask_map_[xid];
  uint32_t old_mask = mask->ToMask();
  mask->RemoveMask(event_mask);
  AfterMaskChanged(xid, old_mask);
}

void XWindowEventManager::AfterMaskChanged(XID xid, uint32_t old_mask) {
  uint32_t new_mask = mask_map_[xid]->ToMask();
  if (new_mask == old_mask)
    return;

  SetEventMask(xid, new_mask);

  if (new_mask == NoEventMask)
    mask_map_.erase(xid);
}

}  // namespace ui
