// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_VSYNC_PROVIDER_MAC_H_
#define UI_DISPLAY_MAC_VSYNC_PROVIDER_MAC_H_

#include <CoreGraphics/CGDirectDisplay.h>

#include <list>
#include <map>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ui/display/display_export.h"
#include "ui/display/mac/display_link_mac.h"

namespace ui {
using NeedsBeginFrameCB = base::RepeatingCallback<void(int64_t, bool)>;

// A VSync provider that provides VSync, which originates in the browser
// process, to ExternalDisplayLinkMac in the GPU process.
// ExternalBeginFrameSourceMojoMac forwards these IPC VSync signals to
// VSyncProviderMac. Only one VSyncProviderMac instance is created to handle all
// requests in both the VizCompositorThread and the GpuMain thread.

class DISPLAY_EXPORT VSyncProviderMac {
 public:
  static VSyncProviderMac* GetInstance();

  VSyncProviderMac(const VSyncProviderMac&) = delete;
  VSyncProviderMac& operator=(const VSyncProviderMac&) = delete;

  // Originated from the browser process
  void OnVSync(const VSyncParamsMac& params, int64_t vsync_display_id);

  void RegisterCallback(VSyncCallbackMac::Callback callback,
                        CGDirectDisplayID display_id);
  void UnregisterCallback(VSyncCallbackMac::Callback callback,
                          CGDirectDisplayID display_id);

  void SetSupportedDisplayLinkId(int64_t vsync_display_id, bool is_supported);

  // Returns the vsync interval via the Vsync provider.
  void SetCallbackForRemoteNeedsBeginFrame(NeedsBeginFrameCB callback);

  // Whether CADisplayLink in Browser with this display_id is supported.
  // The status is updated by SetSupportedDisplayLinkId().
  bool IsDisplayLinkInBrowserValid(int64_t vsync_display_id);

  // Whether the task runner of VSyncProviderMac belongs to the current thread.
  bool BelongsToCurrentThread();

  // Returns true if the provider is connected to the browser (i.e.,
  // `needs_begin_frame_repeating_cb_` is valid) and is running on the Viz
  // thread. Used only to gate recording the ExternalDisplayLink creation
  // histogram.
  bool IsConnectedToBrowserOnVizThread();

  void OnSuspend();

 private:
  friend class base::NoDestructor<VSyncProviderMac>;

  struct DisplayState {
    DisplayState();
    ~DisplayState();
    DisplayState(DisplayState&& other);
    DisplayState& operator=(DisplayState&& other);

    std::list<VSyncCallbackMac::Callback> callbacks;

    // The time when a NeedsBeginFrames(true) request was sent to the browser.
    // This is used to record the latency between VSync requested and received.
    base::TimeTicks begin_frame_request_time;
  };

  VSyncProviderMac();
  virtual ~VSyncProviderMac();

  void AddSupportedDisplayLinkId(CGDirectDisplayID display_id);
  void RemoveSupportedDisplayLinkId(CGDirectDisplayID display_id);

  // Records the time elapsed between sending the NeedsBeginFrames request via
  // IPC and receiving the first VSync signal for the specified display.
  void RecordTimeFromNeedsBeginFramesToVSync(
      base::TimeTicks begin_frame_request_time);

  // Must only be accessed on the Viz thread.
  NeedsBeginFrameCB needs_begin_frame_repeating_cb_;

  // Protects `display_states_` when it is updated on the Viz thread and read
  // concurrently from other threads (such as `CrGpuMain` or
  // `CompositorGpuThread`). Lock acquisition is bypassed when accessing
  // `display_states_` directly on the Viz thread.
  base::Lock id_lock_;
  std::map<CGDirectDisplayID, DisplayState> display_states_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  SEQUENCE_CHECKER(vsync_sequence_checker_);
  base::WeakPtrFactory<VSyncProviderMac> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_VSYNC_PROVIDER_MAC_H_
