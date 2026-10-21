// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_
#define UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_

#import <CoreGraphics/CGDirectDisplay.h>

#include <atomic>
#include <memory>

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
  // Returns true if an NSApplication instance is initialized in the current
  // process (e.g. in the browser process).
  static bool NSAppInitialized();

  WindowServerEventPumpThread(
      base::RepeatingCallback<void(int64_t)> update_vsync_displays_cb);
  ~WindowServerEventPumpThread() override;

  // Outcome of pumping WindowServer events on this thread. Recorded in the
  // "Viz.DisplayLink.WindowServerEventPumpResult" histogram: kSuccess is
  // recorded once, after the first successful drain of the event queue, and a
  // failure value is recorded once when the thread stops pumping.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(WindowServerEventPumpResult)
  enum class Result {
    // The WindowServer event queue was drained successfully at least once.
    kSuccess = 0,

    // CGSMainConnectionID() returned an invalid connection.
    kInvalidMainConnection = 1,

    // CGSGetEventPort() failed or returned MACH_PORT_NULL.
    kGetEventPortFailed = 2,

    // mach_msg() failed to receive from the WindowServer event port.
    kMachMsgReceiveFailed = 3,

    kMaxValue = kMachMsgReceiveFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/gpu/enums.xml:WindowServerEventPumpResult)

 private:
  void PumpWindowServerEvents();

  // Called when this thread can no longer pump WindowServer events. Records
  // `failure` to UMA, disables CADisplayLink creation for the rest of the
  // process lifetime and makes all root frame sinks fall back to a timer-based
  // VSync source. Safe to call from any thread; the work is bounced to the Viz
  // thread.
  void DisableDisplayLink(Result failure);

  // Callback to FrameSinkManagerImpl::UpdateVSyncDisplays(), used to make
  // every root frame sink recreate its DisplayLinkMac. The display ID argument
  // is ignored when the GPU process owns the display link, so 0 is passed to
  // update all displays.
  base::RepeatingCallback<void(int64_t)> update_vsync_displays_cb_;

  std::atomic<bool> shutdown_{false};

  // Task runner of the Viz thread on which this object was created and on
  // which `update_vsync_displays_cb_` must run.
  scoped_refptr<base::SingleThreadTaskRunner> viz_task_runner_;

  // Cached in the constructor so that it can be used from the pump thread
  // without touching `weak_factory_`, which is not thread-safe.
  base::WeakPtr<WindowServerEventPumpThread> weak_ptr_;
  base::WeakPtrFactory<WindowServerEventPumpThread> weak_factory_{this};
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
                                           bool success);

  const CGDirectDisplayID display_id_;
  std::unique_ptr<ObjCState> objc_state_;

  base::WeakPtr<VSyncCallbackMac> vsync_callback_;

  base::WeakPtrFactory<CADisplayLinkMac> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_CA_DISPLAY_LINK_MAC_H_
