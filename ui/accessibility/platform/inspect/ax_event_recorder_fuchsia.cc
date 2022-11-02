// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_event_recorder_fuchsia.h"

#include <ostream>

#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace ui {

// static
AXEventRecorderFuchsia* AXEventRecorderFuchsia::instance_ = nullptr;

AXEventRecorderFuchsia::AXEventRecorderFuchsia(base::ProcessId pid,
                                               const AXTreeSelector& selector) {
  CHECK(!instance_) << "There can be only one instance of"
                    << " AccessibilityEventRecorder at a time.";
  instance_ = this;
}

AXEventRecorderFuchsia::~AXEventRecorderFuchsia() {
  instance_ = nullptr;
}

}  // namespace ui
