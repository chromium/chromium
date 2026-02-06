// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_DISPLAY_LINK_MAC_H_
#define UI_DISPLAY_MAC_DISPLAY_LINK_MAC_H_

#include "base/apple/scoped_typeref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ui/display/display_export.h"

namespace gpu {
class ImageTransportSurfaceOverlayMacTest;
}

namespace ui {

// VSync parameters parsed from CVDisplayLinkOutputCallback's parameters.
struct DISPLAY_EXPORT VSyncParamsMac {
  // The time of the callback.
  bool callback_times_valid = false;
  base::TimeTicks callback_timebase;
  base::TimeDelta callback_interval;

  // The indicated display time.
  bool display_times_valid = false;
  base::TimeTicks display_timebase;
  base::TimeDelta display_interval;

  int64_t drawable_id;
};

// Object used to control the lifetime of callbacks from DisplayLinkMac.
// See notes in DisplayLinkMac::RegisterCallback
class DISPLAY_EXPORT VSyncCallbackMac {
 public:
  using Callback = base::RepeatingCallback<void(VSyncParamsMac)>;
  ~VSyncCallbackMac();

  // To prevent constantly switching VSync on and off, allow this max number of
  // extra CVDisplayLink VSync running before stopping CVDisplayLink.
  static constexpr int kMaxExtraVSyncs = 12;

  base::WeakPtr<VSyncCallbackMac> GetWeakPtr();

 private:
  friend class CADisplayLinkMac;
  friend class CAMetalDisplayLinkMac;
  friend class CVDisplayLinkMac;
  friend class DisplayLinkMacSharedState;
  friend class ExternalDisplayLinkMac;
  friend struct MetalObjCState;
  friend struct ObjCState;

  friend class gpu::ImageTransportSurfaceOverlayMacTest;

  using UnregisterCallback = base::OnceCallback<void(VSyncCallbackMac*)>;

  explicit VSyncCallbackMac(UnregisterCallback unregister_callback,
                            Callback callback,
                            bool do_callback_on_ctor_thread);

  // The callback to unregister `this` with its DisplayLinkMac.
  UnregisterCallback unregister_callback_;

  Callback callback_for_displaylink_thread_;

  base::WeakPtrFactory<VSyncCallbackMac> weak_factory_{this};
};

class DISPLAY_EXPORT PresentationCallbackMac {
 public:
  using Callback = base::RepeatingCallback<void(int64_t, base::TimeTicks)>;
  ~PresentationCallbackMac();

  base::WeakPtr<PresentationCallbackMac> GetWeakPtr();

 private:
  friend class CAMetalDisplayLinkMac;
  friend struct MetalObjCState;

  using UnregisterCallback = base::OnceCallback<void(PresentationCallbackMac*)>;

  explicit PresentationCallbackMac(UnregisterCallback unregister_callback,
                                   Callback callback,
                                   bool do_callback_on_ctor_thread);

  // The callback to unregister `this` with its DisplayLinkMac.
  UnregisterCallback unregister_callback_;

  Callback callback_for_displaylink_thread_;

  base::WeakPtrFactory<PresentationCallbackMac> weak_factory_{this};
};

class DISPLAY_EXPORT DisplayLinkMac : public base::RefCounted<DisplayLinkMac> {
 public:
  // Create a DisplayLinkMac for the specified display. The returned object may
  // only be accessed on the thread on which it was retrieved.
  static scoped_refptr<DisplayLinkMac> GetForDisplay(int64_t display_id);

  static bool SupportsDisplayLinkMacInBrowser();

  // For CADisplayLink and CVDisplayLink in GPU, always return true;
  // For ExternalDisplayLinkMac, check whether the display id has been added in
  // AddSupportedDisplayLinkId().
  static bool IsDisplayLinkAllowed(int64_t display_id);

  // Register an observer callback.
  // * The specified callback will be called at every VSync tick, until the
  //   returned VSyncCallbackMac object is destroyed.
  // * The resulting VSyncCallbackMac object must be destroyed on the same
  //   thread on which it was created.
  // * The callback is guaranteed to be made on the register thread.
  virtual std::unique_ptr<VSyncCallbackMac> RegisterCallback(
      VSyncCallbackMac::Callback callback) = 0;

  virtual std::unique_ptr<PresentationCallbackMac> RegisterPresentationCallback(
      PresentationCallbackMac::Callback callback);

  // Get the panel/monitor refresh interval
  virtual base::TimeDelta GetRefreshInterval() const = 0;
  virtual void GetRefreshIntervalRange(base::TimeDelta& min_interval,
                                       base::TimeDelta& max_interval,
                                       base::TimeDelta& granularity) const = 0;

  virtual void SetPreferredInterval(base::TimeDelta interval) = 0;

  // Retrieves the current (“now”) time of a given display link. Returns
  // base::TimeTicks() if the current time is not available.
  virtual base::TimeTicks GetCurrentTime() const = 0;

 protected:
  friend class base::RefCounted<DisplayLinkMac>;
  friend class CVDisplayLinkMac;
  friend class CADisplayLinkMac;
  friend class CAMetalDisplayLinkMac;

  virtual ~DisplayLinkMac() = default;

  // This histogram tracks the creation status of CVDisplayLink or
  // CADisplayLink. ExternalDisplayLinkMac is excluded due to potential IPC
  // delays; for instance, ExternalBeginFrameSourceMac::SetVSyncDisplayID()
  // might be triggered before Viz receives the display addition IPC.
  static void RecordDisplayLinkCreation(bool success);
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_DISPLAY_LINK_MAC_H_
