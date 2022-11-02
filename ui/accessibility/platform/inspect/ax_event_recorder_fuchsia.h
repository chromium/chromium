// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_FUCHSIA_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_FUCHSIA_H_

#include "base/process/process_handle.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"

namespace ui {

struct AXTreeSelector;

class AX_EXPORT AXEventRecorderFuchsia : public AXEventRecorder {
 public:
  AXEventRecorderFuchsia(base::ProcessId pid, const AXTreeSelector& selector);

  AXEventRecorderFuchsia(const AXEventRecorderFuchsia&) = delete;
  AXEventRecorderFuchsia& operator=(const AXEventRecorderFuchsia&) = delete;

  ~AXEventRecorderFuchsia() override;

 private:
  static AXEventRecorderFuchsia* instance_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_FUCHSIA_H_
