// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_NATIVE_DISPLAY_OBSERVER_H_
#define UI_DISPLAY_TYPES_NATIVE_DISPLAY_OBSERVER_H_

#include "ui/display/types/display_types_export.h"

namespace display {

// Observer class used by NativeDisplayDelegate to announce when the display
// configuration changes.
class DISPLAY_TYPES_EXPORT NativeDisplayObserver {
 public:
  virtual ~NativeDisplayObserver() {}

  virtual void OnConfigurationChanged() = 0;

  // DisplaySnapshots owned by NativeDisplayDelegate are about to be
  // invalidated and any stored pointers to them should be deleted.
  virtual void OnDisplaySnapshotsInvalidated() = 0;
};

}  // namespace display

#endif  // UI_DISPLAY_TYPES_NATIVE_DISPLAY_OBSERVER_H_
