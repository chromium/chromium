// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TABLET_STATE_H_
#define UI_DISPLAY_TABLET_STATE_H_

#include "ui/display/display_export.h"

namespace display {

// Tracks whether we are in the process of entering or exiting tablet mode.
//
// Here's the description of how TabletState transition goes:
//
// TabletState is changed to `kEnteringTabletMode` before starting the process
// to enter tablet mode and then changed to `kInTabletMode` when the transition
// process has completed.
//
// TabletState is changed to `kExitingabletMode` before stating the process to
// exit tablet mode and then changed to `kInClamshellMode` when the transition
// process has completed.
enum class TabletState {
  kInClamshellMode,
  kEnteringTabletMode,
  kInTabletMode,
  kExitingTabletMode,
};

// Returns true if the tablet state is in the process of transition.
DISPLAY_EXPORT bool IsTabletStateChanging(TabletState state);

}  // namespace display

#endif  // UI_DISPLAY_TABLET_STATE_H_
