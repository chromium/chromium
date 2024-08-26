// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_WIN_H_

#include <oleacc.h>

#include "base/component_export.h"
#include "base/process/process_handle.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) AXEventRecorderWin
    : public AXEventRecorder {
 public:
  // Flag values that specify the way events are handled.
  enum ListenerType {
    kSync,   // Events are handled synchronously.
    kAsync,  // Events are handled asynchronously.
  };

  AXEventRecorderWin(base::ProcessId pid,
                     const AXTreeSelector& selector,
                     ListenerType listenerType = kSync);

  AXEventRecorderWin(const AXEventRecorderWin&) = delete;
  AXEventRecorderWin& operator=(const AXEventRecorderWin&) = delete;

  ~AXEventRecorderWin() override;

  // Callback registered by SetWinEventHook. Just calls OnWinEventHook.
  static CALLBACK void WinEventHookThunk(HWINEVENTHOOK handle,
                                         DWORD event,
                                         HWND hwnd,
                                         LONG obj_id,
                                         LONG child_id,
                                         DWORD event_thread,
                                         DWORD event_time);

 private:
  // Called by the thunk registered by SetWinEventHook. Retrieves accessibility
  // info about the node the event was fired on and appends a string to
  // the event log.
  void OnWinEventHook(HWINEVENTHOOK handle,
                      DWORD event,
                      HWND hwnd,
                      LONG obj_id,
                      LONG child_id,
                      DWORD event_thread,
                      DWORD event_time);

  HWINEVENTHOOK win_event_hook_handle_;
  static AXEventRecorderWin* instance_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_WIN_H_
