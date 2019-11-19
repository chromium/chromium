// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_X_X11_WINDOW_EVENT_MANAGER_H_
#define UI_EVENTS_X_X11_WINDOW_EVENT_MANAGER_H_

#include <map>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/x/events_x_export.h"
#include "ui/gfx/x/x11_types.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ui {

class XWindowEventManager;

// Ensures events in |event_mask| are selected on |xid| for the duration of this
// object's lifetime.
class EVENTS_X_EXPORT XScopedEventSelector {
 public:
  XScopedEventSelector(XID xid, uint32_t event_mask);
  ~XScopedEventSelector();

 private:
  XID xid_;
  uint32_t event_mask_;
  base::WeakPtr<XWindowEventManager> event_manager_;

  DISALLOW_COPY_AND_ASSIGN(XScopedEventSelector);
};

// Allows multiple clients within Chrome to select events on the same X window.
class XWindowEventManager {
 public:
  static XWindowEventManager* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<XWindowEventManager>;
  friend class XScopedEventSelector;

  class MultiMask;

  XWindowEventManager();
  ~XWindowEventManager();

  // Guarantees that events in |event_mask| will be reported to Chrome.
  void SelectEvents(XID xid, uint32_t event_mask);

  // Deselects events on |event_mask|.  Chrome will stop receiving events for
  // any set bit in |event_mask| only if no other client has selected that bit.
  void DeselectEvents(XID xid, uint32_t event_mask);

  // Helper method called by SelectEvents and DeselectEvents whenever the mask
  // corresponding to window |xid| might have changed.  Calls SetEventMask if
  // necessary.
  void AfterMaskChanged(XID xid, uint32_t old_mask);

  std::map<XID, std::unique_ptr<MultiMask>> mask_map_;

  // This is used to set XScopedEventSelector::event_manager_.  If |this| is
  // destroyed before any XScopedEventSelector, the |event_manager_| will become
  // invalidated.
  base::WeakPtrFactory<XWindowEventManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(XWindowEventManager);
};

}  // namespace ui

#endif  // UI_EVENTS_X_X11_WINDOW_EVENT_MANAGER_H_
