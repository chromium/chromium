// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/tablet_state.h"

namespace display {

bool IsTabletStateChanging(TabletState state) {
  return state == TabletState::kEnteringTabletMode ||
         state == TabletState::kExitingTabletMode;
}

}  // namespace display
