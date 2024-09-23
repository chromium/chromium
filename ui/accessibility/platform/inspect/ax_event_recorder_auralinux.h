// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_AURALINUX_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_AURALINUX_H_

#include <atk/atk.h>
#include <atspi/atspi.h>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace ui {
class AXPlatformTreeManager;

// This class has two distinct event recording code paths. When we are
// recording events in-process (typically this is used for
// DumpAccessibilityEvents tests), we use ATK's global event handlers. Since
// ATK doesn't support intercepting events from other processes, if we have a
// non-zero PID or an accessibility application name pattern, we use AT-SPI2
// directly to intercept events.
// TODO(crbug.com/40722484) AT-SPI2 should be capable of intercepting events
// in-process as well, thus it should be possible to remove the ATK code path
// entirely.
class COMPONENT_EXPORT(AX_PLATFORM) AXEventRecorderAuraLinux
    : public AXEventRecorder {
 public:
  AXEventRecorderAuraLinux(base::WeakPtr<AXPlatformTreeManager> manager,
                           base::ProcessId pid,
                           const AXTreeSelector& selector);

  AXEventRecorderAuraLinux(const AXEventRecorderAuraLinux&) = delete;
  AXEventRecorderAuraLinux& operator=(const AXEventRecorderAuraLinux&) = delete;

  ~AXEventRecorderAuraLinux() override;

  void ProcessATKEvent(const char* event,
                       unsigned int n_params,
                       const GValue* params);
  void ProcessATSPIEvent(const AtspiEvent* event);

  static gboolean OnATKEventReceived(GSignalInvocationHint* hint,
                                     unsigned int n_params,
                                     const GValue* params,
                                     gpointer data);

 private:
  bool ShouldUseATSPI();

  std::string AtkObjectToString(AtkObject* obj, bool include_name);
  void AddATKEventListener(const char* event_name);
  void AddATKEventListeners();
  void RemoveATKEventListeners();
  bool IncludeState(AtkStateType state_type);

  void AddATSPIEventListeners();
  void RemoveATSPIEventListeners();

  raw_ptr<AtspiEventListener> atspi_event_listener_ = nullptr;

  base::WeakPtr<AXPlatformTreeManager> manager_;
  base::ProcessId pid_;
  AXTreeSelector selector_;
  static AXEventRecorderAuraLinux* instance_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_AURALINUX_H_
