// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_NS_VIEW_IDS_H_
#define UI_BASE_COCOA_NS_VIEW_IDS_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/base/ui_base_export.h"

@class NSView;

namespace ui {

// A class used to manage NSViews across processes.
// - NSViews that may be instantiated in another process are assigned an id in
//   the browser process.
// - Instantiating a ScopedNSViewIdMapping in the process where the NSView is
//   created will make NSViewIds::GetNSView return the specified NSView.
// - This mechanism is used by mojo methods to refer to NSViews across
//   interfaces (in particular, to specify parent NSViews).
class UI_BASE_EXPORT NSViewIds {
 public:
  // Get a new id to use with a new NSView. This is to only be called in the
  // browser process.
  static uint64_t GetNewId();

  // Return an NSView given its id. This is to be called in an app shim process.
  static NSView* GetNSView(uint64_t ns_view_id);
};

// A scoped mapping from |ns_view_id| to |view|. While this object exists,
// NSViewIds::GetNSView will return |view| when queried with |ns_view_id|. This
// is to be instantiated in the app shim process.
class UI_BASE_EXPORT ScopedNSViewIdMapping {
 public:
  ScopedNSViewIdMapping(uint64_t ns_view_id, NSView* view);
  ~ScopedNSViewIdMapping();

 private:
  const uint64_t ns_view_id_;
  DISALLOW_COPY_AND_ASSIGN(ScopedNSViewIdMapping);
};

}  // namespace ui

#endif  // UI_BASE_COCOA_NS_VIEW_IDS_H_
