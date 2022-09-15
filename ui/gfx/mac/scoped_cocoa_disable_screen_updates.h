// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MAC_SCOPED_COCOA_DISABLE_SCREEN_UPDATES_H_
#define UI_GFX_MAC_SCOPED_COCOA_DISABLE_SCREEN_UPDATES_H_

#include "ui/gfx/gfx_export.h"

namespace gfx {

// A stack-based class to disable Cocoa screen updates. When instantiated, it
// disables screen updates and enables them when destroyed. Update disabling
// can be nested, and there is a time-maximum (about 1 second) after which
// Cocoa will automatically re-enable updating. This class doesn't attempt to
// overrule that.
class GFX_EXPORT ScopedCocoaDisableScreenUpdates {
 public:
  ScopedCocoaDisableScreenUpdates();

  ScopedCocoaDisableScreenUpdates(const ScopedCocoaDisableScreenUpdates&) =
      delete;
  ScopedCocoaDisableScreenUpdates& operator=(
      const ScopedCocoaDisableScreenUpdates&) = delete;

  ~ScopedCocoaDisableScreenUpdates();
};

}  // namespace gfx

#endif  // UI_GFX_MAC_SCOPED_COCOA_DISABLE_SCREEN_UPDATES_H_
