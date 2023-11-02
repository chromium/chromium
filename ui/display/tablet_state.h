// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TABLET_STATE_H_
#define UI_DISPLAY_TABLET_STATE_H_

namespace display {

// Tracks whether we are in the process of entering or exiting tablet mode.
enum class TabletState {
  kInClamshellMode,
  kEnteringTabletMode,
  kInTabletMode,
  kExitingTabletMode,
};

}  // namespace display

#endif  // UI_DISPLAY_TABLET_STATE_H_
