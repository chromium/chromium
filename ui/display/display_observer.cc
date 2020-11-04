// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_observer.h"

#include "ui/display/tablet_state.h"

namespace display {

DisplayObserver::~DisplayObserver() {}

void DisplayObserver::OnWillProcessDisplayChanges() {}

void DisplayObserver::OnDidProcessDisplayChanges() {}

void DisplayObserver::OnDisplayAdded(const Display& new_display) {}

void DisplayObserver::OnDisplayRemoved(const Display& old_display) {}

void DisplayObserver::OnDisplayMetricsChanged(const Display& display,
                                              uint32_t changed_metrics) {}

void DisplayObserver::OnCurrentWorkspaceChanged(
    const std::string& new_workspace) {}

void DisplayObserver::OnDisplayTabletStateChanged(TabletState state) {}

}  // namespace display
