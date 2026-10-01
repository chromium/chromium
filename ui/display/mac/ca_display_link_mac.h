// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_
#define UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_

#import <CoreGraphics/CGDirectDisplay.h>

#include "base/functional/callback.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "ui/display/mac/display_link_mac.h"

namespace ui {
struct ObjCState;

// Workaround: In child processes (such as the GPU process) that lack a standard
// AppKit event loop, WindowServer events are not pumped by NSApplication.
// WindowServerEventPumpThread manually drains the connection port so that
// pending display reconfiguration or wake events are processed and reach
// CADisplayLink. Without this workaround:
// (1) CADisplayLink fails to reconfigure and runs at the wrong frame rate
//     after display settings change.
// (2) CADisplayLink fails to send out VSync callbacks after power resume,
//     leading to frozen browser windows.
class DISPLAY_EXPORT WindowServerEventPumpThread : public base::Thread {
 public:
  WindowServerEventPumpThread();
  ~WindowServerEventPumpThread() override;

  // To be implemented.
 private:
  scoped_refptr<base::SingleThreadTaskRunner> viz_task_runner_;
};

class DISPLAY_EXPORT CADisplayLinkMac : public DisplayLinkMac {
 public:
  // Create a CADisplayLinkMac for the specified display.
  static scoped_refptr<DisplayLinkMac> GetForDisplay(
      CGDirectDisplayID display_id,
      bool in_gpu_process);

  // DisplayLinkMac implementation
  std::unique_ptr<VSyncCallbackMac> RegisterCallback(
      VSyncCallbackMac::Callback callback) override;

  base::TimeDelta GetRefreshInterval() const override;
  void GetRefreshIntervalRange(base::TimeDelta& min_interval,
                               base::TimeDelta& max_interval,
                               base::TimeDelta& granularity) const override;

  void SetPreferredInterval(base::TimeDelta interval) override {}

  base::TimeTicks GetCurrentTime() const override;

 private:
  explicit CADisplayLinkMac(CGDirectDisplayID display_id);
  ~CADisplayLinkMac() override;

  CADisplayLinkMac(const CADisplayLinkMac&) = delete;
  CADisplayLinkMac& operator=(const CADisplayLinkMac&) = delete;

  // This is called by VSyncCallbackMac's destructor.
  void UnregisterCallback(VSyncCallbackMac* callback);

  // CADisplayLink callback from ObjCState.display_link.
  void Step();

  // Ensures that the Viz.ExternalBeginFrameSourceMac.DisplayLink.Create2
  // histogram is recorded only once per display within
  // CADisplayLinkMac::GetForDisplay().
  static void TryRecordDisplayLinkCreation(CGDirectDisplayID display_id,
                                           bool success,
                                           bool in_gpu_process);

  const CGDirectDisplayID display_id_;
  std::unique_ptr<ObjCState> objc_state_;

  base::WeakPtr<VSyncCallbackMac> vsync_callback_;

  base::WeakPtrFactory<CADisplayLinkMac> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_
