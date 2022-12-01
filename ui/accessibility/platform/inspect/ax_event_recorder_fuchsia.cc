// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_event_recorder_fuchsia.h"

#include <ostream>

#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace ui {

AXEventRecorderFuchsia::AXEventRecorderFuchsia(base::ProcessId pid,
                                               const AXTreeSelector& selector) {
  CHECK(!instantiated_) << "There can be only one instance of"
                        << " AccessibilityEventRecorder at a time.";
}

AXEventRecorderFuchsia::~AXEventRecorderFuchsia() = default;

}  // namespace ui
