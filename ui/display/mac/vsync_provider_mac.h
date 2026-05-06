// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_VSYNC_PROVIDER_MAC_H_
#define UI_DISPLAY_MAC_VSYNC_PROVIDER_MAC_H_

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
  void OnVSync(const VSyncParamsMac& params, int64_t display_id);

  void RegisterCallback(VSyncCallbackMac::Callback callback,
                        int64_t display_id);
  void UnregisterCallback(VSyncCallbackMac::Callback callback,
                          int64_t display_id);

  void SetSupportedDisplayLinkId(int64_t display_id, bool is_supported);

  // Returns the vsync interval via the Vsync provider.
  void SetCallbackForRemoteNeedsBeginFrame(NeedsBeginFrameCB callback);

  // Whether CADisplayLink in Browser with this display_id is supported.
  bool IsDisplayLinkSupported(int64_t display_id);

  // Whether the task runner of VSyncProviderMac belongs to the current thread.
  bool BelongsToCurrentThread();

 private:
  friend class base::NoDestructor<VSyncProviderMac>;

  VSyncProviderMac();
  virtual ~VSyncProviderMac();

  void AddSupportedDisplayLinkId(int64_t display_id);
  void RemoveSupportedDisplayLinkId(int64_t display_id);

  NeedsBeginFrameCB needs_begin_frame_callback_;

  // Updated on Viz thread and read back on both Viz and gpu main thread.
  // Use this lock when it's written on the Viz thread and read back on the gpu
  // main thread. No need to lock when read on Viz thread.
  base::Lock id_lock_;
  std::map<int64_t, std::list<VSyncCallbackMac::Callback>> callback_lists_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  SEQUENCE_CHECKER(vsync_sequence_checker_);
  base::WeakPtrFactory<VSyncProviderMac> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_VSYNC_PROVIDER_MAC_H_
