// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_FUCHSIA_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_FUCHSIA_H_

#include "base/component_export.h"
#include "base/process/process_handle.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"

namespace ui {

struct AXTreeSelector;

class COMPONENT_EXPORT(AX_PLATFORM) AXEventRecorderFuchsia
    : public AXEventRecorder {
 public:
  AXEventRecorderFuchsia(base::ProcessId pid, const AXTreeSelector& selector);

  AXEventRecorderFuchsia(const AXEventRecorderFuchsia&) = delete;
  AXEventRecorderFuchsia& operator=(const AXEventRecorderFuchsia&) = delete;

  ~AXEventRecorderFuchsia() override;

  bool instantiated_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_FUCHSIA_H_
