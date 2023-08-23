// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_DISPLAY_LINK_MAC_H_
#define UI_DISPLAY_MAC_DISPLAY_LINK_MAC_H_

#include <QuartzCore/CVDisplayLink.h>

#include <memory>
#include <set>
#include <vector>

#include "base/apple/scoped_typeref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ui/display/display_export.h"

namespace ui {

class DisplayLinkMac;
class DisplayLinkMacSharedState;

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
};

// Object used to control the lifetime of callbacks from DisplayLinkMac.
// See notes in DisplayLinkMac::RegisterCallback
class DISPLAY_EXPORT VSyncCallbackMac {
 public:
  using Callback = base::RepeatingCallback<void(VSyncParamsMac)>;
  ~VSyncCallbackMac();

 private:
  friend class DisplayLinkMac;
  friend class DisplayLinkMacSharedState;
  using UnregisterCallback = base::OnceCallback<void(VSyncCallbackMac*)>;

  VSyncCallbackMac(UnregisterCallback unregister_callback,
                   Callback callback,
                   bool do_callback_on_ctor_thread);

  // The callback to unregister `this` with its DisplayLinkMac.
  UnregisterCallback unregister_callback_;

  // The callback that will be run on the CVDisplayLink thread. If `this` was
  // created with `do_callback_on_ctor_thread`, then this callback will post a
  // task to the creating thread,
  Callback callback_for_cvdisplaylink_thread_;

  base::WeakPtrFactory<VSyncCallbackMac> weak_factory_;
};

// DisplayLinkMac indirectly owns a CVDisplayLink (via
// DisplayLinkMacSharedState), and may be used to create VSync callbacks.
class DISPLAY_EXPORT DisplayLinkMac : public base::RefCounted<DisplayLinkMac> {
 public:
  // Create a DisplayLinkMac for the specified display.
  static scoped_refptr<DisplayLinkMac> GetForDisplay(
      CGDirectDisplayID display_id);

  // Register an observer callback.
  // * The specified callback will be called at every VSync tick, until the
  //   returned VSyncCallbackMac object is destroyed.
  // * The resulting VSyncCallbackMac object must be destroyed on the same
  //   thread on which it was created.
  // * If `do_callback_on_register_thread` is true, then the callback is
  //   guaranteed to be made on the calling thread and is guaranteed to be made
  //   only if the resulting VSyncCallbackMac has not been destroyed.
  // * If `do_callback_on_register_thread` is false then the callback may come
  //   from any thread, and may happen after the resulting VSyncCallbackMac is
  //   destroyed.
  std::unique_ptr<VSyncCallbackMac> RegisterCallback(
      VSyncCallbackMac::Callback callback,
      bool do_callback_on_register_thread = true);

  // Get the panel/monitor refresh rate
  double GetRefreshRate() const;

  // Retrieves the current (“now”) time of a given display link. Returns
  // base::TimeTicks() if the current time is not available.
  base::TimeTicks GetCurrentTime() const;

 private:
  friend class base::RefCounted<DisplayLinkMac>;

  DisplayLinkMac(DisplayLinkMacSharedState* shared_state);
  virtual ~DisplayLinkMac();

  // This is called by VSyncCallbackMac's destructor.
  void UnregisterCallback(VSyncCallbackMac* callback);

  // A single DisplayLinkMacSharedState is shared between all DisplayLinkMac
  // instances that have same display ID. This is manually retained and
  // released.
  raw_ptr<DisplayLinkMacSharedState> shared_state_;
};

}  // namespace ui

#endif  // UI_DISPLAY_MAC_DISPLAY_LINK_MAC_H_
